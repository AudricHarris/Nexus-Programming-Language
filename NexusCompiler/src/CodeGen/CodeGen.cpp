#include "CodeGen.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include <llvm/IR/Value.h>
#include <regex>
#include <string>

CodeGenerator::CodeGenerator()
    : module(std::make_unique<llvm::Module>("nexus", context)),
      builder(context) {
  module->setTargetTriple(llvm::Triple(LLVM_HOST_TRIPLE));
}

llvm::Value *CodeGenerator::logErrorV(const char *msg) {
  llvm::errs() << "\033[31mCode compiling error: " << msg << "\033[0m\n";
  return nullptr;
}

static std::string normaliseFunctionName(const std::string &name) {
  if (name == "Main")
    return "main";
  if (name == "Printf")
    return "printf";
  if (name == "Print")
    return "print_literal";
  if (name == "printf")
    return "printf";
  return name;
}

static std::string unescapeString(const std::string &s) {
  std::string result;
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '\\' && i + 1 < s.size()) {
      switch (s[i + 1]) {
      case 'n':
        result += '\n';
        ++i;
        break;
      case 't':
        result += '\t';
        ++i;
        break;
      case 'r':
        result += '\r';
        ++i;
        break;
      case '\\':
        result += '\\';
        ++i;
        break;
      case '"':
        result += '"';
        ++i;
        break;
      case '0':
        result += '\0';
        ++i;
        break;
      default:
        result += s[i];
        break;
      }
    } else {
      result += s[i];
    }
  }
  return result;
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
  if (t == "str" || t == "string")
    return llvm::PointerType::get(context, 0);

  logErrorV(("Unknown type: " + t).c_str());
  return nullptr;
}

static std::string fmtSpecForType(llvm::Type *ty) {
  if (!ty)
    return "%d";
  if (ty->isIntegerTy(64))
    return "%lld";
  if (ty->isIntegerTy())
    return "%d";
  if (ty->isFloatTy() || ty->isDoubleTy())
    return "%f";
  return "%s";
}

namespace {

struct MiniToken {
  enum class Kind {
    Int,
    Float,
    Ident,
    Plus,
    Minus,
    Star,
    SlashSlash,
    Slash,
    Percent,
    LParen,
    RParen,
    End,
    Unknown
  };
  Kind kind;
  std::string text;
};

static std::vector<MiniToken> miniTokenise(const std::string &src) {
  std::vector<MiniToken> toks;
  size_t i = 0;
  while (i < src.size()) {
    char c = src[i];
    if (std::isspace(static_cast<unsigned char>(c))) {
      ++i;
      continue;
    }

    if (std::isdigit(static_cast<unsigned char>(c))) {
      std::string num;
      bool isFloat = false;
      while (
          i < src.size() &&
          (std::isdigit(static_cast<unsigned char>(src[i])) || src[i] == '.')) {
        if (src[i] == '.')
          isFloat = true;
        num += src[i++];
      }
      toks.push_back(
          {isFloat ? MiniToken::Kind::Float : MiniToken::Kind::Int, num});
      continue;
    }

    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
      std::string id;
      while (
          i < src.size() &&
          (std::isalnum(static_cast<unsigned char>(src[i])) || src[i] == '_'))
        id += src[i++];
      toks.push_back({MiniToken::Kind::Ident, id});
      continue;
    }

    switch (c) {
    case '+':
      toks.push_back({MiniToken::Kind::Plus, "+"});
      ++i;
      break;
    case '-':
      toks.push_back({MiniToken::Kind::Minus, "-"});
      ++i;
      break;
    case '*':
      toks.push_back({MiniToken::Kind::Star, "*"});
      ++i;
      break;
    case '/':
      if (i + 1 < src.size() && src[i + 1] == '/') {
        toks.push_back({MiniToken::Kind::SlashSlash, "//"});
        i += 2;
      } else {
        toks.push_back({MiniToken::Kind::Slash, "/"});
        ++i;
      }
      break;
    case '%':
      toks.push_back({MiniToken::Kind::Percent, "%"});
      ++i;
      break;
    case '(':
      toks.push_back({MiniToken::Kind::LParen, "("});
      ++i;
      break;
    case ')':
      toks.push_back({MiniToken::Kind::RParen, ")"});
      ++i;
      break;
    default:
      toks.push_back({MiniToken::Kind::Unknown, std::string(1, c)});
      ++i;
      break;
    }
  }
  toks.push_back({MiniToken::Kind::End, ""});
  return toks;
}

