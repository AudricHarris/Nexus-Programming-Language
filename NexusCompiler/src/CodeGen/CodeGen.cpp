#include "CodeGen.h"
#include "Emitters/BuiltinEmitter.h"
#include "Emitters/PrintEmitter.h"
#include "Emitters/StringEmitter.h"
#include "RTDecl.h"
#include "TypeResolver.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include <memory>
#include <string>
#include <unordered_map>

using namespace llvm;

// ------------------------------- //
// Internal utilities (file-local) //
// ------------------------------- //

static std::string normalizeFunctionName(const std::string &name) {
  static const std::unordered_map<std::string, std::string> kMap = {
      {"Main", "main"},     {"Printf", "printf"}, {"Print", "print_literal"},
      {"printf", "printf"}, {"Read", "read"},     {"Random", "random"},
  };
  auto it = kMap.find(name);
  return it != kMap.end() ? it->second : name;
}

static std::string unescapeString(const std::string &s) {
  std::string out;
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '\\' && i + 1 < s.size()) {
      switch (s[i + 1]) {
      case 'n':
        out += '\n';
        ++i;
        break;
      case 't':
        out += '\t';
        ++i;
        break;
      case 'r':
        out += '\r';
        ++i;
        break;
      case '\\':
        out += '\\';
        ++i;
        break;
      case '"':
        out += '"';
        ++i;
        break;
      case '0':
        out += '\0';
        ++i;
        break;
      case '{':
        out += '{';
        ++i;
        break;
      default:
        out += s[i];
        break;
      }
    } else {
      out += s[i];
    }
  }
  return out;
}

static Value *evalStrLit(const std::string &raw, LLVMContext &ctx,
                         IRBuilder<> &B,
                         const std::map<std::string, VarInfo> &vars, Module *M);

static Value *evalInterp(const std::string &inner, LLVMContext &ctx,
                         IRBuilder<> &B,
                         const std::map<std::string, VarInfo> &vars) {
  bool isIdent = !inner.empty();
  for (char c : inner)
    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
      isIdent = false;
      break;
    }

  if (isIdent) {
    auto it = vars.find(inner);
    if (it != vars.end() && !it->second.isMoved) {
      if (TypeResolver::isString(it->second.type) ||
          TypeResolver::isArray(it->second.type))
        return it->second.allocaInst;
      return B.CreateLoad(it->second.type, it->second.allocaInst,
                          inner + ".load");
    }
    return nullptr;
  }
  try {
    size_t pos;
    long long v = std::stoll(inner, &pos);
    if (pos == inner.size())
      return ConstantInt::get(Type::getInt32Ty(ctx), v);
  } catch (...) {
  }
  try {
    size_t pos;
    double v = std::stod(inner, &pos);
    if (pos == inner.size())
      return ConstantFP::get(Type::getDoubleTy(ctx), v);
  } catch (...) {
  }
  return nullptr;
}

static Value *evalStrLit(const std::string &raw, LLVMContext &ctx,
                         IRBuilder<> &B,
                         const std::map<std::string, VarInfo> &vars,
                         Module *M) {
  Value *result = StringOps::fromLiteral(B, ctx, M, "");
  size_t i = 0;
  std::string chunk;

  auto flushChunk = [&]() {
    if (!chunk.empty()) {
      Value *c = StringOps::fromLiteral(B, ctx, M, chunk);
      result = StringOps::concat(B, ctx, M, result, c);
      chunk.clear();
    }
  };

  while (i < raw.size()) {
    if (raw[i] == '\\' && i + 1 < raw.size() && raw[i + 1] == '{') {
      chunk += '{';
      i += 2;
      continue;
    }
    if (raw[i] != '{') {
      chunk += raw[i++];
      continue;
    }
    if (i + 1 < raw.size() && raw[i + 1] == '{') {
      chunk += '{';
      i += 2;
      continue;
    }

    size_t start = i + 1, depth = 1, j = start;
    while (j < raw.size() && depth > 0) {
      if (raw[j] == '{')
        ++depth;
      else if (raw[j] == '}')
        --depth;
      if (depth > 0)
        ++j;
      else
        break;
    }
    if (j >= raw.size()) {
      chunk += raw[i++];
      continue;
    }

    std::string inner = raw.substr(start, j - start);
    Value *val = evalInterp(inner, ctx, B, vars);
    if (val) {
      flushChunk();
      Value *sv = StringOps::fromValue(B, ctx, M, val);
      result = StringOps::concat(B, ctx, M, result, sv);
    } else {
      chunk += '{' + inner + '}';
    }
    i = j + 1;
  }
  flushChunk();
  return result;
}

// ------------- //
// CodeGenerator //
// ------------- //

CodeGenerator::CodeGenerator()
    : module(std::make_unique<Module>("nexus", context)), builder(context),
      scopeMgr(builder, context, module.get(), namedValues) {
  module->setTargetTriple(Triple(LLVM_HOST_TRIPLE));
}

bool CodeGenerator::isCStringPointer(Type *ty) { return ty->isPointerTy(); }

// free() lazy-declare

llvm::Function *CodeGenerator::getFree() {
  llvm::Function *f = module->getFunction("free");
  if (!f) {
    auto *ft = FunctionType::get(Type::getVoidTy(context),
                                 {PointerType::get(context, 0)}, false);
    f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "free",
                               *module);
  }
  return f;
}

// Error

