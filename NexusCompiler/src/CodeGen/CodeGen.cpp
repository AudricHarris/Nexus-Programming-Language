#include "CodeGen.h"
#include "CodeGenUtils.h"
#include "Emitters/BuiltinEmitter.h"
#include "Emitters/PrintEmitter.h"
#include "Emitters/StringEmitter.h"
#include "Manager/ArithmeticManager.h"
#include "TypeResolver.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include <memory>
#include <string>
#include <unordered_map>

using namespace llvm;

// --------- //
// Utilities //
// --------- //

static std::string normalizeFunctionName(const std::string &name) {
  static const std::unordered_map<std::string, std::string> kMap = {
      {"Main", "main"},     {"Printf", "printf"}, {"Print", "print_literal"},
      {"printf", "printf"}, {"Read", "read"},     {"Random", "random"},
  };
  auto it = kMap.find(name);
  return it != kMap.end() ? it->second : name;
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
    Value *val = codegen_utils::evalInterp(inner, ctx, B, vars);
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

Value *CodeGenerator::logError(const char *msg) {
  errs() << "\033[31mCodeGen error: " << msg << "\033[0m\n";
  return nullptr;
}

// ------------------------------------------ //
// Dispatch single call site, no dynamic_cast //
// ------------------------------------------ //

Value *CodeGenerator::codegen(const Expression &expr) {
  return expr.accept(*this);
}

Value *CodeGenerator::codegen(const Statement &stmt) {
  return stmt.accept(*this);
}

// --------------------------- //
// ExprVisitor implementations //
// --------------------------- //

Value *CodeGenerator::visitIntLit(const IntLitExpr &e) {
  return ConstantInt::get(Type::getInt32Ty(context),
                          std::stoll(e.lit.getWord()));
}

Value *CodeGenerator::visitFloatLit(const FloatLitExpr &e) {
  return ConstantFP::get(Type::getFloatTy(context), std::stod(e.lit.getWord()));
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

Value *CodeGenerator::visitNullLit(const NullLitExpr &) {
  return llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0));
}

