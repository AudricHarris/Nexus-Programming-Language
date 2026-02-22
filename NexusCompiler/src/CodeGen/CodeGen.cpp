#include "CodeGen.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include <regex>
#include <string>

CodeGenerator::CodeGenerator()
    : module(std::make_unique<llvm::Module>("nexus", context)),
      builder(context) {
  module->setTargetTriple(llvm::Triple(LLVM_HOST_TRIPLE));
}

llvm::Value *CodeGenerator::logErrorV(const char *msg) {
  llvm::errs() << "Codegen error: " << msg << "\n";
  return nullptr;
}

static std::string normaliseFunctionName(const std::string &name) {
  if (name == "Main")
    return "main";
  if (name == "Printf")
    return "printf";
  return name;
}

llvm::Type *CodeGenerator::getLLVMType(const Identifier &typeId) {
  std::string t = typeId.token.getWord();

  if (t == "i32" || t == "int" || t == "integer")
    return llvm::Type::getInt32Ty(context);
  if (t == "i64" || t == "long")
    return llvm::Type::getInt64Ty(context);
  if (t == "void")
    return llvm::Type::getVoidTy(context);
  if (t == "bool")
    return llvm::Type::getInt1Ty(context);
  if (t == "f32" || t == "float")
    return llvm::Type::getFloatTy(context);
  if (t == "f64" || t == "double")
    return llvm::Type::getDoubleTy(context);

  logErrorV(("Unknown type: " + t).c_str());
  return nullptr;
}

static std::string
expandInterpolation(const std::string &raw,
                    const std::map<std::string, VarInfo> &namedValues,
                    std::vector<std::string> &outVarNames) // names in order
{
  std::string result;
  std::regex re(R"(\{(\w+)\})");
  auto it = std::sregex_iterator(raw.begin(), raw.end(), re);
  auto end = std::sregex_iterator();
  size_t last = 0;

  for (; it != end; ++it) {
    auto &m = *it;
    result += raw.substr(last, m.position() - last);

    std::string varName = m[1].str();
    outVarNames.push_back(varName);

    auto vi = namedValues.find(varName);
    if (vi != namedValues.end() && vi->second.type) {
      llvm::Type *ty = vi->second.type;
      if (ty->isIntegerTy(64))
        result += "%lld";
      else if (ty->isIntegerTy())
        result += "%d";
      else if (ty->isFloatTy())
        result += "%f";
      else if (ty->isDoubleTy())
        result += "%lf";
      else
        result += "%p";
    } else {
      result += "%d";
    }

    last = m.position() + m.length();
  }
  result += raw.substr(last);
  return result;
}

llvm::Value *CodeGenerator::codegen(const Expression &expr) {

  if (auto *ie = dynamic_cast<const IdentExpr *>(&expr)) {
    auto it = namedValues.find(ie->name.token.getWord());
    if (it == namedValues.end())
      return logErrorV("Unknown variable");
    return builder.CreateLoad(it->second.type, it->second.alloca,
                              ie->name.token.getWord() + "_load");
  }

  if (auto *ile = dynamic_cast<const IntLitExpr *>(&expr)) {
    return llvm::ConstantInt::get(llvm::Type::getInt32Ty(context),
                                  std::stoll(ile->lit.token.getWord()));
  }

  if (auto *sle = dynamic_cast<const StrLitExpr *>(&expr)) {
    return builder.CreateGlobalString(sle->lit.token.getWord());
  }

  if (auto *ce = dynamic_cast<const CallExpr *>(&expr)) {
    std::string calleeName = normaliseFunctionName(ce->callee.token.getWord());

    if (calleeName == "printf" && ce->arguments.size() == 1) {
      if (auto *strArg =
              dynamic_cast<const StrLitExpr *>(ce->arguments[0].get())) {
        std::vector<std::string> varNames;
        std::string fmt = expandInterpolation(strArg->lit.token.getWord(),
                                              namedValues, varNames);

        if (fmt.empty() || fmt.back() != '\n')
          fmt += '\n';

        fmt = CodeGenerator::replaceHexColors(fmt);
        fmt += "\033[0m";

        llvm::Value *fmtPtr = builder.CreateGlobalString(fmt, ".fmt");

        llvm::Function *printfF = module->getFunction("printf");
        if (!printfF)
          return logErrorV("printf not declared");

        std::vector<llvm::Value *> argsV = {fmtPtr};
        for (auto &vn : varNames) {
          auto vi = namedValues.find(vn);
          if (vi == namedValues.end())
            return logErrorV(
                ("Undefined variable in format string: " + vn).c_str());
          argsV.push_back(builder.CreateLoad(vi->second.type, vi->second.alloca,
                                             vn + "_load"));
        }

        return builder.CreateCall(printfF, argsV, "printf_ret");
      }
    }

    llvm::Function *calleeF = module->getFunction(calleeName);
    if (!calleeF)
      return logErrorV(("Unknown function: " + calleeName).c_str());

    if (!calleeF->isVarArg() && calleeF->arg_size() != ce->arguments.size())
      return logErrorV("Incorrect # arguments");

    std::vector<llvm::Value *> argsV;
    for (auto &arg : ce->arguments) {
      auto *v = codegen(*arg);
      if (!v)
        return nullptr;
      argsV.push_back(v);
    }

    return builder.CreateCall(calleeF, argsV, "calltmp");
  }

  if (auto *ae = dynamic_cast<const Assignment *>(&expr)) {
    std::string targetName = ae->target.token.getWord();
    auto it = namedValues.find(targetName);
    if (it == namedValues.end())
      return logErrorV("Cannot assign to unknown variable");

    llvm::Value *val = codegen(*ae->value);
    if (!val)
      return nullptr;

    builder.CreateStore(val, it->second.alloca);
    return val;
  }

  if (auto *inc = dynamic_cast<const Increment *>(&expr)) {
    auto it = namedValues.find(inc->target.token.getWord());
    if (it == namedValues.end())
      return logErrorV("Unknown variable in ++");

    llvm::AllocaInst *alloca = it->second.alloca;
    llvm::Type *valType = it->second.type;

    auto *cur = builder.CreateLoad(valType, alloca, "load_inc");
    auto *one = llvm::ConstantInt::get(valType, 1);
    auto *add = builder.CreateAdd(cur, one, "inctmp");
    builder.CreateStore(add, alloca);
    return add;
  }

  return logErrorV("Unknown expression type");
}