Value *CodeGenerator::logError(const char *msg) {
  errs() << "\033[31mCodeGen error: " << msg << "\033[0m\n";
  return nullptr;
}

// Pending-free helpers

void CodeGenerator::scheduleStringFree(Value *strAlloca) {
  StructType *st = TypeResolver::getStringType(context);
  Value *load = builder.CreateLoad(st, strAlloca);
  Value *data = builder.CreateExtractValue(load, {0}, "tmp.data");
  pendingFrees.push_back(data);
}

void CodeGenerator::flushPendingFrees() {
  for (Value *ptr : pendingFrees)
    builder.CreateCall(getFree(), {ptr});
  pendingFrees.clear();
}

// ------------------- //
// Expression visitors //
// ------------------- //

Value *CodeGenerator::visitIntLit(const IntLitExpr &e) {
  return ConstantInt::get(Type::getInt32Ty(context),
                          std::stoll(e.lit.getWord()));
}

Value *CodeGenerator::visitFloatLit(const FloatLitExpr &e) {
  return ConstantFP::get(Type::getDoubleTy(context),
                         std::stod(e.lit.getWord()));
}

Value *CodeGenerator::visitBoolLit(const BoolLitExpr &e) {
  return ConstantInt::get(Type::getInt1Ty(context),
                          e.lit.getWord() == "true" ? 1 : 0);
}

Value *CodeGenerator::visitCharLit(const CharLitExpr &e) {
  const std::string &word = e.lit.getWord();
  char c = 0;
  if (word.size() == 1) {
    c = word[0];
  } else if (word.size() == 2 && word[0] == '\\') {
    switch (word[1]) {
    case 'n':
      c = '\n';
      break;
    case 't':
      c = '\t';
      break;
    case 'r':
      c = '\r';
      break;
    case '0':
      c = '\0';
      break;
    case '\\':
      c = '\\';
      break;
    case '\'':
      c = '\'';
      break;
    default:
      c = word[1];
      break;
    }
  }
  return llvm::ConstantInt::get(llvm::Type::getInt8Ty(context),
                                static_cast<uint8_t>(c));
}

Value *CodeGenerator::visitStrLit(const StrLitExpr &e) {
  std::string raw = unescapeString(e.lit.getWord());

  // Check for interpolation
  bool hasInterp = false;
  for (size_t i = 0; i < raw.size(); ++i) {
    if (raw[i] == '\\' && i + 1 < raw.size() && raw[i + 1] == '{') {
      ++i;
      continue;
    }
    if (raw[i] == '{' && (i + 1 >= raw.size() || raw[i + 1] != '{')) {
      hasInterp = true;
      break;
    }
  }
  if (hasInterp)
    return evalStrLit(raw, context, builder, namedValues, module.get());

  // Collapse {{ → {
  std::string processed;
  for (size_t i = 0; i < raw.size(); ++i) {
    if (raw[i] == '{' && i + 1 < raw.size() && raw[i + 1] == '{') {
      processed += '{';
      ++i;
    } else {
      processed += raw[i];
    }
  }
  processed = PrintEmitter::replaceHexColors(processed);
  return StringOps::fromLiteral(builder, context, module.get(), processed);
}

Value *CodeGenerator::visitIdentifier(const IdentExpr &e) {
  const std::string &name = e.name.token.getWord();
  auto it = namedValues.find(name);
  if (it == namedValues.end())
    return logError(("Unknown variable: " + name).c_str());
  if (it->second.isMoved)
    return logError(("Use of moved variable: " + name).c_str());

  if (it->second.isReference) {
    Value *ptr = builder.CreateLoad(PointerType::get(context, 0),
                                    it->second.allocaInst, name + ".ref");
    Type *pointee =
        it->second.pointeeType ? it->second.pointeeType : it->second.type;
    if (TypeResolver::isString(pointee) || TypeResolver::isArray(pointee))
      return ptr;
    return builder.CreateLoad(pointee, ptr, name + ".deref");
  }

  if (TypeResolver::isString(it->second.type) ||
      TypeResolver::isArray(it->second.type))
    return it->second.allocaInst;

  return builder.CreateLoad(it->second.type, it->second.allocaInst,
                            name + ".load");
}

// Binary