struct MiniParser {
  const std::vector<MiniToken> &toks;
  size_t pos = 0;
  llvm::LLVMContext &ctx;
  llvm::IRBuilder<> &builder;
  const std::map<std::string, VarInfo> &namedValues;

  const MiniToken &peek() const { return toks[pos]; }
  MiniToken consume() { return toks[pos++]; }
  bool check(MiniToken::Kind k) const { return toks[pos].kind == k; }

  // entry
  llvm::Value *parseExpr() { return parseAddSub(); }

  llvm::Value *parseAddSub() {
    auto *lhs = parseMulDiv();
    if (!lhs)
      return nullptr;
    while (check(MiniToken::Kind::Plus) || check(MiniToken::Kind::Minus)) {
      auto op = consume();
      auto *rhs = parseMulDiv();
      if (!rhs)
        return nullptr;
      lhs = promoteMatch(lhs, rhs);
      rhs = promoteMatch(rhs, lhs);
      if (lhs->getType()->isFloatingPointTy())
        lhs = (op.kind == MiniToken::Kind::Plus)
                  ? builder.CreateFAdd(lhs, rhs, "fadd")
                  : builder.CreateFSub(lhs, rhs, "fsub");
      else
        lhs = (op.kind == MiniToken::Kind::Plus)
                  ? builder.CreateAdd(lhs, rhs, "add")
                  : builder.CreateSub(lhs, rhs, "sub");
    }
    return lhs;
  }

  llvm::Value *parseMulDiv() {
    auto *lhs = parseUnary();
    if (!lhs)
      return nullptr;
    while (check(MiniToken::Kind::Star) || check(MiniToken::Kind::Slash) ||
           check(MiniToken::Kind::SlashSlash) ||
           check(MiniToken::Kind::Percent)) {
      auto op = consume();
      auto *rhs = parseUnary();
      if (!rhs)
        return nullptr;

      if (op.kind == MiniToken::Kind::SlashSlash) {
        // Floor division — always integer
        lhs = toInt(lhs);
        rhs = toInt(rhs);
        lhs = builder.CreateSDiv(lhs, rhs, "divfloor");
      } else if (op.kind == MiniToken::Kind::Percent) {
        lhs = toInt(lhs);
        rhs = toInt(rhs);
        lhs = builder.CreateSRem(lhs, rhs, "mod");
      } else if (op.kind == MiniToken::Kind::Slash) {
        // True division — promote to double
        lhs = toDouble(lhs);
        rhs = toDouble(rhs);
        lhs = builder.CreateFDiv(lhs, rhs, "divtrue");
      } else {
        // Mul
        lhs = promoteMatch(lhs, rhs);
        rhs = promoteMatch(rhs, lhs);
        if (lhs->getType()->isFloatingPointTy())
          lhs = builder.CreateFMul(lhs, rhs, "fmul");
        else
          lhs = builder.CreateMul(lhs, rhs, "mul");
      }
    }
    return lhs;
  }

  llvm::Value *parseUnary() {
    if (check(MiniToken::Kind::Minus)) {
      consume();
      auto *v = parseAtom();
      if (!v)
        return nullptr;
      if (v->getType()->isFloatingPointTy())
        return builder.CreateFNeg(v, "fneg");
      return builder.CreateNeg(v, "neg");
    }
    return parseAtom();
  }