Value *CodeGenerator::visitStrLit(const StrLitExpr &e) {
  std::string raw = codegen_utils::unescapeString(e.lit.getWord());

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
  if (hasInterp) {
    Value *v = evalStrLit(raw, context, builder, namedValues, module.get());
    if (auto *ai = llvm::dyn_cast<llvm::AllocaInst>(v))
      scopeMgr.declareTmp(ai, TypeResolver::getStringType(context));
    return v;
  }

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
  Value *v = StringOps::fromLiteral(builder, context, module.get(), processed);
  if (auto *ai = llvm::dyn_cast<llvm::AllocaInst>(v))
    scopeMgr.declareTmp(ai, TypeResolver::getStringType(context));
  return v;
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

Value *CodeGenerator::visitBinary(const BinaryExpr &expr) {
  Value *lhs = codegen(*expr.left);
  Value *rhs = codegen(*expr.right);

  if (!lhs || !rhs)
    return nullptr;

  auto resolveType = [&](Value *v, const Expression &e) -> Type * {
    if (auto *id = dynamic_cast<const IdentExpr *>(&e)) {
      auto it = namedValues.find(id->name.token.getWord());
      if (it != namedValues.end())
        return it->second.type;
    }
    if (auto *ai = llvm::dyn_cast<llvm::AllocaInst>(v))
      return ai->getAllocatedType();
    return v->getType();
  };

  Type *lTy = resolveType(lhs, *expr.left);
  Type *rTy = resolveType(rhs, *expr.right);

  if (expr.op == BinaryOp::Add) {
    bool lIsStr = TypeResolver::isString(lTy);
    bool rIsStr = TypeResolver::isString(rTy);
    if (lIsStr || rIsStr) {
      if (!lIsStr)
        lhs = StringOps::fromValue(builder, context, module.get(), lhs);
      if (!rIsStr)
        rhs = StringOps::fromValue(builder, context, module.get(), rhs);
      return StringOps::concat(builder, context, module.get(), lhs, rhs);
    }
  }

  bool lIsStr = TypeResolver::isString(lTy);
  bool rIsStr = TypeResolver::isString(rTy);
  if (lIsStr && rIsStr) {
    switch (expr.op) {
    case BinaryOp::Add: {
      Value *cat = StringOps::concat(builder, context, module.get(), lhs, rhs);
      if (auto *ai = llvm::dyn_cast<llvm::AllocaInst>(cat))
        scopeMgr.declareTmp(ai, TypeResolver::getStringType(context));
      return cat;
    }
    case BinaryOp::Eq:
      return StringOps::equals(builder, context, module.get(), lhs, rhs);
    case BinaryOp::Ne: {
      Value *eq = StringOps::equals(builder, context, module.get(), lhs, rhs);
      return builder.CreateNot(eq);
    }
    default:
      return logError("Unsupported string operator");
    }
  }

  // Promote mismatched numeric types so LLVM never sees a float+i32
  // instruction. Rule: if either side is floating-point, widen the integer side
  // to match.
  Type *lValTy = lhs->getType();
  Type *rValTy = rhs->getType();
  if (lValTy->isFloatingPointTy() && rValTy->isIntegerTy())
    rhs = builder.CreateSIToFP(rhs, lValTy, "itof");
  else if (rValTy->isFloatingPointTy() && lValTy->isIntegerTy())
    lhs = builder.CreateSIToFP(lhs, rValTy, "itof");
  // If both are floats but different widths (float vs double), widen to double.
  else if (lValTy->isFloatingPointTy() && rValTy->isFloatingPointTy() &&
           lValTy != rValTy) {
    Type *wide =
        lValTy->getPrimitiveSizeInBits() >= rValTy->getPrimitiveSizeInBits()
            ? lValTy
            : rValTy;
    if (lValTy != wide)
      lhs = builder.CreateFPExt(lhs, wide, "fpext");
    if (rValTy != wide)
      rhs = builder.CreateFPExt(rhs, wide, "fpext");
  }

  Value *result =
      ArithmeticManager::emitBinaryOp(builder, context, expr.op, lhs, rhs);
  if (!result)
    return logError("Unsupported binary operator");
  return result;
}

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

Value *CodeGenerator::visitAssign(const AssignExpr &e) {
  const std::string &tgt = e.target.token.getWord();
  auto it = namedValues.find(tgt);
  if (it == namedValues.end())
    return logError(("Undeclared variable: " + tgt).c_str());
  if (it->second.isConst)
    return logError(("Cannot reassign const variable: " + tgt).c_str());

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

  if (TypeResolver::isString(targetTy)) {
    Value *oldVal =
        builder.CreateLoad(targetTy, it->second.allocaInst, tgt + ".old");
    Value *oldData = builder.CreateExtractValue(oldVal, {0}, "old.data");
    builder.CreateCall(getFree(), {oldData});

    Value *loaded = builder.CreateLoad(targetTy, val);
    builder.CreateStore(loaded, it->second.allocaInst);

    if (auto *ai = llvm::dyn_cast<llvm::AllocaInst>(val)) {
      llvm::StructType *st = TypeResolver::getStringType(context);
      Value *dataField = builder.CreateStructGEP(st, ai, 0, "null.data.ptr");
      builder.CreateStore(
          llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0)),
          dataField);
    }
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

Value *CodeGenerator::visitCompoundAssign(const CompoundAssignExpr &e) {
  const std::string &name = e.target.token.getWord();
  auto it = namedValues.find(name);
  if (it == namedValues.end())
    return logError(("Unknown variable: " + name).c_str());
  if (it->second.isConst)
    return logError(
        ("Cannot compound-assign to const variable: " + name).c_str());

  Type *targetTy = it->second.type;

  // ── String += ────────────────────────────────────────────────────────────
  // Only += is allowed on strings; all other compound ops are an error.
  if (TypeResolver::isString(targetTy)) {
    if (e.op != BinaryOp::Add)
      return logError(
          ("Compound operator is not supported on string variable: " + name)
              .c_str());

    Value *rhs = codegen(*e.value);
    if (!rhs)
      return nullptr;

    // Determine rhs type so we can stringify non-string values automatically.
    Type *rhsTy = nullptr;
    if (auto *ai = llvm::dyn_cast<llvm::AllocaInst>(rhs))
      rhsTy = ai->getAllocatedType();
    else
      rhsTy = rhs->getType();

    // Coerce rhs to a string alloca if it isn't one already.
    Value *rhsStr =
        TypeResolver::isString(rhsTy)
            ? rhs
            : StringOps::fromValue(builder, context, module.get(), rhs);

    // Concatenate: new = old + rhs
    Value *concat = StringOps::concat(builder, context, module.get(),
                                      it->second.allocaInst, rhsStr);

    // Free the old string buffer before overwriting.
    llvm::StructType *strSt = TypeResolver::getStringType(context);
    Value *oldVal =
        builder.CreateLoad(strSt, it->second.allocaInst, name + ".old");
    Value *oldData = builder.CreateExtractValue(oldVal, {0}, "old.data");
    builder.CreateCall(getFree(), {oldData});

    // Store the concatenated result back.
    Value *newVal = builder.CreateLoad(strSt, concat, "concat.val");
    builder.CreateStore(newVal, it->second.allocaInst);

    // Null out the temporary concat buffer's data pointer so it won't
    // be double-freed if the scope manager cleans it up.
    if (auto *ai = llvm::dyn_cast<llvm::AllocaInst>(concat)) {
      Value *dataField = builder.CreateStructGEP(strSt, ai, 0, "tmp.data.ptr");
      builder.CreateStore(
          llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0)),
          dataField);
    }

    return it->second.allocaInst;
  }

  // ── Numeric compound assignment (+=, -=, *=, /=, %=, …) ─────────────────
  Value *cur =
      builder.CreateLoad(targetTy, it->second.allocaInst, name + ".load");

  Value *rhs = codegen(*e.value);
  if (!rhs)
    return nullptr;

  // Promote operands so both sides always have the same type before we hand
  // them to emitBinaryOp. The variable's declared type wins: we widen rhs
  // to match cur, never the other way around.
  Type *curTy = cur->getType();
  Type *rhsTy = rhs->getType();
  if (curTy->isFloatingPointTy() && rhsTy->isIntegerTy()) {
    // int rhs into float/double var  (e.g. floatVar += 1)
    rhs = builder.CreateSIToFP(rhs, curTy, "itof");
  } else if (curTy->isIntegerTy() && rhsTy->isFloatingPointTy()) {
    // float rhs into int var -- truncate (lossy but intentional)
    rhs = builder.CreateFPToSI(rhs, curTy, "ftoi");
  } else if (curTy->isFloatingPointTy() && rhsTy->isFloatingPointTy() &&
             curTy != rhsTy) {
    // float vs double -- widen or truncate rhs to match the variable's type
    rhs = curTy->getPrimitiveSizeInBits() > rhsTy->getPrimitiveSizeInBits()
              ? builder.CreateFPExt(rhs, curTy, "fpext")
              : builder.CreateFPTrunc(rhs, curTy, "fptrunc");
  }

  Value *result =
      ArithmeticManager::emitBinaryOp(builder, context, e.op, cur, rhs);
  if (!result)
    return logError(
        ("Unsupported compound operator on variable: " + name).c_str());

  // The arithmetic may have widened the result (e.g. float+double -> double).
  // Always truncate/convert back to the variable's declared type before
  // storing, otherwise we write more bytes than the alloca holds and corrupt
  // the stack -- which is the root cause of the SEGV seen in out.ll where a
  // double result was stored into a 4-byte float alloca.
  Type *resultTy = result->getType();
  if (resultTy != targetTy) {
    if (targetTy->isFloatingPointTy() && resultTy->isFloatingPointTy()) {
      result = targetTy->getPrimitiveSizeInBits() <
                       resultTy->getPrimitiveSizeInBits()
                   ? builder.CreateFPTrunc(result, targetTy, "fptrunc.back")
                   : builder.CreateFPExt(result, targetTy, "fpext.back");
    } else if (targetTy->isIntegerTy() && resultTy->isFloatingPointTy()) {
      result = builder.CreateFPToSI(result, targetTy, "ftoi.back");
    } else if (targetTy->isFloatingPointTy() && resultTy->isIntegerTy()) {
      result = builder.CreateSIToFP(result, targetTy, "itof.back");
    }
  }

  builder.CreateStore(result, it->second.allocaInst);
  return result;
}

static Type *resolveElemType(llvm::LLVMContext &context,
                             llvm::StructType *arrSt) {
  Type *elemTy = TypeResolver::elemType(context, arrSt);
  if (elemTy)
    return elemTy;
  llvm::StringRef name = arrSt->getName();
  if (name.starts_with("array.")) {
    std::string innerName = name.substr(6).str();
    Type *t = llvm::StructType::getTypeByName(context, innerName);
    if (t)
      return t;
    t = TypeResolver::fromName(context, innerName);
    if (t)
      return t;
  }
  return nullptr;
}