Value *CodeGenerator::visitBinary(const BinaryExpr &e) {

  // Determine if an expression produces a string value
  std::function<bool(const Expression &)> isStrExpr =
      [&](const Expression &ex) -> bool {
    if (dynamic_cast<const StrLitExpr *>(&ex))
      return true;
    if (auto *id = dynamic_cast<const IdentExpr *>(&ex)) {
      auto it = namedValues.find(id->name.token.getWord());
      if (it != namedValues.end())
        return TypeResolver::isString(it->second.type);
    }
    if (auto *bin = dynamic_cast<const BinaryExpr *>(&ex))
      if (bin->op == BinaryOp::Add)
        return isStrExpr(*bin->left) || isStrExpr(*bin->right);
    return false;
  };

  bool ls = isStrExpr(*e.left), rs = isStrExpr(*e.right);

  // String concatenation
  if (e.op == BinaryOp::Add && (ls || rs)) {
    Value *l = codegen(*e.left), *r = codegen(*e.right);
    if (!l || !r)
      return nullptr;

    Value *lstr =
        ls ? l : StringOps::fromValue(builder, context, module.get(), l);
    Value *rstr =
        rs ? r : StringOps::fromValue(builder, context, module.get(), r);
    Value *res = StringOps::concat(builder, context, module.get(), lstr, rstr);

    // Free temporaries (non-identifier operands)
    bool rhsIsNamed = dynamic_cast<const IdentExpr *>(e.right.get()) != nullptr;
    if (!rhsIsNamed) {
      StructType *st = TypeResolver::getStringType(context);
      Value *load = builder.CreateLoad(st, rstr);
      Value *data = builder.CreateExtractValue(load, {0}, "rhs.tmp.data");
      builder.CreateCall(getFree(), {data});
    }
    bool lhsIsNamed = dynamic_cast<const IdentExpr *>(e.left.get()) != nullptr;
    if (!lhsIsNamed && ls) {
      StructType *st = TypeResolver::getStringType(context);
      Value *load = builder.CreateLoad(st, lstr);
      Value *data = builder.CreateExtractValue(load, {0}, "lhs.tmp.data");
      builder.CreateCall(getFree(), {data});
    }
    return res;
  }

  // String equality / inequality
  if ((e.op == BinaryOp::Eq || e.op == BinaryOp::Ne) && (ls || rs)) {
    Value *l = codegen(*e.left), *r = codegen(*e.right);
    if (!l || !r)
      return nullptr;

    auto charPtr = [&](Value *v) -> Value * {
      Type *ty = v->getType();
      if (ty->isPointerTy()) {
        if (auto *ai = dyn_cast<AllocaInst>(v)) {
          if (TypeResolver::isString(ai->getAllocatedType())) {
            Value *loaded = builder.CreateLoad(
                cast<StructType>(ai->getAllocatedType()), v, "str.load");
            return builder.CreateExtractValue(loaded, {0}, "str.data");
          }
        }
        return v;
      }
      if (TypeResolver::isString(ty))
        return builder.CreateExtractValue(v, {0}, "str.data");
      return v;
    };

    llvm::Function *strcmpF = module->getFunction("strcmp");
    if (!strcmpF)
      return logError("strcmp not declared");

    Value *cmp = builder.CreateCall(strcmpF, {charPtr(l), charPtr(r)}, "scmp");
    Value *eq =
        builder.CreateICmpEQ(cmp, ConstantInt::get(cmp->getType(), 0), "seq");

    auto freeStringStruct = [&](Value *v) {
      if (auto *ai = dyn_cast<AllocaInst>(v))
        if (TypeResolver::isString(ai->getAllocatedType())) {
          StructType *st = cast<StructType>(ai->getAllocatedType());
          Value *load = builder.CreateLoad(st, ai);
          Value *data = builder.CreateExtractValue(load, {0}, "cmp.tmp.data");
          builder.CreateCall(getFree(), {data});
        }
    };

    bool lIsNamed = dynamic_cast<const IdentExpr *>(e.left.get()) != nullptr;
    bool rIsNamed = dynamic_cast<const IdentExpr *>(e.right.get()) != nullptr;
    if (!lIsNamed && ls)
      freeStringStruct(l);
    if (!rIsNamed && rs)
      freeStringStruct(r);

    return e.op == BinaryOp::Eq ? eq : builder.CreateNot(eq, "sne");
  }

  // Scalar path ArithmeticManager
  Value *l = codegen(*e.left), *r = codegen(*e.right);
  if (!l || !r)
    return nullptr;

  Value *result = ArithmeticManager::emitBinaryOp(builder, context, e.op, l, r);
  if (!result)
    return logError("Unknown binary op");
  return result;
}

// Unary

Value *CodeGenerator::visitUnary(const UnaryExpr &e) {
  Value *v = codegen(*e.operand);
  if (!v)
    return nullptr;
  switch (e.op) {
  case UnaryOp::Negate:
    return v->getType()->isFloatingPointTy() ? builder.CreateFNeg(v, "fneg")
                                             : builder.CreateNeg(v, "neg");
  case UnaryOp::Not: {
    Value *b = v->getType()->isIntegerTy(1)
                   ? v
                   : builder.CreateICmpNE(v, ConstantInt::get(v->getType(), 0),
                                          "tobool");
    return builder.CreateNot(b, "lnot");
  }
  }
  return logError("Unknown unary op");
}

// Assign