  llvm::Value *parseAtom() {
    auto tok = consume();
    if (tok.kind == MiniToken::Kind::Int) {
      return llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx),
                                    std::stoll(tok.text));
    }
    if (tok.kind == MiniToken::Kind::Float) {
      return llvm::ConstantFP::get(llvm::Type::getDoubleTy(ctx),
                                   std::stod(tok.text));
    }
    if (tok.kind == MiniToken::Kind::Ident) {
      auto it = namedValues.find(tok.text);
      if (it == namedValues.end())
        return nullptr; // unknown var
      return builder.CreateLoad(it->second.type, it->second.alloca,
                                tok.text + "_load");
    }
    if (tok.kind == MiniToken::Kind::LParen) {
      auto *v = parseExpr();
      // consume RParen
      if (check(MiniToken::Kind::RParen))
        consume();
      return v;
    }
    return nullptr;
  }

  llvm::Value *toDouble(llvm::Value *v) {
    if (v->getType()->isDoubleTy())
      return v;
    if (v->getType()->isFloatTy())
      return builder.CreateFPExt(v, llvm::Type::getDoubleTy(ctx), "f2d");
    if (v->getType()->isIntegerTy())
      return builder.CreateSIToFP(v, llvm::Type::getDoubleTy(ctx), "i2d");
    return v;
  }

  llvm::Value *toInt(llvm::Value *v) {
    if (v->getType()->isFloatingPointTy())
      return builder.CreateFPToSI(v, llvm::Type::getInt32Ty(ctx), "f2i");
    return v;
  }

  // If one operand is float and the other isn't, promote the other
  llvm::Value *promoteMatch(llvm::Value *v, llvm::Value *other) {
    if (other->getType()->isFloatingPointTy() && v->getType()->isIntegerTy())
      return toDouble(v);
    return v;
  }
};

} // namespace

static std::string
expandPrintfString(const std::string &raw, llvm::LLVMContext &ctx,
                   llvm::IRBuilder<> &builder,
                   const std::map<std::string, VarInfo> &namedValues,
                   std::vector<llvm::Value *> &outExprs) {
  std::string result;
  size_t i = 0;

  while (i < raw.size()) {
    if (raw[i] != '{') {
      result += raw[i++];
      continue;
    }

    size_t start = i + 1;
    size_t depth = 1;
    size_t j = start;
    while (j < raw.size() && depth > 0) {
      if (raw[j] == '{')
        ++depth;
      else if (raw[j] == '}')
        --depth;
      if (depth > 0)
        ++j;
    }
    std::string inner = raw.substr(start, j - start);

    auto toks = miniTokenise(inner);
    MiniParser mp{toks, 0, ctx, builder, namedValues};
    llvm::Value *val = mp.parseExpr();

    if (val) {
      result += fmtSpecForType(val->getType());
      outExprs.push_back(val);
    } else {
      result += '{';
      result += inner;
      result += '}';
    }

    i = j + 1;
  }

  return result;
}