Value *CodeGenerator::visitArrayIndex(const ArrayIndexExpr &e) {
  Value *ptr = nullptr;
  Type *ty = nullptr;
  std::string name;

  if (e.object) {
    ptr = codegen(*e.object);
    if (!ptr)
      return nullptr;
    if (auto *ai = llvm::dyn_cast<llvm::AllocaInst>(ptr))
      ty = ai->getAllocatedType();
    else if (auto *gep = llvm::dyn_cast<llvm::GetElementPtrInst>(ptr))
      ty = gep->getResultElementType();
    else
      return logError("Cannot determine type of array base expression");
    name = "<expr>";
  } else {
    name = e.array.token.getWord();
    auto it = namedValues.find(name);
    if (it == namedValues.end())
      return logError(("Unknown variable: " + name).c_str());

    if (TypeResolver::isString(it->second.type)) {
      if (e.indices.size() != 1)
        return logError("String indexing takes exactly one index");
      Value *idx = codegen(*e.indices[0]);
      if (!idx)
        return nullptr;
      if (idx->getType()->isIntegerTy(32))
        idx = builder.CreateSExt(idx, Type::getInt64Ty(context));
      llvm::StructType *strSt = TypeResolver::getStringType(context);
      Value *loaded =
          builder.CreateLoad(strSt, it->second.allocaInst, "str.load");
      Value *dataPtr = builder.CreateExtractValue(loaded, {0}, "str.data");
      Value *charPtr =
          builder.CreateGEP(Type::getInt8Ty(context), dataPtr, idx, "char.ptr");
      return builder.CreateLoad(Type::getInt8Ty(context), charPtr, "char");
    }

    ptr = it->second.allocaInst;
    ty = it->second.type;

    if (it->second.isReference) {
      ptr = builder.CreateLoad(PointerType::get(context, 0), ptr);
      if (it->second.pointeeType)
        ty = it->second.pointeeType;
    }
  }

  {
    auto *maybeSt = llvm::dyn_cast<llvm::StructType>(ty);
    if (!maybeSt || !TypeResolver::isArray(maybeSt)) {
      for (size_t i = 0; i < e.indices.size(); ++i) {
        ty = TypeResolver::getOrCreateArrayStruct(context, ty);
        if (!ty)
          return logError("Failed to reconstruct array type");
      }
    }
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
    Type *elemTy = resolveElemType(context, arrSt);
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
  Value *ptr = nullptr;
  Type *ty = nullptr;
  std::string name;

  if (e.object) {
    ptr = codegen(*e.object);
    if (!ptr)
      return nullptr;
    if (auto *ai = llvm::dyn_cast<llvm::AllocaInst>(ptr))
      ty = ai->getAllocatedType();
    else if (auto *gep = llvm::dyn_cast<llvm::GetElementPtrInst>(ptr))
      ty = gep->getResultElementType();
    else
      return logError("Cannot determine type of array base expression");
    name = "<expr>";
  } else {
    name = e.array.token.getWord();
    auto it = namedValues.find(name);
    if (it == namedValues.end())
      return logError(("Unknown variable: " + name).c_str());
    if (it->second.isConst)
      return logError(
          ("Cannot assign to element of const array: " + name).c_str());

    ptr = it->second.allocaInst;
    ty = it->second.type;

    if (it->second.isReference) {
      ptr =
          builder.CreateLoad(PointerType::get(context, 0), ptr, name + ".ref");
      if (it->second.pointeeType)
        ty = it->second.pointeeType;
    }
  }

  {
    auto *maybeSt = llvm::dyn_cast<llvm::StructType>(ty);
    if (!maybeSt || !TypeResolver::isArray(maybeSt)) {
      for (size_t i = 0; i < e.indices.size(); ++i) {
        ty = TypeResolver::getOrCreateArrayStruct(context, ty);
        if (!ty)
          return logError("Failed to reconstruct array type");
      }
    }
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
    Type *elemTy = resolveElemType(context, arrSt);
    if (!elemTy)
      return logError("Cannot resolve element type");

    Value *elemPtr = builder.CreateGEP(elemTy, dataPtr, idx, "elem.ptr");

    if (d == e.indices.size() - 1) {
      Value *val = codegen(*e.value);
      if (!val)
        return nullptr;
      if (TypeResolver::isArray(elemTy) || TypeResolver::isString(elemTy)) {
        if (val->getType()->isPointerTy())
          val = builder.CreateLoad(elemTy, val, "slot.val");
        builder.CreateStore(val, elemPtr);
        return val;
      }
      if (llvm::dyn_cast<llvm::StructType>(elemTy)) {
        Value *structVal = val;
        if (val->getType()->isPointerTy())
          structVal = builder.CreateLoad(elemTy, val, "struct.val");
        builder.CreateStore(structVal, elemPtr);
        return val;
      }
      val = TypeResolver::coerce(builder, val, elemTy);
      builder.CreateStore(val, elemPtr);
      return val;
    }
    ptr = elemPtr;
    ty = elemTy;
  }
  return nullptr;
}

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

Value *CodeGenerator::visitIndexedLength(const IndexedLengthExpr &e) {
  const std::string &name = e.arrayName.token.getWord();
  auto it = namedValues.find(name);
  if (it == namedValues.end())
    return logError(("Unknown variable: " + name).c_str());

  Value *ptr = it->second.allocaInst;
  Type *ty = it->second.type;

  for (size_t d = 0; d < e.indices.size(); ++d) {
    auto *arrSt = dyn_cast<StructType>(ty);
    if (!arrSt || !TypeResolver::isArray(arrSt))
      return logError("Not an array");

    Value *idx = codegen(*e.indices[d]);
    if (!idx)
      return nullptr;
    if (idx->getType()->isIntegerTy(32))
      idx = builder.CreateSExt(idx, Type::getInt64Ty(context));

    Value *loaded = builder.CreateLoad(arrSt, ptr, "arr.load");
    Value *dataPtr = builder.CreateExtractValue(loaded, {1}, "arr.data");
    Type *elemTy = resolveElemType(context, arrSt);
    if (!elemTy)
      return logError("Cannot resolve element type");

    ptr = builder.CreateGEP(elemTy, dataPtr, idx, "elem.ptr");
    ty = elemTy;
  }

  auto *subSt = dyn_cast<StructType>(ty);
  if (!subSt || !TypeResolver::isArray(subSt))
    return logError(".length: element is not an array");

  Value *loaded = builder.CreateLoad(subSt, ptr, "subarr.load");
  return builder.CreateExtractValue(loaded, {0}, "length");
}

Value *CodeGenerator::visitNewArray(const NewArrayExpr &e) {
  std::string typeName = e.arrayType.base.token.getWord();
  Type *elemType = TypeResolver::fromName(context, typeName);
  if (!elemType)
    elemType = llvm::StructType::getTypeByName(context, typeName);
  if (!elemType)
    return logError(("Unknown element type: " + typeName).c_str());

  std::vector<Value *> dimValues;
  for (auto &sizeExpr : e.sizes) {
    Value *v = codegen(*sizeExpr);
    if (!v)
      return nullptr;
    dimValues.push_back(v);
  }
  return ArrayEmitter::makeND(builder, context, *module, elemType, dimValues);
}

Value *CodeGenerator::visitCall(const CallExpr &e) {
  const std::string rawName = e.callee.token.getWord();
  const std::string calleeName = normalizeFunctionName(rawName);

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

      if (sit->second.isReference) {
        Value *ptr = builder.CreateLoad(PointerType::get(context, 0),
                                        sit->second.allocaInst,
                                        id->name.token.getWord() + ".fwd");
        args.push_back(ptr);
      } else {
        args.push_back(sit->second.allocaInst);
      }
    } else {
      Value *v = codegen(*e.arguments[i]);
      if (!v)
        return nullptr;

      if (auto *ai = dyn_cast<AllocaInst>(v)) {
        Type *allocTy = ai->getAllocatedType();
        if (TypeResolver::isString(allocTy)) {
          bool expectsRawPtr = false;
          bool isExtern = callee->isDeclaration();
          if (i < callee->arg_size()) {
            Type *expectedTy = callee->getFunctionType()->getParamType(i);
            expectsRawPtr = expectedTy->isPointerTy();
          }
          if (expectsRawPtr) {
            Value *loaded = builder.CreateLoad(allocTy, ai, "str.load");
            Value *strData =
                builder.CreateExtractValue(loaded, {0}, "str.data");
            if (isExtern) {
              bool isGL = calleeName.size() >= 2 && calleeName[0] == 'g' &&
                          calleeName[1] == 'l' &&
                          (calleeName.size() < 4 || calleeName[2] != 'f');
              if (isGL) {
                AllocaInst *slot = builder.CreateAlloca(
                    PointerType::get(context, 0), nullptr, "strptr.slot");
                builder.CreateStore(strData, slot);
                v = slot;
              } else {
                v = strData;
              }
            } else {
              v = strData;
            }
          } else {
            v = builder.CreateLoad(allocTy, ai, "arg.val");
          }
        } else if (TypeResolver::isArray(allocTy)) {
          bool isExtern = callee->isDeclaration();
          bool expectsRawPtr = false;
          if (i < callee->arg_size()) {
            Type *expectedTy = callee->getFunctionType()->getParamType(i);
            expectsRawPtr = expectedTy->isPointerTy();
          }
          if (isExtern && expectsRawPtr) {
            Value *loaded = builder.CreateLoad(allocTy, ai, "arr.load");
            v = builder.CreateExtractValue(loaded, {1}, "arr.data");
          } else {
            v = builder.CreateLoad(allocTy, ai, "arg.val");
          }
        }
      } else {
        Type *vTy = v->getType();
        if (i < callee->arg_size()) {
          Type *expectedTy = callee->getFunctionType()->getParamType(i);
          if (vTy->isPointerTy() && (TypeResolver::isString(expectedTy) ||
                                     TypeResolver::isArray(expectedTy))) {
            v = builder.CreateLoad(expectedTy, v, "deref.arg");
          }
        }
      }

      if (callee->isDeclaration() && i < callee->arg_size()) {
        Type *expectedTy = callee->getFunctionType()->getParamType(i);
        if (expectedTy->isPointerTy()) {
          if (auto *id =
                  dynamic_cast<const IdentExpr *>(e.arguments[i].get())) {
            auto sit = namedValues.find(id->name.token.getWord());
            if (sit != namedValues.end() &&
                !TypeResolver::isString(sit->second.type) &&
                !TypeResolver::isArray(sit->second.type)) {
              v = sit->second.allocaInst;
            }
          }
        }
      }

      if (!callee->isVarArg() && i < callee->arg_size()) {
        Type *expectedTy = callee->getFunctionType()->getParamType(i);
        v = TypeResolver::coerce(builder, v, expectedTy);
      }

      args.push_back(v);
    }
  }

  bool isVoid = callee->getReturnType()->isVoidTy();
  return builder.CreateCall(callee, args, isVoid ? "" : "call");
}