Value *CodeGenerator::visitAssign(const AssignExpr &e) {
  const std::string &tgt = e.target.token.getWord();
  auto it = namedValues.find(tgt);
  if (it == namedValues.end())
    return logError(("Undeclared variable: " + tgt).c_str());
  if (it->second.isConst)
    return logError(("Cannot reassign const variable: " + tgt).c_str());

  // Reference target
  if (it->second.isReference) {
    Value *val = codegen(*e.value);
    if (!val)
      return nullptr;
    Type *pointee =
        it->second.pointeeType ? it->second.pointeeType : it->second.type;
    Value *ptr = builder.CreateLoad(PointerType::get(context, 0),
                                    it->second.allocaInst, tgt + ".ref");
    if (TypeResolver::isString(pointee) || TypeResolver::isArray(pointee)) {
      Value *loaded = builder.CreateLoad(pointee, val);
      builder.CreateStore(loaded, ptr);
    } else {
      builder.CreateStore(TypeResolver::coerce(builder, val, pointee), ptr);
    }
    return val;
  }

  if (it->second.isBorrowed)
    return logError(("Cannot modify borrowed variable: " + tgt).c_str());

  Value *val = codegen(*e.value);
  if (!val)
    return nullptr;
  Type *targetTy = it->second.type;

  // String assignment: free old buffer first
  if (TypeResolver::isString(targetTy)) {
    Value *oldVal =
        builder.CreateLoad(targetTy, it->second.allocaInst, tgt + ".old");
    Value *oldData = builder.CreateExtractValue(oldVal, {0}, "old.data");
    builder.CreateCall(getFree(), {oldData});

    Value *loaded = builder.CreateLoad(targetTy, val);
    builder.CreateStore(loaded, it->second.allocaInst);
    return it->second.allocaInst;
  }

  val = TypeResolver::coerce(builder, val, targetTy);

  switch (e.kind) {
  case AssignKind::Copy:
    builder.CreateStore(val, it->second.allocaInst);
    break;

  case AssignKind::Move: {
    auto *sid = dynamic_cast<const IdentExpr *>(e.value.get());
    if (!sid)
      return logError("Move requires an identifier on the right-hand side");
    const std::string &src = sid->name.token.getWord();
    auto sit = namedValues.find(src);
    if (sit == namedValues.end())
      return logError(("Unknown variable: " + src).c_str());
    if (sit->second.isMoved)
      return logError(("Already moved: " + src).c_str());
    if (sit->second.isBorrowed)
      return logError(("Cannot move borrowed value: " + src).c_str());
    builder.CreateStore(val, it->second.allocaInst);
    sit->second.isMoved = true;
    break;
  }

  case AssignKind::Borrow: {
    auto *sid = dynamic_cast<const IdentExpr *>(e.value.get());
    if (!sid)
      return logError("Borrow requires an identifier on the right-hand side");
    const std::string &src = sid->name.token.getWord();
    auto sit = namedValues.find(src);
    if (sit == namedValues.end())
      return logError(("Unknown variable: " + src).c_str());
    if (sit->second.isMoved)
      return logError(("Cannot borrow moved value: " + src).c_str());
    namedValues[tgt] = {sit->second.allocaInst, sit->second.type, true, false};
    break;
  }
  }
  return val;
}

// Increment / Decrement

Value *CodeGenerator::visitIncrement(const Increment &e) {
  return generateIncrDecr(e.target.token.getWord(), true);
}

Value *CodeGenerator::visitDecrement(const Decrement &e) {
  return generateIncrDecr(e.target.token.getWord(), false);
}

Value *CodeGenerator::generateIncrDecr(const std::string &name, bool isInc) {
  auto it = namedValues.find(name);
  if (it == namedValues.end())
    return logError(("Unknown variable: " + name).c_str());
  if (it->second.isConst)
    return logError(("Cannot modify const variable: " + name).c_str());

  if (it->second.isReference) {
    Type *pt =
        it->second.pointeeType ? it->second.pointeeType : it->second.type;
    Value *ptr = builder.CreateLoad(PointerType::get(context, 0),
                                    it->second.allocaInst, name + ".ref");
    Value *cur = builder.CreateLoad(pt, ptr);
    Value *res =
        pt->isFloatingPointTy()
            ? (isInc
                   ? builder.CreateFAdd(cur, ConstantFP::get(pt, 1.0), "finc")
                   : builder.CreateFSub(cur, ConstantFP::get(pt, 1.0), "fdec"))
            : (isInc ? builder.CreateAdd(cur, ConstantInt::get(pt, 1), "inc")
                     : builder.CreateSub(cur, ConstantInt::get(pt, 1), "dec"));
    builder.CreateStore(res, ptr);
    return res;
  }

  if (it->second.isBorrowed || it->second.isMoved)
    return logError(("Cannot modify " + name).c_str());

  Type *ty = it->second.type;
  Value *cur = builder.CreateLoad(ty, it->second.allocaInst, name + ".load");
  Value *res =
      ty->isFloatingPointTy()
          ? (isInc ? builder.CreateFAdd(cur, ConstantFP::get(ty, 1.0), "finc")
                   : builder.CreateFSub(cur, ConstantFP::get(ty, 1.0), "fdec"))
          : (isInc ? builder.CreateAdd(cur, ConstantInt::get(ty, 1), "inc")
                   : builder.CreateSub(cur, ConstantInt::get(ty, 1), "dec"));
  builder.CreateStore(res, it->second.allocaInst);
  return res;
}

// Array indexing

Value *CodeGenerator::visitArrayIndex(const ArrayIndexExpr &e) {
  const std::string &name = e.array.token.getWord();
  auto it = namedValues.find(name);
  if (it == namedValues.end())
    return logError(("Unknown variable: " + name).c_str());

  Value *ptr = it->second.allocaInst;
  Type *ty = it->second.type;

  if (it->second.isReference) {
    ptr = builder.CreateLoad(PointerType::get(context, 0), ptr);
    if (it->second.pointeeType)
      ty = it->second.pointeeType;
  }

  for (size_t d = 0; d < e.indices.size(); ++d) {
    Value *idx = codegen(*e.indices[d]);
    if (!idx)
      return nullptr;
    if (idx->getType()->isIntegerTy(32))
      idx = builder.CreateSExt(idx, Type::getInt64Ty(context));

    auto *arrSt = dyn_cast<StructType>(ty);
    if (!arrSt || !TypeResolver::isArray(arrSt))
      return logError(("Not an array: " + name).c_str());

    Value *loaded = builder.CreateLoad(arrSt, ptr, "arr.load");
    Value *dataPtr = builder.CreateExtractValue(loaded, {1}, "arr.data");
    Type *elemTy = TypeResolver::elemType(context, arrSt);
    if (!elemTy)
      return logError("Cannot resolve element type");

    Value *elemPtr = builder.CreateGEP(elemTy, dataPtr, idx, "elem.ptr");

    if (d == e.indices.size() - 1) {
      if (TypeResolver::isArray(elemTy) || TypeResolver::isString(elemTy))
        return elemPtr;
      if (TypeResolver::isString(ty))
        return builder.CreateLoad(Type::getInt8Ty(context), elemPtr, "char");
      return builder.CreateLoad(elemTy, elemPtr, "elem");
    }
    ptr = elemPtr;
    ty = elemTy;
  }
  return nullptr;
}