llvm::Value *CodeGenerator::codegen(const Expression &expr) {

  if (auto *ie = dynamic_cast<const IdentExpr *>(&expr)) {
    auto it = namedValues.find(ie->name.token.getWord());
    if (it == namedValues.end())
      return logErrorV(
          ("Unknown variable: " + ie->name.token.getWord()).c_str());
    return builder.CreateLoad(it->second.type, it->second.alloca,
                              ie->name.token.getWord() + "_load");
  }

  if (auto *ile = dynamic_cast<const IntLitExpr *>(&expr)) {
    return llvm::ConstantInt::get(llvm::Type::getInt32Ty(context),
                                  std::stoll(ile->lit.token.getWord()));
  }

  if (auto *fle = dynamic_cast<const FloatLitExpr *>(&expr)) {
    return llvm::ConstantFP::get(llvm::Type::getDoubleTy(context),
                                 std::stod(fle->lit.token.getWord()));
  }

  if (auto *sle = dynamic_cast<const StrLitExpr *>(&expr)) {
    return builder.CreateGlobalString(sle->lit.token.getWord());
  }

  if (auto *bin = dynamic_cast<const BinaryExpr *>(&expr)) {
    llvm::Value *left = codegen(*bin->left);
    llvm::Value *right = codegen(*bin->right);
    if (!left || !right)
      return nullptr;

    bool leftFloat = left->getType()->isFloatingPointTy();
    bool rightFloat = right->getType()->isFloatingPointTy();

    // Auto-promote for true division and mixed float/int
    auto toDouble = [&](llvm::Value *v) -> llvm::Value * {
      if (v->getType()->isDoubleTy())
        return v;
      if (v->getType()->isFloatTy())
        return builder.CreateFPExt(v, llvm::Type::getDoubleTy(context), "f2d");
      return builder.CreateSIToFP(v, llvm::Type::getDoubleTy(context), "i2d");
    };

    switch (bin->op) {
    case BinaryOp::Add:
      if (leftFloat || rightFloat) {
        left = toDouble(left);
        right = toDouble(right);
        return builder.CreateFAdd(left, right, "faddtmp");
      }
      return builder.CreateAdd(left, right, "addtmp");

    case BinaryOp::Sub:
      if (leftFloat || rightFloat) {
        left = toDouble(left);
        right = toDouble(right);
        return builder.CreateFSub(left, right, "fsubtmp");
      }
      return builder.CreateSub(left, right, "subtmp");

    case BinaryOp::Mul:
      if (leftFloat || rightFloat) {
        left = toDouble(left);
        right = toDouble(right);
        return builder.CreateFMul(left, right, "fmultmp");
      }
      return builder.CreateMul(left, right, "multmp");

    case BinaryOp::Div:
      left = toDouble(left);
      right = toDouble(right);
      return builder.CreateFDiv(left, right, "divtmp");

    case BinaryOp::DivFloor:
      if (leftFloat)
        left =
            builder.CreateFPToSI(left, llvm::Type::getInt32Ty(context), "f2i");
      if (rightFloat)
        right =
            builder.CreateFPToSI(right, llvm::Type::getInt32Ty(context), "f2i");
      return builder.CreateSDiv(left, right, "divfloortmp");

    case BinaryOp::Mod:
      if (leftFloat)
        left =
            builder.CreateFPToSI(left, llvm::Type::getInt32Ty(context), "f2i");
      if (rightFloat)
        right =
            builder.CreateFPToSI(right, llvm::Type::getInt32Ty(context), "f2i");
      return builder.CreateSRem(left, right, "modtmp");

    case BinaryOp::Lt:
      if (leftFloat || rightFloat) {
        left = toDouble(left);
        right = toDouble(right);
        return builder.CreateFCmpOLT(left, right, "fcmplt");
      }
      return builder.CreateICmpSLT(left, right, "cmplt");
    case BinaryOp::Le:
      if (leftFloat || rightFloat) {
        left = toDouble(left);
        right = toDouble(right);
        return builder.CreateFCmpOLE(left, right, "fcmple");
      }
      return builder.CreateICmpSLE(left, right, "cmple");
    case BinaryOp::Gt:
      if (leftFloat || rightFloat) {
        left = toDouble(left);
        right = toDouble(right);
        return builder.CreateFCmpOGT(left, right, "fcmpgt");
      }
      return builder.CreateICmpSGT(left, right, "cmpgt");
    case BinaryOp::Ge:
      if (leftFloat || rightFloat) {
        left = toDouble(left);
        right = toDouble(right);
        return builder.CreateFCmpOGE(left, right, "fcmpge");
      }
      return builder.CreateICmpSGE(left, right, "cmpge");
    case BinaryOp::Eq:
      if (leftFloat || rightFloat) {
        left = toDouble(left);
        right = toDouble(right);
        return builder.CreateFCmpOEQ(left, right, "fcmpeq");
      }
      return builder.CreateICmpEQ(left, right, "cmpeq");
    case BinaryOp::Ne:
      if (leftFloat || rightFloat) {
        left = toDouble(left);
        right = toDouble(right);
        return builder.CreateFCmpONE(left, right, "fcmpne");
      }
      return builder.CreateICmpNE(left, right, "cmpne");
    }
  }

  if (auto *un = dynamic_cast<const UnaryExpr *>(&expr)) {
    llvm::Value *operand = codegen(*un->operand);
    if (!operand)
      return nullptr;
    switch (un->op) {
    case UnaryOp::Negate:
      if (operand->getType()->isFloatingPointTy())
        return builder.CreateFNeg(operand, "fnegtmp");
      return builder.CreateNeg(operand, "negtmp");
    }
  }

  if (auto *ae = dynamic_cast<const AssignExpr *>(&expr)) {
    std::string targetName = ae->target.token.getWord();
    auto it = namedValues.find(targetName);
    if (it == namedValues.end())
      return logErrorV(
          ("Cannot assign to unknown variable: " + targetName).c_str());

    llvm::Value *val = codegen(*ae->value);
    if (!val)
      return nullptr;

    builder.CreateStore(val, it->second.alloca);
    return val;
  }

  if (auto *inc = dynamic_cast<const Increment *>(&expr)) {
    auto it = namedValues.find(inc->target.token.getWord());
    if (it == namedValues.end())
      return logErrorV(
          ("Unknown variable in ++: " + inc->target.token.getWord()).c_str());

    llvm::AllocaInst *alloca = it->second.alloca;
    llvm::Type *valType = it->second.type;

    auto *cur = builder.CreateLoad(valType, alloca, "load_inc");
    auto *one = llvm::ConstantInt::get(valType, 1);
    auto *add = builder.CreateAdd(cur, one, "inctmp");
    builder.CreateStore(add, alloca);
    return add;
  }

  if (auto *dec = dynamic_cast<const Decrement *>(&expr)) {
    auto it = namedValues.find(dec->target.token.getWord());
    if (it == namedValues.end())
      return logErrorV(
          ("Unknown variable in --: " + dec->target.token.getWord()).c_str());

    llvm::AllocaInst *alloca = it->second.alloca;
    llvm::Type *valType = it->second.type;

    auto *cur = builder.CreateLoad(valType, alloca, "load_dec");
    auto *one = llvm::ConstantInt::get(valType, 1);
    auto *sub = builder.CreateSub(cur, one, "dectmp");
    builder.CreateStore(sub, alloca);
    return sub;
  }

  if (auto *ce = dynamic_cast<const CallExpr *>(&expr)) {
    std::string rawName = ce->callee.token.getWord();
    std::string calleeName = normaliseFunctionName(rawName);

    if ((calleeName == "printf" || rawName == "Printf") &&
        ce->arguments.size() == 1) {
      if (auto *strArg =
              dynamic_cast<const StrLitExpr *>(ce->arguments[0].get())) {
        std::vector<llvm::Value *> interpolatedExprs;
        std::string fmt = expandPrintfString(
            unescapeString(strArg->lit.token.getWord()), context, builder,
            namedValues, interpolatedExprs);

        fmt = CodeGenerator::replaceHexColors(fmt);
        fmt += "\033[0m";

        llvm::Value *fmtPtr = builder.CreateGlobalString(fmt, ".fmt");

        llvm::Function *printfF = module->getFunction("printf");
        if (!printfF)
          return logErrorV("printf not declared");

        std::vector<llvm::Value *> argsV = {fmtPtr};
        for (auto *v : interpolatedExprs) {
          if (v->getType()->isFloatTy())
            v = builder.CreateFPExt(v, llvm::Type::getDoubleTy(context),
                                    "f2d_arg");
          argsV.push_back(v);
        }

        return builder.CreateCall(printfF, argsV, "printf_ret");
      }
    }

    if (rawName == "Print" && ce->arguments.size() == 1) {
      if (auto *strArg =
              dynamic_cast<const StrLitExpr *>(ce->arguments[0].get())) {
        std::string literal = unescapeString(strArg->lit.token.getWord());

        literal = CodeGenerator::replaceHexColors(literal);
        literal += "\033[0m";
        literal += '\n';

        llvm::Value *fmtPtr = builder.CreateGlobalString(literal, ".lit");
        llvm::Value *strArg2 = builder.CreateGlobalString("%s", ".pfmt");

        llvm::Function *printfF = module->getFunction("printf");
        if (!printfF)
          return logErrorV("printf not declared");

        return builder.CreateCall(printfF, {strArg2, fmtPtr}, "print_ret");
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

  return logErrorV("Unknown expression type");
}

llvm::Value *CodeGenerator::codegen(const Statement &stmt) {

  if (auto *vd = dynamic_cast<const VarDecl *>(&stmt)) {
    std::string varName = vd->name.token.getWord();

    llvm::Type *ty = getLLVMType(vd->type);
    if (!ty)
      return logErrorV(("Unknown type in var decl: " + varName).c_str());

    llvm::AllocaInst *alloc = builder.CreateAlloca(ty, nullptr, varName);
    namedValues[varName] = {alloc, ty};

    if (vd->initializer) {
      auto *initVal = codegen(*vd->initializer);
      if (!initVal)
        return logErrorV(
            ("Failed to evaluate initializer for: " + varName).c_str());

      if (ty->isFloatTy() && initVal->getType()->isDoubleTy())
        initVal = builder.CreateFPTrunc(initVal, ty, "d2f");

      builder.CreateStore(initVal, alloc);
    }

    return alloc;
  }

  if (auto *es = dynamic_cast<const ExprStmt *>(&stmt)) {
    return codegen(*es->expr);
  }

  if (auto *ifs = dynamic_cast<const IfStmt *>(&stmt)) {
    llvm::Value *condV = codegen(*ifs->condition);
    if (!condV)
      return nullptr;

    if (!condV->getType()->isIntegerTy(1)) {
      condV = builder.CreateICmpNE(
          condV, llvm::ConstantInt::get(condV->getType(), 0), "ifcond");
    }

    llvm::Function *func = builder.GetInsertBlock()->getParent();

    llvm::BasicBlock *thenBB = llvm::BasicBlock::Create(context, "then", func);
    llvm::BasicBlock *elseBB = llvm::BasicBlock::Create(context, "else");
    llvm::BasicBlock *mergeBB = llvm::BasicBlock::Create(context, "ifcont");

    builder.CreateCondBr(condV, thenBB, elseBB);

    builder.SetInsertPoint(thenBB);
    codegen(*ifs->thenBranch);
    if (!builder.GetInsertBlock()->getTerminator())
      builder.CreateBr(mergeBB);

    func->insert(func->end(), elseBB);
    builder.SetInsertPoint(elseBB);
    if (ifs->elseBranch)
      codegen(*ifs->elseBranch);
    if (!builder.GetInsertBlock()->getTerminator())
      builder.CreateBr(mergeBB);

    func->insert(func->end(), mergeBB);
    builder.SetInsertPoint(mergeBB);
    return nullptr;
  }

  if (auto *loop = dynamic_cast<const WhileStmt *>(&stmt)) {
    llvm::Function *func = builder.GetInsertBlock()->getParent();

    llvm::BasicBlock *condBB =
        llvm::BasicBlock::Create(context, "while.cond", func);
    llvm::BasicBlock *bodyBB = llvm::BasicBlock::Create(context, "while.body");
    llvm::BasicBlock *exitBB = llvm::BasicBlock::Create(context, "while.end");

    builder.CreateBr(condBB);

    builder.SetInsertPoint(condBB);
    llvm::Value *condV = codegen(*loop->condition);
    if (!condV)
      return nullptr;

    if (!condV->getType()->isIntegerTy(1)) {
      condV = builder.CreateICmpNE(
          condV, llvm::ConstantInt::get(condV->getType(), 0), "cond");
    }

    builder.CreateCondBr(condV, bodyBB, exitBB);
    func->insert(func->end(), bodyBB);
    builder.SetInsertPoint(bodyBB);
    codegen(*loop->doBranch);

    if (!builder.GetInsertBlock()->getTerminator())
      builder.CreateBr(condBB);

    func->insert(func->end(), exitBB);
    builder.SetInsertPoint(exitBB);
    return nullptr;
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