Value *CodeGenerator::visitFieldAccess(const FieldAccessExpr &e) {
  auto [structPtr, st] = resolveStructPtr(*e.object);
  if (!structPtr || !st)
    return logError("Field access requires a struct expression");

  const std::string structName = st->getName().str();
  unsigned idx = 0;
  bool found = false;
  for (const auto &s : structDefs) {
    if (s->name == structName) {
      for (unsigned i = 0; i < s->fields.size(); ++i) {
        if (s->fields[i].name == e.field) {
          idx = i;
          found = true;
          break;
        }
      }
    }
  }
  if (!found)
    return logError(("Unknown field: " + e.field).c_str());

  Value *gep = builder.CreateStructGEP(st, structPtr, idx, e.field + ".ptr");
  Type *fieldTy = st->getElementType(idx);

  if (TypeResolver::isString(fieldTy) || TypeResolver::isArray(fieldTy))
    return gep;

  return builder.CreateLoad(fieldTy, gep, e.field);
}

std::pair<Value *, llvm::StructType *>
CodeGenerator::resolveStructPtr(const Expression &expr) {
  if (auto *id = dynamic_cast<const IdentExpr *>(&expr)) {
    const std::string &name = id->name.token.getWord();
    auto it = namedValues.find(name);
    if (it == namedValues.end())
      return {nullptr, nullptr};

    Value *ptr = it->second.allocaInst;
    Type *ty = it->second.type;

    if (it->second.isReference) {
      ptr =
          builder.CreateLoad(PointerType::get(context, 0), ptr, name + ".ref");
      ty = it->second.pointeeType ? it->second.pointeeType : ty;
    }

    auto *st = llvm::dyn_cast<llvm::StructType>(ty);
    if (!st)
      return {nullptr, nullptr};
    return {ptr, st};
  }

  if (auto *ai = dynamic_cast<const ArrayIndexExpr *>(&expr)) {
    const std::string &name = ai->array.token.getWord();
    auto it = namedValues.find(name);
    if (it == namedValues.end())
      return {nullptr, nullptr};

    Value *ptr = it->second.allocaInst;
    Type *ty = it->second.type;

    auto ensureArrayDepth = [&](Type *t, size_t depth) -> Type * {
      auto *maybeSt = llvm::dyn_cast<llvm::StructType>(t);
      if (maybeSt && TypeResolver::isArray(maybeSt))
        return t;
      for (size_t i = 0; i < depth; ++i) {
        t = TypeResolver::getOrCreateArrayStruct(context, t);
        if (!t)
          return nullptr;
      }
      return t;
    };

    if (it->second.isReference) {
      ptr = builder.CreateLoad(PointerType::get(context, 0), ptr);
      if (it->second.pointeeType)
        ty = it->second.pointeeType;
      ty = ensureArrayDepth(ty, ai->indices.size());
      if (!ty)
        return {nullptr, nullptr};
    } else {
      ty = ensureArrayDepth(ty, ai->indices.size());
      if (!ty)
        return {nullptr, nullptr};
    }

    for (size_t d = 0; d < ai->indices.size(); ++d) {
      Value *idx = codegen(*ai->indices[d]);
      if (!idx)
        return {nullptr, nullptr};
      if (idx->getType()->isIntegerTy(32))
        idx = builder.CreateSExt(idx, Type::getInt64Ty(context));

      auto *arrSt = dyn_cast<StructType>(ty);
      if (!arrSt || !TypeResolver::isArray(arrSt))
        return {nullptr, nullptr};

      Value *loaded = builder.CreateLoad(arrSt, ptr, "arr.load");
      Value *dataPtr = builder.CreateExtractValue(loaded, {1}, "arr.data");
      Type *elemTy = resolveElemType(context, arrSt);
      if (!elemTy)
        return {nullptr, nullptr};

      Value *elemPtr = builder.CreateGEP(elemTy, dataPtr, idx, "elem.ptr");

      if (d == ai->indices.size() - 1) {
        auto *st = llvm::dyn_cast<llvm::StructType>(elemTy);
        if (!st)
          return {nullptr, nullptr};
        for (const auto *sd : structDefs)
          if (sd->name == st->getName().str())
            return {elemPtr, st};
        return {nullptr, nullptr};
      }
      ptr = elemPtr;
      ty = elemTy;
    }
    return {nullptr, nullptr};
  }

  if (auto *fa = dynamic_cast<const FieldAccessExpr *>(&expr)) {
    auto [basePtr, baseSt] = resolveStructPtr(*fa->object);
    if (!basePtr || !baseSt)
      return {nullptr, nullptr};

    const std::string baseStructName = baseSt->getName().str();
    unsigned idx = 0;
    bool found = false;
    Type *fieldTy = nullptr;
    for (const auto &s : structDefs) {
      if (s->name == baseStructName) {
        for (unsigned i = 0; i < s->fields.size(); ++i) {
          if (s->fields[i].name == fa->field) {
            idx = i;
            found = true;
            fieldTy = baseSt->getElementType(i);
            break;
          }
        }
      }
    }
    if (!found || !fieldTy)
      return {nullptr, nullptr};

    Value *gep =
        builder.CreateStructGEP(baseSt, basePtr, idx, fa->field + ".ptr");
    auto *st = llvm::dyn_cast<llvm::StructType>(fieldTy);
    if (!st || TypeResolver::isArray(st))
      return {nullptr, nullptr};
    return {gep, st};
  }

  return {nullptr, nullptr};
}