Value *CodeGenerator::visitArrayIndexAssign(const ArrayIndexAssignExpr &e) {
  const std::string &name = e.array.token.getWord();
  auto it = namedValues.find(name);
  if (it == namedValues.end())
    return logError(("Unknown variable: " + name).c_str());
  if (it->second.isConst)
    return logError(
        ("Cannot assign to element of const array: " + name).c_str());

  Value *ptr = it->second.allocaInst;
  Type *ty = it->second.type;

  if (it->second.isReference) {
    ptr = builder.CreateLoad(PointerType::get(context, 0), ptr, name + ".ref");
    if (it->second.pointeeType)
      ty = it->second.pointeeType;
  }

  for (size_t d = 0; d < e.indices.size(); ++d) {
    Value *idx = codegen(*e.indices[d]);
    if (!idx)
      return nullptr;
    if (idx->getType()->isIntegerTy(32))
      idx = builder.CreateSExt(idx, Type::getInt64Ty(context));

    auto *arrSt = dyn_cast<StructType>(ty);
    if (!arrSt || !TypeResolver::isArray(arrSt))
      return logError(("Not an array: " + name).c_str());

    Value *loaded = builder.CreateLoad(arrSt, ptr, "arr.load");
    Value *dataPtr = builder.CreateExtractValue(loaded, {1}, "arr.data");
    Type *elemTy = TypeResolver::elemType(context, arrSt);
    if (!elemTy)
      return logError("Cannot resolve element type");

    Value *elemPtr = builder.CreateGEP(elemTy, dataPtr, idx, "elem.ptr");

    if (d == e.indices.size() - 1) {
      if (TypeResolver::isArray(elemTy) || TypeResolver::isString(elemTy))
        return logError("Cannot assign scalar to array slot");
      Value *val = codegen(*e.value);
      if (!val)
        return nullptr;
      val = TypeResolver::coerce(builder, val, elemTy);
      builder.CreateStore(val, elemPtr);
      return val;
    }
    ptr = elemPtr;
    ty = elemTy;
  }
  return nullptr;
}

// Length property

Value *CodeGenerator::visitLengthProperty(const LengthPropertyExpr &e) {
  const std::string &name = e.name.token.getWord();
  auto it = namedValues.find(name);
  if (it == namedValues.end())
    return logError(("Unknown variable: " + name).c_str());

  Type *ty = it->second.type;
  if (TypeResolver::isArray(ty)) {
    Value *loaded = builder.CreateLoad(ty, it->second.allocaInst);
    return builder.CreateExtractValue(loaded, {0}, "length");
  }
  if (TypeResolver::isString(ty)) {
    Value *loaded = builder.CreateLoad(ty, it->second.allocaInst);
    return builder.CreateExtractValue(loaded, {1}, "length");
  }
  return logError(".length not applicable to this type");
}

// New array

Value *CodeGenerator::visitNewArray(const NewArrayExpr &e) {
  std::string typeName = e.arrayType.base.token.getWord();
  Type *elemType = TypeResolver::fromName(context, typeName);
  if (!elemType)
    return nullptr;

  std::vector<Value *> dimValues;
  for (auto &sizeExpr : e.sizes) {
    Value *v = codegen(*sizeExpr);
    if (!v)
      return nullptr;
    dimValues.push_back(v);
  }
  return ArrayEmitter::makeND(builder, context, *module, elemType, dimValues);
}

// Call

Value *CodeGenerator::visitCall(const CallExpr &e) {
  const std::string rawName = e.callee.token.getWord();
  const std::string calleeName = normalizeFunctionName(rawName);

  // Nexus print builtins
  if ((calleeName == "printf" || rawName == "Printf") &&
      e.arguments.size() == 1)
    return PrintEmitter::handlePrintf(e, builder, context, module.get(),
                                      namedValues);
  if (rawName == "Print" && e.arguments.size() == 1)
    return PrintEmitter::handlePrint(e, builder, context, module.get());

  if (rawName == "Read")
    return BuiltinEmitter::handleRead(builder, context, module.get());

  if (rawName == "Random")
    return BuiltinEmitter::handleRandom(builder, context, module.get());

  llvm::Function *callee = module->getFunction(calleeName);
  if (!callee)
    return logError(("Unknown function: " + calleeName).c_str());
  if (!callee->isVarArg() && callee->arg_size() != e.arguments.size())
    return logError("Wrong number of arguments");

  auto refIt = borrowRefParams.find(calleeName);
  std::vector<Value *> args;
  for (size_t i = 0; i < e.arguments.size(); ++i) {
    bool isRef = refIt != borrowRefParams.end() && i < refIt->second.size() &&
                 refIt->second[i];
    if (isRef) {
      auto *id = dynamic_cast<const IdentExpr *>(e.arguments[i].get());
      if (!id)
        return logError("Borrow-ref parameter requires an identifier");
      auto sit = namedValues.find(id->name.token.getWord());
      if (sit == namedValues.end())
        return logError(
            ("Unknown variable: " + id->name.token.getWord()).c_str());
      args.push_back(sit->second.allocaInst);
    } else {
      Value *v = codegen(*e.arguments[i]);
      if (!v)
        return nullptr;

      if (auto *ai = dyn_cast<AllocaInst>(v)) {
        Type *allocTy = ai->getAllocatedType();
        if (TypeResolver::isArray(allocTy) || TypeResolver::isString(allocTy)) {
          v = builder.CreateLoad(allocTy, ai, "arg.val");
        }
      }
      args.push_back(v);
    }
  }

  bool isVoid = callee->getReturnType()->isVoidTy();
  return builder.CreateCall(callee, args, isVoid ? "" : "call");
}