llvm::Value *CodeGenerator::codegen(const Statement &stmt) {

  if (auto *vd = dynamic_cast<const VarDecl *>(&stmt)) {
    std::string varName = vd->name.token.getWord();

    llvm::Type *ty = getLLVMType(vd->type);
    if (!ty)
      return logErrorV("Unknown type in var decl");

    llvm::AllocaInst *alloc = builder.CreateAlloca(ty, nullptr, varName);

    if (vd->initializer) {
      auto *initVal = codegen(*vd->initializer);
      if (!initVal)
        return nullptr;
      builder.CreateStore(initVal, alloc);
    }

    namedValues[varName] = {alloc, ty};
    return alloc;
  }

  if (auto *es = dynamic_cast<const ExprStmt *>(&stmt)) {
    return codegen(*es->expr);
  }

  if (auto *ret = dynamic_cast<const Return *>(&stmt)) {
    if (ret->value) {
      auto *v = codegen(**ret->value);
      if (!v)
        return nullptr;
      builder.CreateRet(v);
    } else {
      builder.CreateRetVoid();
    }
    return nullptr;
  }

  return logErrorV("Unknown statement type");
}

void CodeGenerator::codegen(const Block &block) {
  for (const auto &stmt : block.statements) {
    if (builder.GetInsertBlock()->getTerminator())
      break;
    codegen(*stmt);
  }
}

llvm::Function *CodeGenerator::codegen(const Function &func) {
  std::string fname = normaliseFunctionName(func.name.token.getWord());

  llvm::Type *retTy = llvm::Type::getInt32Ty(context);

  std::vector<llvm::Type *> paramTypes;
  for (const auto &p : func.params) {
    llvm::Type *t = getLLVMType(p.type);
    if (!t)
      return nullptr;
    paramTypes.push_back(t);
  }

  llvm::Function *f = module->getFunction(fname);
  if (!f) {
    auto *ft = llvm::FunctionType::get(retTy, paramTypes, false);
    f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, fname,
                               *module);
  }

  llvm::BasicBlock *bb = llvm::BasicBlock::Create(context, "entry", f);
  builder.SetInsertPoint(bb);

  namedValues.clear();

  size_t idx = 0;
  for (auto &arg : f->args()) {
    const auto &param = func.params[idx++];
    std::string name = param.name.token.getWord();

    llvm::AllocaInst *alloc =
        builder.CreateAlloca(arg.getType(), nullptr, name);
    builder.CreateStore(&arg, alloc);
    namedValues[name] = {alloc, arg.getType()};
  }

  codegen(*func.body);

  if (!builder.GetInsertBlock()->getTerminator()) {
    if (retTy->isVoidTy())
      builder.CreateRetVoid();
    else
      builder.CreateRet(llvm::ConstantInt::get(retTy, 0));
  }

  if (llvm::verifyFunction(*f, &llvm::errs())) {
    f->eraseFromParent();
    return nullptr;
  }

  return f;
}

bool CodeGenerator::generate(const Program &program,
                             const std::string &outputFilename) {
  namedValues.clear();

  {
    auto *i8PtrTy = llvm::PointerType::get(context, 0);
    auto *printfTy = llvm::FunctionType::get(llvm::Type::getInt32Ty(context),
                                             {i8PtrTy}, /*vararg=*/true);
    llvm::Function::Create(printfTy, llvm::Function::ExternalLinkage, "printf",
                           *module);
  }

  for (const auto &f : program.functions) {
    if (!codegen(*f))
      return false;
  }

  module->print(llvm::outs(), nullptr);

  std::error_code ec;
  llvm::raw_fd_ostream out(outputFilename + ".ll", ec, llvm::sys::fs::OF_None);
  if (ec) {
    llvm::errs() << "Could not open output: " << ec.message() << "\n";
    return false;
  }
  module->print(out, nullptr);
  return true;
}

std::string CodeGenerator::hexToAnsi(const std::string &hex) {
  if (hex.size() != 7 || hex[0] != '#')
    return hex;

  int r = std::stoi(hex.substr(1, 2), nullptr, 16);
  int g = std::stoi(hex.substr(3, 2), nullptr, 16);
  int b = std::stoi(hex.substr(5, 2), nullptr, 16);

  return "\033[38;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" +
         std::to_string(b) + "m";
}

std::string CodeGenerator::replaceHexColors(const std::string &input) {
  std::regex hexColor(R"(#([0-9a-fA-F]{6}))");
  std::string result;
  std::sregex_iterator begin(input.begin(), input.end(), hexColor);
  std::sregex_iterator end;

  size_t lastPos = 0;

  for (auto i = begin; i != end; ++i) {
    auto match = *i;
    result += input.substr(lastPos, match.position() - lastPos);
    result += hexToAnsi(match.str());
    lastPos = match.position() + match.length();
  }

  result += input.substr(lastPos);
  return result;
}