Value *CodeGenerator::visitStructLit(const StructLitExpr &e) {
  llvm::StructType *st = llvm::StructType::getTypeByName(context, e.typeName);
  if (!st)
    return logError(("Unknown struct type: " + e.typeName).c_str());

  AllocaInst *alloca = builder.CreateAlloca(st, nullptr, e.typeName + ".lit");

  for (unsigned i = 0; i < e.values.size(); ++i) {
    if (i >= st->getNumElements())
      return logError("Too many values in struct literal");

    Value *val = codegen(*e.values[i]);
    if (!val)
      return nullptr;

    Type *fieldTy = st->getElementType(i);

    if (TypeResolver::isString(fieldTy)) {
      if (auto *ai = dyn_cast<AllocaInst>(val)) {
        val = builder.CreateLoad(fieldTy, ai, "field.load");
        llvm::StructType *strSt = TypeResolver::getStringType(context);
        Value *dataGep = builder.CreateStructGEP(strSt, ai, 0, "src.data.ptr");
        builder.CreateStore(
            llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0)),
            dataGep);
      }
    } else if (TypeResolver::isArray(fieldTy)) {
      if (auto *ai = dyn_cast<AllocaInst>(val))
        val = builder.CreateLoad(fieldTy, ai, "field.load");
    } else {
      val = TypeResolver::coerce(builder, val, fieldTy);
    }

    Value *gep = builder.CreateStructGEP(st, alloca, i,
                                         e.typeName + ".f" + std::to_string(i));
    builder.CreateStore(val, gep);
  }
  return alloca;
}

Value *CodeGenerator::visitFieldAssign(const FieldAssignExpr &e) {
  auto [structPtr, st] = resolveStructPtr(*e.object);
  if (!structPtr || !st)
    return logError("Field assign requires a struct expression");

  const std::string structName = st->getName().str();
  unsigned idx = 0;
  bool found = false;
  for (const auto &s : structDefs) {
    if (s->name == structName) {
      for (unsigned i = 0; i < s->fields.size(); ++i) {
        if (s->fields[i].name == e.field) {
          idx = i;
          found = true;
          break;
        }
      }
    }
  }
  if (!found)
    return logError(("Unknown field: " + e.field).c_str());

  Value *val = codegen(*e.value);
  if (!val)
    return nullptr;

  Value *gep = builder.CreateStructGEP(st, structPtr, idx, e.field + ".ptr");
  Type *fieldTy = st->getElementType(idx);

  if (TypeResolver::isArray(fieldTy)) {
    if (auto *ai = dyn_cast<AllocaInst>(val)) {
      if (ai->getAllocatedType() == fieldTy)
        val = builder.CreateLoad(fieldTy, ai, e.field + ".arr.load");
    } else if (val->getType()->isPointerTy()) {
      val = builder.CreateLoad(fieldTy, val, e.field + ".arr.load");
    }
  }

  builder.CreateStore(TypeResolver::coerce(builder, val, fieldTy), gep);
  return val;
}

// --------------------------- //
// StmtVisitor implementations //
// --------------------------- //

llvm::AllocaInst *CodeGenerator::createEntryAlloca(llvm::Type *ty,
                                                   const std::string &name) {
  llvm::Function *fn = builder.GetInsertBlock()->getParent();
  llvm::BasicBlock &entryBB = fn->getEntryBlock();
  llvm::IRBuilder<> tmpB(&entryBB, entryBB.begin());
  return tmpB.CreateAlloca(ty, nullptr, name);
}

Value *CodeGenerator::visitExprStmt(const ExprStmt &s) {
  return s.expr ? codegen(*s.expr) : nullptr;
}