// Expression dispatch

Value *CodeGenerator::codegen(const Expression &expr) {
  if (auto *p = dynamic_cast<const IdentExpr *>(&expr))
    return visitIdentifier(*p);
  if (auto *p = dynamic_cast<const IntLitExpr *>(&expr))
    return visitIntLit(*p);
  if (auto *p = dynamic_cast<const FloatLitExpr *>(&expr))
    return visitFloatLit(*p);
  if (auto *p = dynamic_cast<const StrLitExpr *>(&expr))
    return visitStrLit(*p);
  if (auto *p = dynamic_cast<const BoolLitExpr *>(&expr))
    return visitBoolLit(*p);
  if (auto *p = dynamic_cast<const CharLitExpr *>(&expr))
    return visitCharLit(*p);
  if (auto *p = dynamic_cast<const BinaryExpr *>(&expr))
    return visitBinary(*p);
  if (auto *p = dynamic_cast<const UnaryExpr *>(&expr))
    return visitUnary(*p);
  if (auto *p = dynamic_cast<const AssignExpr *>(&expr))
    return visitAssign(*p);
  if (auto *p = dynamic_cast<const Increment *>(&expr))
    return visitIncrement(*p);
  if (auto *p = dynamic_cast<const Decrement *>(&expr))
    return visitDecrement(*p);
  if (auto *p = dynamic_cast<const CallExpr *>(&expr))
    return visitCall(*p);
  if (auto *p = dynamic_cast<const NewArrayExpr *>(&expr))
    return visitNewArray(*p);
  if (auto *p = dynamic_cast<const ArrayIndexExpr *>(&expr))
    return visitArrayIndex(*p);
  if (auto *p = dynamic_cast<const ArrayIndexAssignExpr *>(&expr))
    return visitArrayIndexAssign(*p);
  if (auto *p = dynamic_cast<const LengthPropertyExpr *>(&expr))
    return visitLengthProperty(*p);

  return logError("Unknown expression node");
}

// ------------------ //
// Statement visitors //
// ------------------ //

Value *CodeGenerator::visitVarDecl(const VarDecl &d) {
  const std::string &name = d.name.token.getWord();
  Type *ty = TypeResolver::fromTypeDesc(context, d.type);
  if (!ty)
    return logError(("Unknown type for variable: " + name).c_str());

  AllocaInst *alloca = builder.CreateAlloca(ty, nullptr, name);
  scopeMgr.declare(name);

  if (!d.initializer) {
    namedValues[name] = VarInfo(alloca, ty, false, false, false, d.isConst);
    return alloca;
  }

  switch (d.kind) {
  case AssignKind::Move:
    return initMove(d, name, ty, alloca);
  case AssignKind::Borrow:
    return initBorrow(d, name, alloca);
  default:
    return initCopy(d, name, ty, alloca);
  }
}

Value *CodeGenerator::initCopy(const VarDecl &d, const std::string &name,
                               Type *ty, AllocaInst *alloca) {
  Value *init = codegen(*d.initializer);
  if (!init)
    return logError(("Failed to evaluate initializer for: " + name).c_str());

  if (TypeResolver::isString(ty)) {
    StructType *st = cast<StructType>(ty);
    Value *loaded = builder.CreateLoad(st, init);
    Value *srcData = builder.CreateExtractValue(loaded, {0});
    Value *srcLen = builder.CreateExtractValue(loaded, {1});
    Value *srcCap = builder.CreateExtractValue(loaded, {2});

    llvm::Function *mallocF = RTDecl::malloc_(module.get(), context);
    llvm::Function *memcpyF = RTDecl::memcpy_(module.get(), context);
    Value *newMem = builder.CreateCall(mallocF, {srcCap});
    builder.CreateCall(
        memcpyF, {newMem, srcData, srcLen, ConstantInt::getFalse(context)});
    Value *nullPos =
        builder.CreateGEP(Type::getInt8Ty(context), newMem, srcLen);
    builder.CreateStore(ConstantInt::get(Type::getInt8Ty(context), 0), nullPos);

    builder.CreateStore(newMem, builder.CreateStructGEP(st, alloca, 0));
    builder.CreateStore(srcLen, builder.CreateStructGEP(st, alloca, 1));
    builder.CreateStore(srcCap, builder.CreateStructGEP(st, alloca, 2));

    bool isTempSource =
        namedValues.find(init->getName().str()) == namedValues.end();
    if (isTempSource)
      builder.CreateCall(getFree(), {srcData});

    namedValues[name] = VarInfo(alloca, ty, false, false, false, d.isConst);
    namedValues[name].ownsHeap = true;
    return alloca;
  }

  if (TypeResolver::isArray(ty)) {
    Value *loaded = builder.CreateLoad(ty, init);
    builder.CreateStore(loaded, alloca);
    namedValues[name] = VarInfo(alloca, ty, false, false, false, d.isConst);
    namedValues[name].ownsHeap = true;
    return alloca;
  }

  init = TypeResolver::coerce(builder, init, ty);
  builder.CreateStore(init, alloca);
  namedValues[name] = VarInfo(alloca, ty, false, false, false, d.isConst);
  return alloca;
}

Value *CodeGenerator::initMove(const VarDecl &d, const std::string &name,
                               Type *ty, AllocaInst *alloca) {
  auto *id = dynamic_cast<const IdentExpr *>(d.initializer.get());
  if (!id)
    return logError("Move initialiser requires a variable (identifier)");

  const std::string &src = id->name.token.getWord();
  auto sit = namedValues.find(src);
  if (sit == namedValues.end())
    return logError(("Unknown variable: " + src).c_str());
  if (sit->second.isMoved)
    return logError(("Already moved: " + src).c_str());
  if (sit->second.isBorrowed)
    return logError(("Cannot move borrowed value: " + src).c_str());
  if (sit->second.isConst)
    return logError(("Cannot move const value: " + src).c_str());

  Value *val = builder.CreateLoad(sit->second.type, sit->second.allocaInst,
                                  src + ".move");
  builder.CreateStore(val, alloca);
  sit->second.isMoved = true;
  sit->second.ownsHeap = false;

  namedValues[name] = VarInfo(alloca, ty, false, false, false, d.isConst);
  namedValues[name].ownsHeap = true;
  return alloca;
}

Value *CodeGenerator::initBorrow(const VarDecl &d, const std::string &name,
                                 AllocaInst *alloca) {
  auto *id = dynamic_cast<const IdentExpr *>(d.initializer.get());
  if (!id)
    return logError("Borrow initialiser requires a variable (identifier)");

  const std::string &src = id->name.token.getWord();
  auto sit = namedValues.find(src);
  if (sit == namedValues.end())
    return logError(("Unknown variable: " + src).c_str());
  if (sit->second.isMoved)
    return logError(("Cannot borrow moved value: " + src).c_str());

  builder.CreateStore(sit->second.allocaInst, alloca);
  sit->second.isBorrowed = true;

  Type *refTy = PointerType::get(context, 0);
  VarInfo vi(alloca, refTy, false, false, /*isRef=*/true, d.isConst, src);
  vi.pointeeType = sit->second.type;
  namedValues[name] = vi;
  return alloca;
}

// Statement dispatch

Value *CodeGenerator::codegen(const Statement &stmt) {
  if (auto *p = dynamic_cast<const VarDecl *>(&stmt))
    return visitVarDecl(*p);
  if (auto *p = dynamic_cast<const ExprStmt *>(&stmt)) {
    Value *v = codegen(*p->expr);
    flushPendingFrees();
    return v;
  }
  if (auto *p = dynamic_cast<const IfStmt *>(&stmt))
    return visitIfStmt(*p);
  if (auto *p = dynamic_cast<const WhileStmt *>(&stmt))
    return visitWhileStmt(*p);
  if (auto *p = dynamic_cast<const Return *>(&stmt))
    return visitReturn(*p);
  return logError("Unknown statement node");
}

// NOTE: scope is popped by the caller (visitIfStmt, visitWhileStmt, etc.)
// so they can emit the branch first. For plain blocks the caller pops too.
void CodeGenerator::codegen(const Block &block) {
  scopeMgr.pushScope();
  for (const auto &s : block.statements) {
    if (builder.GetInsertBlock()->getTerminator())
      break;
    codegen(*s);
  }
}

// Control flow

Value *CodeGenerator::visitIfStmt(const IfStmt &s) {
  Value *cond = codegen(*s.condition);
  if (!cond)
    return nullptr;
  if (!cond->getType()->isIntegerTy(1))
    cond = builder.CreateICmpNE(cond, ConstantInt::get(cond->getType(), 0),
                                "if.cond");

  llvm::Function *fn = builder.GetInsertBlock()->getParent();
  BasicBlock *thenBB = BasicBlock::Create(context, "then", fn);
  BasicBlock *elseBB = BasicBlock::Create(context, "else");
  BasicBlock *mergeBB = BasicBlock::Create(context, "merge");

  builder.CreateCondBr(cond, thenBB, elseBB);

  // then
  builder.SetInsertPoint(thenBB);
  codegen(*s.thenBranch);
  scopeMgr.popScope();
  if (!builder.GetInsertBlock()->getTerminator())
    builder.CreateBr(mergeBB);

  // else
  fn->insert(fn->end(), elseBB);
  builder.SetInsertPoint(elseBB);
  if (s.elseBranch) {
    codegen(*s.elseBranch);
    scopeMgr.popScope();
  }
  if (!builder.GetInsertBlock()->getTerminator())
    builder.CreateBr(mergeBB);

  fn->insert(fn->end(), mergeBB);
  builder.SetInsertPoint(mergeBB);
  return nullptr;
}