Value *CodeGenerator::visitVarDecl(const VarDecl &d) {
  const std::string name = d.name.token.getWord();
  const std::string typeName = d.type.base.token.getWord();

  Type *ty = TypeResolver::fromTypeDesc(context, d.type);
  if (!ty)
    ty = llvm::StructType::getTypeByName(context, typeName);
  if (!ty)
    return logError(("Unknown type: " + typeName).c_str());

  AllocaInst *alloca = createEntryAlloca(ty, name);
  VarInfo vi(alloca, ty, false, false, false, d.isConst);

  if (!d.initializer) {
    namedValues[name] = vi;
    scopeMgr.declare(name);
    return alloca;
  }

  std::string srcName;
  if (auto *id = dynamic_cast<IdentExpr *>(d.initializer.get()))
    srcName = id->name.token.getWord();

  Value *init = codegen(*d.initializer);
  if (!init)
    return nullptr;

  switch (d.kind) {
  case AssignKind::Copy: {
    if (TypeResolver::isString(ty)) {
      Value *initPtr = init;
      if (!llvm::isa<llvm::AllocaInst>(init) &&
          !init->getType()->isPointerTy()) {
        AllocaInst *tmp = builder.CreateAlloca(ty, nullptr, name + ".init.tmp");
        builder.CreateStore(init, tmp);
        initPtr = tmp;
      }

      Value *empty = StringOps::fromLiteral(builder, context, module.get(), "");
      Value *copy =
          StringOps::concat(builder, context, module.get(), initPtr, empty);
      Value *copyVal = builder.CreateLoad(ty, copy);
      builder.CreateStore(copyVal, alloca);

      llvm::StructType *st = TypeResolver::getStringType(context);
      Value *emptyData = builder.CreateExtractValue(
          builder.CreateLoad(st, empty), {0}, "empty.data");
      builder.CreateCall(getFree(), {emptyData});

      if (srcName.empty()) {
        llvm::AllocaInst *initAI = llvm::dyn_cast<llvm::AllocaInst>(initPtr);
        if (initAI && !scopeMgr.isTmp(initAI)) {
          Value *initData = builder.CreateExtractValue(
              builder.CreateLoad(st, initPtr), {0}, "init.data");
          builder.CreateCall(getFree(), {initData});
        }
      }
      vi.ownsHeap = true;

    } else if (TypeResolver::isArray(ty) ||
               dynamic_cast<const NewArrayExpr *>(d.initializer.get())) {
      bool isNew =
          dynamic_cast<const NewArrayExpr *>(d.initializer.get()) != nullptr;
      if (isNew) {
        if (auto *srcAI = llvm::dyn_cast<llvm::AllocaInst>(init)) {
          Type *realTy = srcAI->getAllocatedType();
          if (realTy != ty) {
            alloca = builder.CreateAlloca(realTy, nullptr, name);
            ty = realTy;
            vi = VarInfo(alloca, ty, false, false, false, d.isConst);
          }
        }
      }
      Value *arrVal = init->getType()->isPointerTy()
                          ? builder.CreateLoad(ty, init, name + ".arr.load")
                          : init;
      builder.CreateStore(arrVal, alloca);
      vi.ownsHeap = isNew;

    } else if (ty->isStructTy()) {
      Value *structVal = init;
      if (init->getType()->isPointerTy())
        structVal = builder.CreateLoad(ty, init, name + ".structval");
      builder.CreateStore(structVal, alloca);
      vi.ownsHeap = false;

      if (dynamic_cast<const StructLitExpr *>(d.initializer.get())) {
        auto *st = llvm::dyn_cast<llvm::StructType>(ty);
        const StructDecl *def = nullptr;
        for (const auto *sd : structDefs)
          if (sd->name == st->getName().str()) {
            def = sd;
            break;
          }

        if (def) {
          for (unsigned i = 0; i < def->fields.size(); ++i) {
            Type *fieldTy = st->getElementType(i);
            if (!TypeResolver::isString(fieldTy))
              continue;
            Value *fieldGep = builder.CreateStructGEP(
                st, alloca, i, name + ".f" + std::to_string(i) + ".own");
            AllocaInst *fieldAlloca = llvm::dyn_cast<AllocaInst>(fieldGep);
            if (fieldAlloca) {
              scopeMgr.declareTmp(fieldAlloca,
                                  TypeResolver::getStringType(context));
            } else {
              AllocaInst *strCopy = builder.CreateAlloca(
                  TypeResolver::getStringType(context), nullptr,
                  name + ".f" + std::to_string(i) + ".destructor");
              Value *fieldVal =
                  builder.CreateLoad(fieldTy, fieldGep, "field.for.dtor");
              builder.CreateStore(fieldVal, strCopy);
              scopeMgr.declareTmp(strCopy,
                                  TypeResolver::getStringType(context));
            }
          }
        }
      }
    } else {
      builder.CreateStore(TypeResolver::coerce(builder, init, ty), alloca);
    }
    break;
  }

  case AssignKind::Move: {
    if (srcName.empty())
      return logError("Move requires identifier");
    auto &srcInfo = namedValues[srcName];
    if (TypeResolver::isString(ty) || TypeResolver::isArray(ty)) {
      Value *val =
          builder.CreateLoad(ty, srcInfo.allocaInst, srcName + ".move");
      builder.CreateStore(val, alloca);
    } else {
      builder.CreateStore(TypeResolver::coerce(builder, init, ty), alloca);
    }
    vi.ownsHeap = srcInfo.ownsHeap;
    srcInfo.ownsHeap = false;
    srcInfo.isMoved = true;
    break;
  }

  case AssignKind::Borrow: {
    if (srcName.empty())
      return logError("Borrow requires identifier");
    auto &srcInfo = namedValues[srcName];
    vi.allocaInst = srcInfo.allocaInst;
    vi.type = srcInfo.type;
    vi.isBorrowed = true;
    vi.isReference = true;
    vi.ownsHeap = false;
    namedValues[name] = vi;
    scopeMgr.declare(name);
    return alloca;
  }
  }

  namedValues[name] = vi;
  scopeMgr.declare(name);
  return alloca;
}

void CodeGenerator::codegen(const Block &block) {
  scopeMgr.pushScope();
  for (const auto &s : block.statements) {
    if (builder.GetInsertBlock()->getTerminator())
      break;
    codegen(*s);
  }
}

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

  builder.SetInsertPoint(thenBB);
  codegen(*s.thenBranch);
  scopeMgr.popScope();
  if (!builder.GetInsertBlock()->getTerminator())
    builder.CreateBr(mergeBB);

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

  loopStack.push_back({condBB, exitBB});
  codegen(*s.doBranch);
  loopStack.pop_back();

  scopeMgr.popScope();
  if (!builder.GetInsertBlock()->getTerminator())
    builder.CreateBr(condBB);

  fn->insert(fn->end(), exitBB);
  builder.SetInsertPoint(exitBB);
  return nullptr;
}