Value *CodeGenerator::visitWhileStmt(const WhileStmt &s) {
  llvm::Function *fn = builder.GetInsertBlock()->getParent();
  BasicBlock *condBB = BasicBlock::Create(context, "while.cond", fn);
  BasicBlock *bodyBB = BasicBlock::Create(context, "while.body");
  BasicBlock *exitBB = BasicBlock::Create(context, "while.exit");

  builder.CreateBr(condBB);

  builder.SetInsertPoint(condBB);
  Value *cond = codegen(*s.condition);
  if (!cond)
    return nullptr;
  if (!cond->getType()->isIntegerTy(1))
    cond =
        builder.CreateICmpNE(cond, ConstantInt::get(cond->getType(), 0), "wc");
  builder.CreateCondBr(cond, bodyBB, exitBB);

  fn->insert(fn->end(), bodyBB);
  builder.SetInsertPoint(bodyBB);
  codegen(*s.doBranch);
  scopeMgr.popScope();
  if (!builder.GetInsertBlock()->getTerminator())
    builder.CreateBr(condBB);

  fn->insert(fn->end(), exitBB);
  builder.SetInsertPoint(exitBB);
  return nullptr;
}

Value *CodeGenerator::visitReturn(const Return &s) {
  scopeMgr.popAll();
  if (s.value) {
    Value *v = codegen(**s.value);
    if (!v)
      return nullptr;
    builder.CreateRet(v);
  } else {
    builder.CreateRetVoid();
  }
  return nullptr;
}

// ---------------- //
// Function codegen //
// ---------------- //

llvm::Function *CodeGenerator::codegen(const AST_H::Function &func) {
  const std::string fname = normalizeFunctionName(func.name.token.getWord());
  Type *retTy = TypeResolver::fromTypeDesc(context, func.returnType);
  if (!retTy)
    return nullptr;

  std::vector<Type *> paramTypes;
  std::vector<bool> paramIsRef;
  for (const auto &p : func.params) {
    Type *pt = TypeResolver::fromTypeDesc(context, p.type);
    if (!pt)
      return nullptr;
    paramTypes.push_back(p.isBorrowRef ? PointerType::get(context, 0) : pt);
    paramIsRef.push_back(p.isBorrowRef);
  }
  borrowRefParams[fname] = paramIsRef;

  llvm::Function *f = module->getFunction(fname);
  if (!f) {
    auto *ft = FunctionType::get(retTy, paramTypes, false);
    f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, fname,
                               *module);
  }
  f->addFnAttr("stackrealignment");

  BasicBlock *entry = BasicBlock::Create(context, "entry", f);
  builder.SetInsertPoint(entry);

  if (fname == "main") {
    BuiltinEmitter::emitRuntimeInit(builder, context, module.get());
  }

  namedValues.clear();

  // Push the function's top-level scope
  scopeMgr.pushScope();

  size_t idx = 0;
  for (auto &arg : f->args()) {
    const auto &param = func.params[idx++];
    const std::string pname = param.name.token.getWord();

    if (param.isBorrowRef) {
      AllocaInst *ptrAlloca = builder.CreateAlloca(PointerType::get(context, 0),
                                                   nullptr, pname + ".refptr");
      builder.CreateStore(&arg, ptrAlloca);
      Type *pointee = TypeResolver::fromTypeDesc(context, param.type);
      VarInfo vi(ptrAlloca, PointerType::get(context, 0), false, false, true,
                 param.isConst);
      vi.pointeeType = pointee;
      namedValues[pname] = vi;
    } else {
      Type *declaredTy = TypeResolver::fromTypeDesc(context, param.type);
      if (!declaredTy)
        declaredTy = arg.getType(); // fallback for scalars

      AllocaInst *a = builder.CreateAlloca(declaredTy, nullptr, pname);

      if (TypeResolver::isArray(declaredTy) ||
          TypeResolver::isString(declaredTy)) {
        builder.CreateStore(&arg, a);
      } else {
        builder.CreateStore(&arg, a);
      }

      namedValues[pname] =
          VarInfo(a, declaredTy, false, false, false, param.isConst);
    }
    scopeMgr.declare(pname);
  }

  codegen(*func.body);

  if (!builder.GetInsertBlock()->getTerminator()) {
    scopeMgr.popAll();
    if (retTy->isVoidTy())
      builder.CreateRetVoid();
    else
      builder.CreateRet(ConstantInt::get(retTy, 0));
  }

  if (verifyFunction(*f, &errs())) {
    f->eraseFromParent();
    return nullptr;
  }
  return f;
}

// ------------------ //
// Top-level generate //
// ------------------ //

bool CodeGenerator::generate(const Program &program,
                             const std::string &outputFilename) {
  namedValues.clear();

  Type *ptrTy = PointerType::get(context, 0);
  Type *i32 = Type::getInt32Ty(context);

  if (!module->getFunction("printf")) {
    auto *ft = FunctionType::get(i32, {ptrTy}, true);
    llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "printf",
                           *module);
  }
  if (!module->getFunction("strcmp")) {
    auto *ft = FunctionType::get(i32, {ptrTy, ptrTy}, false);
    llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "strcmp",
                           *module);
  }
  if (!module->getFunction("scanf")) {
    auto *ft = FunctionType::get(i32, {ptrTy, ptrTy}, false);
    llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "scanf",
                           *module);
  }

  for (const auto &fn : program.functions)
    if (!codegen(*fn))
      return false;

  module->print(llvm::outs(), nullptr);

  std::error_code ec;
  raw_fd_ostream out(outputFilename + ".ll", ec, sys::fs::OF_None);
  if (ec) {
    errs() << "Cannot open output file: " << ec.message() << "\n";
    return false;
  }
  module->print(out, nullptr);
  return true;
}