Value *CodeGenerator::visitForRange(const ForRangeStmt &s) {
  Type *varTy = TypeResolver::fromTypeDesc(context, s.varType);
  if (!varTy)
    varTy = llvm::StructType::getTypeByName(context,
                                            s.varType.base.token.getWord());
  if (!varTy)
    return logError("Unknown type in for-range loop variable");

  Value *startVal = codegen(*s.start);
  Value *endVal = codegen(*s.end);
  Value *stepVal = codegen(*s.step);
  if (!startVal || !endVal || !stepVal)
    return nullptr;

  startVal = TypeResolver::coerce(builder, startVal, varTy);
  endVal = TypeResolver::coerce(builder, endVal, varTy);
  stepVal = TypeResolver::coerce(builder, stepVal, varTy);

  const std::string &vname = s.varName.token.getWord();
  AllocaInst *varAlloca = createEntryAlloca(varTy, vname);
  builder.CreateStore(startVal, varAlloca);

  VarInfo vi(varAlloca, varTy, false, false, false, s.varType.isConst);
  namedValues[vname] = vi;

  llvm::Function *fn = builder.GetInsertBlock()->getParent();
  BasicBlock *condBB = BasicBlock::Create(context, "for.cond", fn);
  BasicBlock *bodyBB = BasicBlock::Create(context, "for.body");
  BasicBlock *stepBB = BasicBlock::Create(context, "for.step");
  BasicBlock *exitBB = BasicBlock::Create(context, "for.exit");

  builder.CreateBr(condBB);

  builder.SetInsertPoint(condBB);
  Value *cur = builder.CreateLoad(varTy, varAlloca, vname + ".cur");

  Value *cond;
  bool isFloat = varTy->isFloatingPointTy();
  if (isFloat) {
    cond = builder.CreateFCmpOLT(cur, endVal, "for.cond.f");
  } else {
    if (auto *cs = llvm::dyn_cast<llvm::ConstantInt>(stepVal)) {
      if (cs->getSExtValue() >= 0)
        cond = builder.CreateICmpSLT(cur, endVal, "for.cond.lt");
      else
        cond = builder.CreateICmpSGT(cur, endVal, "for.cond.gt");
    } else {
      Value *zero = ConstantInt::get(varTy, 0);
      Value *stepPos = builder.CreateICmpSGT(stepVal, zero, "step.pos");
      Value *ltEnd = builder.CreateICmpSLT(cur, endVal, "cur.lt.end");
      Value *gtEnd = builder.CreateICmpSGT(cur, endVal, "cur.gt.end");
      cond = builder.CreateSelect(stepPos, ltEnd, gtEnd, "for.cond.dyn");
    }
  }
  builder.CreateCondBr(cond, bodyBB, exitBB);

  fn->insert(fn->end(), bodyBB);
  builder.SetInsertPoint(bodyBB);

  loopStack.push_back({stepBB, exitBB});
  codegen(*s.body);
  loopStack.pop_back();

  scopeMgr.popScope();
  if (!builder.GetInsertBlock()->getTerminator())
    builder.CreateBr(stepBB);

  fn->insert(fn->end(), stepBB);
  builder.SetInsertPoint(stepBB);
  Value *oldVal = builder.CreateLoad(varTy, varAlloca, vname + ".old");
  Value *newVal = isFloat ? builder.CreateFAdd(oldVal, stepVal, "for.step.f")
                          : builder.CreateAdd(oldVal, stepVal, "for.step.i");
  builder.CreateStore(newVal, varAlloca);
  builder.CreateBr(condBB);

  fn->insert(fn->end(), exitBB);
  builder.SetInsertPoint(exitBB);

  namedValues.erase(vname);
  return nullptr;
}

Value *CodeGenerator::visitBreak(const Break &) {
  if (loopStack.empty())
    return logError("'break' outside loop");
  builder.CreateBr(loopStack.back().exitBB);
  return nullptr;
}

Value *CodeGenerator::visitContinue(const Continue &) {
  if (loopStack.empty())
    return logError("'continue' outside loop");
  builder.CreateBr(loopStack.back().condBB);
  return nullptr;
}

Value *CodeGenerator::visitReturn(const Return &s) {
  Value *retVal = nullptr;
  if (s.value)
    retVal = codegen(**s.value);

  scopeMgr.emitAllDestructors();

  if (retVal) {
    llvm::Function *fn = builder.GetInsertBlock()->getParent();
    Type *retTy = fn->getReturnType();

    if (retVal->getType()->isPointerTy() &&
        (TypeResolver::isString(retTy) || TypeResolver::isArray(retTy) ||
         retTy->isStructTy())) {
      retVal = builder.CreateLoad(retTy, retVal, "ret.load");
    } else {
      retVal = TypeResolver::coerce(builder, retVal, retTy);
    }
    builder.CreateRet(retVal);
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
    retTy = llvm::StructType::getTypeByName(
        context, func.returnType.base.token.getWord());
  if (!retTy)
    return nullptr;

  std::vector<Type *> paramTypes;
  std::vector<bool> paramIsRef;
  for (const auto &p : func.params) {
    Type *pt = TypeResolver::fromTypeDesc(context, p.type);
    if (!pt)
      pt =
          llvm::StructType::getTypeByName(context, p.type.base.token.getWord());
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

  if (fname == "main")
    BuiltinEmitter::emitRuntimeInit(builder, context, module.get());

  namedValues.clear();
  namedValues = globalValues;
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
      if (!pointee)
        pointee = llvm::StructType::getTypeByName(
            context, param.type.base.token.getWord());
      VarInfo vi(ptrAlloca, PointerType::get(context, 0), false, false, true,
                 param.isConst);
      vi.pointeeType = pointee;
      namedValues[pname] = vi;
    } else {
      Type *declaredTy = TypeResolver::fromTypeDesc(context, param.type);
      if (!declaredTy)
        declaredTy = llvm::StructType::getTypeByName(
            context, param.type.base.token.getWord());
      if (!declaredTy)
        declaredTy = arg.getType();

      AllocaInst *a = builder.CreateAlloca(declaredTy, nullptr, pname);
      builder.CreateStore(&arg, a);
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
    else if (retTy->isFloatingPointTy())
      builder.CreateRet(llvm::ConstantFP::get(retTy, 0.0));
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
  structDefs.clear();
  for (const auto &s : program.structs)
    structDefs.push_back(s.get());

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

  for (const auto &s : program.structs) {
    if (!llvm::StructType::getTypeByName(context, s->name))
      llvm::StructType::create(context, s->name);
  }

  for (const auto &s : program.structs) {
    llvm::StructType *st = llvm::StructType::getTypeByName(context, s->name);
    if (!st || !st->isOpaque())
      continue;

    std::vector<llvm::Type *> fieldTypes;
    bool allResolved = true;
    for (const auto &f : s->fields) {
      llvm::Type *ft = TypeResolver::fromTypeDesc(context, f.type);
      if (!ft)
        ft = llvm::StructType::getTypeByName(context,
                                             f.type.base.token.getWord());
      if (!ft) {
        errs() << "CodeGen error: cannot resolve type '"
               << f.type.base.token.getWord() << "' for field '" << f.name
               << "' in struct '" << s->name << "'\n";
        allResolved = false;
        break;
      }
      fieldTypes.push_back(ft);
    }
    if (allResolved)
      st->setBody(fieldTypes);
  }

  for (const auto &block : program.externBlocks) {
    for (const auto &decl : block.decls) {
      if (module->getFunction(decl.name))
        continue;
      std::vector<llvm::Type *> pts;
      for (const auto &p : decl.paramTypes) {
        const std::string tname = p.base.token.getWord();
        if (p.isPtr || p.dimensions > 0 || tname == "str" || tname == "string")
          pts.push_back(llvm::PointerType::get(context, 0));
        else
          pts.push_back(TypeResolver::fromName(context, tname));
      }
      auto *retTy =
          TypeResolver::fromName(context, decl.returnType.base.token.getWord());
      auto *ft = llvm::FunctionType::get(retTy, pts, false);
      llvm::Function::Create(ft, llvm::Function::ExternalLinkage, decl.name,
                             *module);
    }
  }

  for (const auto &fn : program.functions) {
    const std::string fname = normalizeFunctionName(fn->name.token.getWord());
    if (!module->getFunction(fname)) {
      Type *retTy = TypeResolver::fromTypeDesc(context, fn->returnType);
      if (!retTy)
        retTy = llvm::Type::getVoidTy(context);

      std::vector<Type *> paramTypes;
      for (const auto &p : fn->params) {
        Type *pt = TypeResolver::fromTypeDesc(context, p.type);
        if (!pt)
          continue;
        paramTypes.push_back(p.isBorrowRef ? PointerType::get(context, 0) : pt);
      }
      auto *ft = FunctionType::get(retTy, paramTypes, false);
      llvm::Function::Create(ft, llvm::Function::ExternalLinkage, fname,
                             *module);
    }
  }

  for (const auto &gv : program.globals) {
    llvm::Type *ty = TypeResolver::fromTypeDesc(context, gv->type);
    if (!ty) {
      logError(("Global variable '" + gv->name + "': unknown type '" +
                gv->type.base.token.getWord() + "'")
                   .c_str());
      return false;
    }

    auto evalConstField = [&](const Expression *expr,
                              llvm::Type *fieldTy) -> llvm::Constant * {
      bool negate = false;
      if (auto *unary = dynamic_cast<const UnaryExpr *>(expr)) {
        if (unary->op != UnaryOp::Negate) {
          logError(("Global '" + gv->name + "': unsupported unary op in field")
                       .c_str());
          return nullptr;
        }
        negate = true;
        expr = unary->operand.get();
      }

      llvm::Constant *fc = nullptr;
      if (auto *fi = dynamic_cast<const IntLitExpr *>(expr)) {
        if (fieldTy->isFloatingPointTy()) {
          double v = std::stod(fi->lit.getWord());
          if (negate)
            v = -v;
          fc = llvm::ConstantFP::get(fieldTy, v);
        } else {
          long long v = std::stoll(fi->lit.getWord());
          if (negate)
            v = -v;
          fc = llvm::ConstantInt::get(fieldTy, v);
        }
      } else if (auto *ff = dynamic_cast<const FloatLitExpr *>(expr)) {
        double v = std::stod(ff->lit.getWord());
        if (negate)
          v = -v;
        fc = llvm::ConstantFP::get(fieldTy, v);
      } else if (auto *fb = dynamic_cast<const BoolLitExpr *>(expr)) {
        long long v = fb->lit.getWord() == "true" ? 1 : 0;
        if (negate)
          v = -v;
        fc = llvm::ConstantInt::get(fieldTy, v);
      } else {
        logError(
            ("Global '" + gv->name + "': field is not a constant expression")
                .c_str());
        return nullptr;
      }

      if (fc && fieldTy->isFloatTy() && fc->getType()->isDoubleTy())
        fc = llvm::ConstantFP::get(
            fieldTy,
            llvm::cast<llvm::ConstantFP>(fc)->getValueAPF().convertToDouble());

      return fc;
    };

    llvm::Constant *init = nullptr;

    if (auto *intExpr = dynamic_cast<IntLitExpr *>(gv->init.get())) {
      if (ty->isFloatingPointTy()) {
        double val = std::stod(intExpr->lit.getWord());
        init =
            ty->isFloatTy()
                ? llvm::ConstantFP::get(llvm::Type::getFloatTy(context), val)
                : llvm::ConstantFP::get(llvm::Type::getDoubleTy(context), val);
      } else {
        init = llvm::ConstantInt::get(ty, std::stoll(intExpr->lit.getWord()));
      }
    } else if (auto *fltExpr = dynamic_cast<FloatLitExpr *>(gv->init.get())) {
      double val = std::stod(fltExpr->lit.getWord());
      init = ty->isFloatTy()
                 ? llvm::ConstantFP::get(llvm::Type::getFloatTy(context), val)
                 : llvm::ConstantFP::get(llvm::Type::getDoubleTy(context), val);
    } else if (auto *slExpr = dynamic_cast<StructLitExpr *>(gv->init.get())) {
      auto *st = llvm::dyn_cast<llvm::StructType>(ty);
      if (!st) {
        logError(("Global '" + gv->name + "': type is not a struct").c_str());
        return false;
      }
      if (slExpr->values.size() != st->getNumElements()) {
        logError(("Global '" + gv->name +
                  "': wrong number of fields in struct literal")
                     .c_str());
        return false;
      }
      std::vector<llvm::Constant *> fieldConsts;
      for (unsigned i = 0; i < slExpr->values.size(); ++i) {
        llvm::Type *fieldTy = st->getElementType(i);
        llvm::Constant *fc = evalConstField(slExpr->values[i].get(), fieldTy);
        if (!fc)
          return false;
        fieldConsts.push_back(fc);
      }
      init = llvm::ConstantStruct::get(st, fieldConsts);
    }

    if (!init) {
      logError(("Global variable '" + gv->name +
                "' must have a constant initializer")
                   .c_str());
      return false;
    }

    auto *gVar = new llvm::GlobalVariable(*module, ty, gv->isConst,
                                          llvm::GlobalValue::ExternalLinkage,
                                          init, gv->name);

    VarInfo vi(gVar, ty, false, false, false, gv->isConst);
    namedValues[gv->name] = vi;
    globalValues[gv->name] = vi;
  }

  for (const auto &fn : program.functions)
    if (!codegen(*fn))
      return false;

  std::error_code ec;
  raw_fd_ostream out(outputFilename + ".ll", ec, sys::fs::OF_None);
  if (ec) {
    errs() << "Cannot open output file: " << ec.message() << "\n";
    return false;
  }
  module->print(out, nullptr);
  return true;
}
