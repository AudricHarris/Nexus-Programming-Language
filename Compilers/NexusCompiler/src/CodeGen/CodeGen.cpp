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
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Type.h>
#include <memory>
#include <string>
#include <unordered_set>

using namespace llvm;

static bool blockHasTerminator(llvm::IRBuilder<> &B) {
  llvm::BasicBlock *bb = B.GetInsertBlock();
  if (!bb || bb->empty())
    return false;
  return bb->back().isTerminator();
}

/*---------------------------------------*/
/*             Utilities                 */
/*---------------------------------------*/

/**
 * Maps user-facing function names to their canonical runtime equivalents.
 * Names not present in the table are returned unchanged.
 * @param name the raw function name from the AST
 * @return the normalised name used in the emitted IR
 */
static std::string normalizeFunctionName(const std::string &name) {
  static const std::unordered_map<std::string, std::string> kMap = {
      {"Main", "main"},     {"Printf", "printf"}, {"Print", "print_literal"},
      {"printf", "printf"}, {"Read", "read"},     {"Random", "random"},
  };
  auto it = kMap.find(name);
  return it != kMap.end() ? it->second : name;
}

/**
 * Returns true for LLVM types that must be passed by pointer across call
 * boundaries (structs, strings, arrays). Scalars are passed by value.
 * @param pt the LLVM type to test
 * @return true if the type should be transmitted as a pointer argument
 */
static bool passAsPointer(llvm::Type *pt) {
  return TypeResolver::isString(pt) || TypeResolver::isArray(pt) ||
         pt->isStructTy();
}

/*---------------------------------------*/
/*    String interpolation in StrLit     */
/*---------------------------------------*/

/**
 * Generates IR for a string literal, handling both plain strings and
 * interpolated strings of the form "Hello {name}!".
 *
 * Plain strings are interned as global constants and returned directly.
 * Interpolated strings are decomposed into segments: each plain-text chunk
 * becomes a constant, each {expr} slot is evaluated and converted to a
 * string, then all segments are concatenated with StringOps::concat().
 *
 * Ownership rules: every temporary alloca produced here is registered with
 * the scope manager so its heap buffer is freed at scope exit.
 *
 * @param e the string-literal AST node
 * @return the LLVM Value* holding the resulting string (an AllocaInst*)
 */
Value *CodeGenerator::visitStrLit(const StrLitExpr &e) {
  std::string raw = codegen_utils::unescapeString(e.lit.getWord());

  // Check whether the string contains any interpolation slots {expr}.
  bool hasInterp = false;
  for (size_t i = 0; i < raw.size(); ++i) {
    if (raw[i] == '\\' && i + 1 < raw.size() && raw[i + 1] == '{') {
      ++i; // Escaped brace — not an interpolation slot.
      continue;
    }
    if (raw[i] == '{' && (i + 1 >= raw.size() || raw[i + 1] != '{')) {
      hasInterp = true;
      break;
    }
  }

  // --- Plain string (no interpolation) ---
  if (!hasInterp) {
    // Collapse {{ → { and translate hex colour escapes, then intern.
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
    Value *v =
        StringOps::fromLiteral(builder, context, module.get(), processed);
    if (auto *ai = llvm::dyn_cast<llvm::AllocaInst>(v))
      scopeMgr.declareTmp(ai, TypeResolver::getStringType(context));
    return v;
  }

  // --- Interpolated string ---
  // Temporaries produced here are registered for destruction at scope exit.
  auto registerTmp = [&](Value *v) {
    if (auto *ai = llvm::dyn_cast<llvm::AllocaInst>(v)) {
      llvm::Function *currentFn = builder.GetInsertBlock()->getParent();
      if (ai->getParent()->getParent() == currentFn)
        scopeMgr.declareTmp(ai, TypeResolver::getStringType(context));
    }
  };

  size_t i = 0;
  std::string chunk;
  Value *result = nullptr;

  // Appends a value segment to the running concatenation result.
  auto appendValue = [&](Value *v) {
    if (!result) {
      result = v;
    } else {
      Value *cat = StringOps::concat(builder, context, module.get(), result, v);
      registerTmp(cat);
      result = cat;
    }
  };

  // Flushes the current plain-text chunk as a string literal segment.
  auto flushChunk = [&]() {
    if (!chunk.empty()) {
      Value *c = StringOps::fromLiteral(builder, context, module.get(), chunk);
      registerTmp(c);
      appendValue(c);
      chunk.clear();
    }
  };

  while (i < raw.size()) {
    // \{ — escaped opening brace, treat as literal '{'.
    if (raw[i] == '\\' && i + 1 < raw.size() && raw[i + 1] == '{') {
      chunk += '{';
      i += 2;
      continue;
    }

    // {{ — escaped double-brace, treat as literal '{'.
    if (raw[i] == '{' && i + 1 < raw.size() && raw[i + 1] == '{') {
      chunk += '{';
      i += 2;
      continue;
    }

    // Regular character outside an interpolation slot.
    if (raw[i] != '{') {
      chunk += raw[i++];
      continue;
    }

    // Opening '{' found — scan for the matching '}', respecting nesting.
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

    // Unmatched brace — treat as literal text.
    if (j >= raw.size()) {
      chunk += raw[i++];
      continue;
    }

    std::string inner = raw.substr(start, j - start);
    Value *val =
        codegen_utils::evalInterp(inner, context, builder, namedValues);
    if (val) {
      flushChunk();
      Value *sv = StringOps::fromValue(builder, context, module.get(), val);
      registerTmp(sv);
      appendValue(sv);
    } else {
      chunk += '{' + inner + '}';
    }
    i = j + 1;
  }
  flushChunk();

  // Edge case: the format string contained only interpolation slots that all
  // failed to evaluate, yielding an empty result.
  if (!result) {
    Value *empty = StringOps::fromLiteral(builder, context, module.get(), "");
    registerTmp(empty);
    return empty;
  }
  return result;
}

/*---------------------------------------*/
/*           Constructor / init          */
/*---------------------------------------*/

/**
 * Constructs the CodeGenerator and initialises LLVM state.
 * The module is named "nexus" and its target triple is set to the host
 * platform so the emitted IR can be compiled without further flags.
 */
CodeGenerator::CodeGenerator()
    : module(std::make_unique<Module>("nexus", context)), builder(context),
      scopeMgr(builder, context, module.get(), namedValues) {
  module->setTargetTriple(Triple(LLVM_HOST_TRIPLE));
}

/**
 * Returns true if the given LLVM type is a pointer type.
 * Used by callers that need to distinguish pointer from value types without
 * importing TypeResolver directly.
 * @param ty the LLVM type to check
 */
bool CodeGenerator::isCStringPointer(Type *ty) { return ty->isPointerTy(); }

/**
 * Lazily declares the C runtime 'free' function in the current module
 * and returns a pointer to it. Subsequent calls return the cached function.
 * @return the llvm::Function* for free(void*)
 */
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

/**
 * Prints a red diagnostic message to stderr and returns nullptr.
 * All codegen methods return nullptr on error, which propagates upward.
 * @param msg the human-readable error description
 * @return always nullptr
 */
Value *CodeGenerator::logError(const char *msg) {
  errs() << "\033[31mCodeGen error: " << msg << "\033[0m\n";
  return nullptr;
}

/*---------------------------------------*/
/*    Dispatch — visitor entry points    */
/*---------------------------------------*/

/**
 * Dispatches code generation for an expression node via the visitor pattern.
 * @param expr the expression AST node to lower
 * @return the LLVM Value* representing the expression result, or nullptr
 */
Value *CodeGenerator::codegen(const Expression &expr) {
  return expr.accept(*this);
}

/**
 * Dispatches code generation for a statement node via the visitor pattern.
 * @param stmt the statement AST node to lower
 * @return the LLVM Value* produced by the statement (often nullptr)
 */
Value *CodeGenerator::codegen(const Statement &stmt) {
  return stmt.accept(*this);
}

/**
 * Thin wrapper that forwards to visitBlock.
 * @param block the block AST node to lower
 * @return the value produced by the last statement in the block
 */
Value *CodeGenerator::codegen(const Block &block) { return visitBlock(block); }

/*---------------------------------------*/
/*     ExprVisitor implementations       */
/*---------------------------------------*/

/**
 * Generates a 32-bit integer constant.
 * @param e the integer literal AST node
 * @return an llvm::ConstantInt with i32 type
 */
Value *CodeGenerator::visitIntLit(const IntLitExpr &e) {
  return ConstantInt::get(Type::getInt32Ty(context),
                          std::stoll(e.lit.getWord()));
}

/**
 * Generates a 32-bit floating-point constant.
 * @param e the float literal AST node
 * @return an llvm::ConstantFP with float type
 */
Value *CodeGenerator::visitFloatLit(const FloatLitExpr &e) {
  return ConstantFP::get(Type::getFloatTy(context), std::stod(e.lit.getWord()));
}

/**
 * Generates a 1-bit boolean constant (0 or 1).
 * @param e the boolean literal AST node ("true" or "false")
 * @return an llvm::ConstantInt with i1 type
 */
Value *CodeGenerator::visitBoolLit(const BoolLitExpr &e) {
  return ConstantInt::get(Type::getInt1Ty(context),
                          e.lit.getWord() == "true" ? 1 : 0);
}

/**
 * Generates an 8-bit integer constant from a character literal.
 * Supports the standard C escape sequences (\n, \t, \r, \0, \\, \').
 * @param e the character literal AST node
 * @return an llvm::ConstantInt with i8 type
 */
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

/**
 * Generates a typed null pointer constant.
 * @return an llvm::ConstantPointerNull for the opaque pointer type
 */
Value *CodeGenerator::visitNullLit(const NullLitExpr &) {
  return llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0));
}

/**
 * Loads the value of a named variable from its alloca slot.
 * Handles three cases:
 *  - Reference variables: loads the stored pointer then dereferences it.
 *  - Aggregate variables (strings, arrays): returns the alloca directly.
 *  - Scalar variables: emits a plain load instruction.
 * @param e the identifier expression naming the variable
 * @return the LLVM Value* holding the variable's current value
 */
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
    // Aggregate references are returned as raw pointers; scalars are loaded.
    if (TypeResolver::isString(pointee) || TypeResolver::isArray(pointee))
      return ptr;
    return builder.CreateLoad(pointee, ptr, name + ".deref");
  }

  // Aggregate types are always handled as pointers to their alloca storage.
  // Structs are included here so that visitReturn's dyn_cast<AllocaInst>
  // succeeds and can null out owned string/array field data pointers before
  // scope cleanup runs — preventing a use-after-free on struct return.
  if (TypeResolver::isString(it->second.type) ||
      TypeResolver::isArray(it->second.type) || it->second.type->isStructTy())
    return it->second.allocaInst;

  return builder.CreateLoad(it->second.type, it->second.allocaInst,
                            name + ".load");
}

/*---------------------------------------*/
/*    Binary expressions (arithmetic /   */
/*            comparison)                */
/*---------------------------------------*/

/**
 * Generates IR for a binary expression (arithmetic, comparison, or string op).
 *
 * The method resolves the semantic types of both operands and handles the
 * following special cases before delegating to ArithmeticManager:
 *  - String concatenation: any Add where at least one side is a string.
 *  - String equality: Eq/Ne between two strings uses strcmp.
 *  - Numeric promotion: int/float mismatches are widened to a common type.
 *  - Null comparisons: integer literal 0 beside a pointer becomes nullptr.
 *
 * @param expr the binary expression AST node
 * @return the LLVM Value* of the expression result, or nullptr on error
 */
Value *CodeGenerator::visitBinary(const BinaryExpr &expr) {
  Value *lhs = codegen(*expr.left);
  Value *rhs = codegen(*expr.right);
  if (!lhs || !rhs)
    return nullptr;

  // Resolves the declared type of a value, preferring the symbol table entry
  // over the IR type (which may already be a pointer for aggregates).
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
  auto liftEnumToTag = [&](Value *&val, Type *&ty, Value *other) {
    // If `val` is a pointer to an enum struct (has i32 tag at field 0)
    // and `other` is an integer, extract the tag and use that instead.
    if (!ty->isStructTy())
      return;
    if (other->getType()->isIntegerTy() &&
        cast<StructType>(ty)->getNumElements() >= 1 &&
        cast<StructType>(ty)->getElementType(0)->isIntegerTy(32)) {
      if (auto *ai = dyn_cast<AllocaInst>(val)) {
        llvm::StructType *tagView = llvm::StructType::get(
            context, {llvm::Type::getInt32Ty(context)}, false);
        Value *tagPtr = builder.CreateStructGEP(tagView, ai, 0, "eq.tag.ptr");
        val = builder.CreateLoad(llvm::Type::getInt32Ty(context), tagPtr,
                                 "eq.tag");
        ty = llvm::Type::getInt32Ty(context);
      }
    }
  };

  if (expr.op == BinaryOp::Eq || expr.op == BinaryOp::Ne) {
    liftEnumToTag(lhs, lTy, rhs);
    liftEnumToTag(rhs, rTy, lhs);
  }

  bool lIsEnum = lTy->isStructTy() && !TypeResolver::isString(lTy) &&
                 !TypeResolver::isArray(lTy);
  bool rIsEnum = rTy->isStructTy() && !TypeResolver::isString(rTy) &&
                 !TypeResolver::isArray(rTy);
  if ((expr.op == BinaryOp::Eq || expr.op == BinaryOp::Ne) && lIsEnum &&
      rIsEnum) {
    auto extractTag = [&](Value *v, Type *t) -> Value * {
      auto *ai = dyn_cast<AllocaInst>(v);
      if (!ai)
        return nullptr;
      auto *st = dyn_cast<StructType>(t);
      if (!st || st->getNumElements() == 0)
        return nullptr;
      llvm::StructType *tagView = llvm::StructType::get(
          context, {llvm::Type::getInt32Ty(context)}, false);
      Value *ptr = builder.CreateStructGEP(tagView, ai, 0, "etag.ptr");
      return builder.CreateLoad(llvm::Type::getInt32Ty(context), ptr, "etag");
    };
    Value *lTag = extractTag(lhs, lTy);
    Value *rTag = extractTag(rhs, rTy);
    if (lTag && rTag) {
      Value *cmp = (expr.op == BinaryOp::Eq)
                       ? builder.CreateICmpEQ(lTag, rTag, "enum.eq")
                       : builder.CreateICmpNE(lTag, rTag, "enum.ne");
      return cmp;
    }
  }

  // String concatenation: if either operand is a string, coerce the other.
  bool lIsStr = TypeResolver::isString(lTy);
  bool rIsStr = TypeResolver::isString(rTy);

  if (expr.op == BinaryOp::Add && (lIsStr || rIsStr)) {
    if (!lIsStr)
      lhs = StringOps::fromValue(builder, context, module.get(), lhs);
    if (!rIsStr)
      rhs = StringOps::fromValue(builder, context, module.get(), rhs);
    return StringOps::concat(builder, context, module.get(), lhs, rhs);
  }

  // String equality / inequality via strcmp.
  if (lIsStr && rIsStr) {
    switch (expr.op) {
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

  // Numeric type promotion before delegating to ArithmeticManager.
  Type *lValTy = lhs->getType();
  Type *rValTy = rhs->getType();

  if (lValTy->isFloatingPointTy() && rValTy->isIntegerTy())
    rhs = builder.CreateSIToFP(rhs, lValTy, "itof");
  else if (rValTy->isFloatingPointTy() && lValTy->isIntegerTy())
    lhs = builder.CreateSIToFP(lhs, rValTy, "itof");
  else if (lValTy->isFloatingPointTy() && rValTy->isFloatingPointTy() &&
           lValTy != rValTy) {
    // Widen the narrower floating-point type to match the wider one.
    Type *wide =
        lValTy->getPrimitiveSizeInBits() >= rValTy->getPrimitiveSizeInBits()
            ? lValTy
            : rValTy;
    if (lValTy != wide)
      lhs = builder.CreateFPExt(lhs, wide, "fpext");
    if (rValTy != wide)
      rhs = builder.CreateFPExt(rhs, wide, "fpext");
  }

  // Allow null comparisons: integer constant 0 on one side becomes nullptr.
  if (expr.op == BinaryOp::Eq || expr.op == BinaryOp::Ne) {
    auto liftNullPtr = [&](Value *&val, Value *other) {
      if (other->getType()->isPointerTy() && val->getType()->isIntegerTy()) {
        if (auto *ci = llvm::dyn_cast<llvm::ConstantInt>(val))
          if (ci->isZero())
            val = llvm::ConstantPointerNull::get(
                llvm::PointerType::get(context, 0));
      }
    };
    liftNullPtr(rhs, lhs);
    liftNullPtr(lhs, rhs);
  }

  Value *result =
      ArithmeticManager::emitBinaryOp(builder, context, expr.op, lhs, rhs);
  if (!result)
    return logError("Unsupported binary operator");
  return result;
}

/**
 * Generates IR for a unary expression (arithmetic negation or logical NOT).
 * @param e the unary expression AST node
 * @return the LLVM Value* of the negated / inverted operand, or nullptr
 */
Value *CodeGenerator::visitUnary(const UnaryExpr &e) {
  Value *v = codegen(*e.operand);
  if (!v)
    return nullptr;
  switch (e.op) {
  case UnaryOp::Negate:
    return v->getType()->isFloatingPointTy() ? builder.CreateFNeg(v, "fneg")
                                             : builder.CreateNeg(v, "neg");
  case UnaryOp::Not: {
    // Coerce non-boolean to i1 before inverting.
    Value *b = v->getType()->isIntegerTy(1)
                   ? v
                   : builder.CreateICmpNE(v, ConstantInt::get(v->getType(), 0),
                                          "tobool");
    return builder.CreateNot(b, "lnot");
  }
  }
  return logError("Unknown unary op");
}

/*---------------------------------------*/
/*        Assignment expressions         */
/*---------------------------------------*/

/**
 * Generates IR for a simple assignment (copy, move, or borrow).
 *
 * Copy: stores the new value into the target's alloca.
 *   - String copy: frees the old heap buffer first to avoid leaks.
 * Move: transfers ownership and marks the source as moved.
 * Borrow: rebinds the target's VarInfo to alias the source's alloca.
 *
 * @param e the assignment expression AST node
 * @return the assigned LLVM Value*, or nullptr on error
 */
Value *CodeGenerator::visitAssign(const AssignExpr &e) {
  const std::string &tgt = e.target.token.getWord();
  auto it = namedValues.find(tgt);
  if (it == namedValues.end())
    return logError(("Undeclared variable: " + tgt).c_str());
  if (it->second.isConst)
    return logError(("Cannot reassign const variable: " + tgt + " on line " +
                     std::to_string(e.target.token.getLine()))
                        .c_str());

  // Reference target: store through the stored pointer.
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

  // String assignment: free the old heap buffer first, then store the new one.
  // The source's data pointer is nulled to prevent a double-free.
  if (TypeResolver::isString(targetTy)) {
    Value *oldVal =
        builder.CreateLoad(targetTy, it->second.allocaInst, tgt + ".old");
    Value *oldData = builder.CreateExtractValue(oldVal, {0}, "old.data");
    builder.CreateCall(getFree(), {oldData});

    Value *loaded = builder.CreateLoad(targetTy, val);
    builder.CreateStore(loaded, it->second.allocaInst);

    // Null the source's data pointer to prevent a double-free.
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

/**
 * Generates IR for a post-increment expression (variable++).
 * @param e the increment AST node
 * @return the incremented value, or nullptr on error
 */
Value *CodeGenerator::visitIncrement(const Increment &e) {
  return generateIncrDecr(e.target.token.getWord(), true);
}

/**
 * Generates IR for a post-decrement expression (variable--).
 * @param e the decrement AST node
 * @return the decremented value, or nullptr on error
 */
Value *CodeGenerator::visitDecrement(const Decrement &e) {
  return generateIncrDecr(e.target.token.getWord(), false);
}

/**
 * Shared implementation for ++ and --. Handles both reference and direct
 * variables, and works for both integer and floating-point types.
 * @param name the name of the variable to modify
 * @param isInc true for increment, false for decrement
 * @return the updated value, or nullptr on error
 */
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

/**
 * Generates IR for a compound assignment (+=, -=, *=, /=, etc.).
 * Handles string += by concatenating in-place and freeing the old buffer.
 * For numeric types, promotes the RHS to match the target before delegating
 * to ArithmeticManager, then coerces the result back if needed.
 * @param e the compound-assignment expression AST node
 * @return the updated value, or nullptr on error
 */
Value *CodeGenerator::visitCompoundAssign(const CompoundAssignExpr &e) {
  const std::string &name = e.target.token.getWord();
  auto it = namedValues.find(name);
  if (it == namedValues.end())
    return logError(("Unknown variable: " + name).c_str());
  if (it->second.isConst)
    return logError(
        ("Cannot compound-assign to const variable: " + name).c_str());

  Type *targetTy = it->second.type;

  // String += : concatenate in-place, replacing the heap buffer.
  if (TypeResolver::isString(targetTy)) {
    if (e.op != BinaryOp::Add)
      return logError(
          ("Compound operator not supported on string: " + name).c_str());

    Value *rhs = codegen(*e.value);
    if (!rhs)
      return nullptr;

    // Determine whether the RHS is already a string or needs conversion.
    Type *rhsTy = nullptr;
    if (auto *ai = llvm::dyn_cast<llvm::AllocaInst>(rhs))
      rhsTy = ai->getAllocatedType();
    else
      rhsTy = rhs->getType();

    Value *rhsStr =
        TypeResolver::isString(rhsTy)
            ? rhs
            : StringOps::fromValue(builder, context, module.get(), rhs);

    Value *concat = StringOps::concat(builder, context, module.get(),
                                      it->second.allocaInst, rhsStr);

    // Free the old buffer before installing the new one.
    llvm::StructType *strSt = TypeResolver::getStringType(context);
    Value *oldVal =
        builder.CreateLoad(strSt, it->second.allocaInst, name + ".old");
    Value *oldData = builder.CreateExtractValue(oldVal, {0}, "old.data");
    builder.CreateCall(getFree(), {oldData});

    Value *newVal = builder.CreateLoad(strSt, concat, "concat.val");
    builder.CreateStore(newVal, it->second.allocaInst);

    // Null the temporary concat buffer's data pointer to avoid double-free.
    if (auto *ai = llvm::dyn_cast<llvm::AllocaInst>(concat)) {
      Value *dataField = builder.CreateStructGEP(strSt, ai, 0, "tmp.data.ptr");
      builder.CreateStore(
          llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0)),
          dataField);
    }
    return it->second.allocaInst;
  }

  Value *cur =
      builder.CreateLoad(targetTy, it->second.allocaInst, name + ".load");

  Value *rhs = codegen(*e.value);
  if (!rhs)
    return nullptr;

  // Promote types to match the target before the arithmetic operation.
  Type *curTy = cur->getType();
  Type *rhsTy = rhs->getType();
  if (curTy->isFloatingPointTy() && rhsTy->isIntegerTy())
    rhs = builder.CreateSIToFP(rhs, curTy, "itof");
  else if (curTy->isIntegerTy() && rhsTy->isFloatingPointTy())
    rhs = builder.CreateFPToSI(rhs, curTy, "ftoi");
  else if (curTy->isFloatingPointTy() && rhsTy->isFloatingPointTy() &&
           curTy != rhsTy) {
    rhs = curTy->getPrimitiveSizeInBits() > rhsTy->getPrimitiveSizeInBits()
              ? builder.CreateFPExt(rhs, curTy, "fpext")
              : builder.CreateFPTrunc(rhs, curTy, "fptrunc");
  }

  Value *result =
      ArithmeticManager::emitBinaryOp(builder, context, e.op, cur, rhs);
  if (!result)
    return logError(
        ("Unsupported compound operator on variable: " + name).c_str());

  // Coerce result back to the declared target type if promotion widened it.
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

/*---------------------------------------*/
/*             Array types               */
/*---------------------------------------*/

/**
 * Resolves the element type of an array struct.
 * Falls back to a name-based lookup for user-defined struct elements that
 * TypeResolver does not recognise by primitive name alone.
 * @param context the LLVM context
 * @param arrSt the array struct type (e.g. "array.i32")
 * @return the element LLVM type, or nullptr if it cannot be determined
 */
static Type *resolveElemType(llvm::LLVMContext &context,
                             llvm::StructType *arrSt) {
  Type *elemTy = TypeResolver::elemType(context, arrSt);
  if (elemTy)
    return elemTy;

  llvm::StringRef name = arrSt->getName();
  if (name.starts_with("array.")) {
    std::string innerName = name.substr(6).str();
    if (Type *t = llvm::StructType::getTypeByName(context, innerName))
      return t;
    if (Type *t = TypeResolver::fromName(context, innerName))
      return t;
  }
  return nullptr;
}

/**
 * Generates IR for an array index read expression (e.g. arr[i] or arr[i][j]).
 *
 * Handles multi-dimensional indexing by iterating through each dimension:
 * loads the array struct, extracts the data pointer, then GEPs to the element.
 * String indexing is a special case that returns a single i8 character.
 *
 * @param e the array-index expression AST node
 * @return the LLVM Value* of the element (a load or a pointer for aggregates)
 */
Value *CodeGenerator::visitArrayIndex(const ArrayIndexExpr &e) {
  Value *ptr = nullptr;
  Type *ty = nullptr;
  std::string name;

  if (e.object) {
    // Expression-based base (e.g. a function call returning an array).
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

    // String indexing yields a single character.
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

  // Ensure the type reflects the correct array depth for the given index count.
  if (auto *maybeSt = llvm::dyn_cast<llvm::StructType>(ty)) {
    if (!TypeResolver::isArray(maybeSt)) {
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
      // At the last dimension: return a pointer for aggregates, a load for
      // scalars.
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

/**
 * Generates IR for an array element assignment (e.g. arr[i] = val).
 * Mirrors visitArrayIndex but stores the value at the resolved element pointer
 * instead of loading it.
 * @param e the array-index assignment expression AST node
 * @return the LLVM Value* that was stored, or nullptr on error
 */
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

  if (auto *maybeSt = llvm::dyn_cast<llvm::StructType>(ty)) {
    if (!TypeResolver::isArray(maybeSt)) {
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
        Value *structVal = val->getType()->isPointerTy()
                               ? builder.CreateLoad(elemTy, val, "struct.val")
                               : val;
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

/**
 * Generates IR for the '.length' property read.
 * Arrays store their length at field index 0; strings store it at field 1.
 * @param e the length-property expression AST node
 * @return an i64 LLVM Value* holding the length, or nullptr on error
 */
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

/**
 * Generates IR for .length on a nested array element (e.g. arr[i].length).
 * Navigates each index dimension and then reads field 0 of the resulting
 * array struct.
 * @param e the indexed-length expression AST node
 * @return an i64 LLVM Value* holding the sub-array length, or nullptr
 */
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

/**
 * Generates IR to allocate a new heap array with the given dimensions.
 * Delegates to ArrayEmitter::makeND which handles both 1-D and N-D cases.
 * @param e the new-array expression AST node
 * @return an AllocaInst* pointing to the initialised array struct, or nullptr
 */
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

/*---------------------------------------*/
/*    Explicit casts (as / cast<>)       */
/*---------------------------------------*/

/**
 * Generates IR for an explicit type cast expression.
 * Supports: int ↔ int (trunc/sext/zext), float ↔ float (fpext/fptrunc),
 * int ↔ float (sitofp/fptosi), bool casts, and ptr ↔ ptr / ptr ↔ int.
 * @param e the cast expression AST node
 * @return the cast LLVM Value*, or nullptr if the cast is unsupported
 */
Value *CodeGenerator::visitCast(const CastExpr &e) {
  Value *val = codegen(*e.expr);
  if (!val)
    return nullptr;

  const std::string &targetName = e.targetType.base.token.getWord();
  Type *srcTy = val->getType();
  Type *dstTy = TypeResolver::fromName(context, targetName);
  if (!dstTy)
    return logError(("Unknown cast target type: " + targetName).c_str());

  // No-op cast: types already match.
  if (srcTy == dstTy)
    return val;

  if (srcTy->isIntegerTy() && dstTy->isIntegerTy()) {
    unsigned srcBits = srcTy->getIntegerBitWidth();
    unsigned dstBits = dstTy->getIntegerBitWidth();
    if (dstBits < srcBits)
      return builder.CreateTrunc(val, dstTy, "cast.trunc");
    // Zero-extend booleans (i1), sign-extend all other integers.
    if (srcBits == 1)
      return builder.CreateZExt(val, dstTy, "cast.zext");
    return builder.CreateSExt(val, dstTy, "cast.sext");
  }

  if (srcTy->isFloatingPointTy() && dstTy->isFloatingPointTy()) {
    if (srcTy->getPrimitiveSizeInBits() < dstTy->getPrimitiveSizeInBits())
      return builder.CreateFPExt(val, dstTy, "cast.fpext");
    return builder.CreateFPTrunc(val, dstTy, "cast.fptrunc");
  }

  if (srcTy->isFloatingPointTy() && dstTy->isIntegerTy()) {
    if (dstTy->getIntegerBitWidth() == 1)
      return builder.CreateFCmpONE(val, ConstantFP::get(srcTy, 0.0),
                                   "cast.ftobool");
    return builder.CreateFPToSI(val, dstTy, "cast.fptosi");
  }

  if (srcTy->isIntegerTy() && dstTy->isFloatingPointTy()) {
    if (srcTy->getIntegerBitWidth() == 1)
      return builder.CreateUIToFP(val, dstTy, "cast.uitofp");
    return builder.CreateSIToFP(val, dstTy, "cast.sitofp");
  }

  if (srcTy->isIntegerTy() && dstTy->getIntegerBitWidth() == 1)
    return builder.CreateICmpNE(val, ConstantInt::get(srcTy, 0), "cast.tobool");

  if (srcTy->isPointerTy() && dstTy->isPointerTy())
    return builder.CreatePointerCast(val, dstTy, "cast.ptr");

  if (srcTy->isIntegerTy() && dstTy->isPointerTy())
    return builder.CreateIntToPtr(val, dstTy, "cast.itoptr");

  if (srcTy->isPointerTy() && dstTy->isIntegerTy())
    return builder.CreatePtrToInt(val, dstTy, "cast.ptrtoi");

  return logError(("Unsupported cast: " + targetName).c_str());
}

/*---------------------------------------*/
/*        Name-mangling helpers          */
/*---------------------------------------*/

/**
 * Mangles a user function name to "Name$arity" to support overloading.
 * Built-in names (Main, Printf, Print, Random, Read) bypass mangling and
 * are instead passed through normalizeFunctionName().
 * @param name the function name as written in source
 * @param params the parameter list (used to derive the arity suffix)
 * @return the mangled function name for use in the IR
 */
static std::string mangleName(const std::string &name,
                              const std::vector<Parameter> &params) {
  static const std::unordered_set<std::string> kNoMangle = {
      "Main", "Printf", "Print", "Random", "Read"};
  if (kNoMangle.count(name))
    return normalizeFunctionName(name);
  return name + "$" + std::to_string(params.size());
}

/*---------------------------------------*/
/*       Function call emission          */
/*---------------------------------------*/

/**
 * Generates IR for a function call expression.
 *
 * Built-in calls (Printf, Print, Read, Random) are handled by dedicated
 * emitters. All other calls go through the standard LLVM call instruction.
 *
 * Argument preparation handles several calling-convention details:
 *  - BorrowArg / BorrowMutArg: the alloca address is passed directly.
 *  - Reference parameters: the alloca address is passed without loading.
 *  - Aggregate arguments (string, array, struct): passed as pointers.
 *  - Extern functions expecting char*: the data pointer is extracted from
 *    the string struct.
 *  - Final coercion ensures every argument matches the declared parameter type.
 *
 * @param e the call expression AST node
 * @return the LLVM Value* of the call result, or nullptr on error
 */
Value *CodeGenerator::visitCall(const CallExpr &e) {
  std::string rawName = "";
  std::string enumPrefix = "";

  if (auto *id = dynamic_cast<const IdentExpr *>(e.callee.get())) {
    rawName = id->name.token.getWord();
  } else if (auto *fa = dynamic_cast<const FieldAccessExpr *>(e.callee.get())) {
    std::string variantName = fa->field;

    if (auto *baseId = dynamic_cast<const IdentExpr *>(fa->object.get())) {
      std::string enumName = baseId->name.token.getWord();

      // Try the plain name first (non-generic enums).
      llvm::StructType *enumType =
          llvm::StructType::getTypeByName(context, "struct." + enumName);
      if (!enumType)
        enumType = llvm::StructType::getTypeByName(context, enumName);

      // For generic enums (e.g. Option<Animal>), the union struct is
      // registered under a mangled name like "Option$Animal" — not "Option".
      // Infer the concrete type args from the call arguments by inspecting the
      // namedValues table (no IR is emitted here), then call
      // instantiateGenericEnum which is idempotent and returns the existing
      // struct if it was already created by the forward-declaration pass.
      if (!enumType && currentProgram) {
        const EnumDecl *tmpl = nullptr;
        for (const auto &ed : currentProgram->enums) {
          if (ed->name == enumName && !ed->typeParams.empty()) {
            tmpl = ed.get();
            break;
          }
        }

        if (tmpl) {
          // Map each type parameter to its concrete LLVM type by looking at
          // the type of the corresponding call argument in namedValues.
          std::unordered_map<std::string, llvm::Type *> typeSubst;
          for (const auto &v : tmpl->variants) {
            if (v.name != variantName)
              continue;
            for (size_t i = 0; i < v.fields.size() && i < e.arguments.size();
                 ++i) {
              const std::string &fieldTypeName = v.fields[i].typeName;
              for (const auto &tp : tmpl->typeParams) {
                if (tp != fieldTypeName)
                  continue;
                if (auto *id =
                        dynamic_cast<const IdentExpr *>(e.arguments[i].get())) {
                  auto it = namedValues.find(id->name.token.getWord());
                  if (it != namedValues.end())
                    typeSubst[tp] = it->second.type;
                }
                break;
              }
            }
            break;
          }

          // Build ordered concrete arg lists matching tmpl->typeParams.
          std::vector<llvm::Type *> concreteArgs;
          std::vector<std::string> argNames;
          bool allResolved = true;
          for (const auto &tp : tmpl->typeParams) {
            auto it = typeSubst.find(tp);
            if (it == typeSubst.end()) {
              allResolved = false;
              break;
            }
            llvm::Type *t = it->second;
            concreteArgs.push_back(t);
            std::string tname;
            if (auto *st = llvm::dyn_cast<llvm::StructType>(t))
              tname = st->getName().str();
            else {
              llvm::raw_string_ostream rso(tname);
              t->print(rso);
            }
            argNames.push_back(tname);
          }

          if (allResolved && !concreteArgs.empty())
            enumType = instantiateGenericEnum(enumName, concreteArgs, argNames);
        }
      }

      if (!enumType)
        return logError(
            ("Unknown enum type for variant constructor: " + enumName).c_str());

      llvm::AllocaInst *enumAlloca = createEntryAlloca(enumType, "enum.tmp");
      builder.CreateStore(llvm::Constant::getNullValue(enumType), enumAlloca);

      int tagIndex = 0;
      {
        std::string key = enumName + "::" + variantName;
        auto tvIt = enumTagValues.find(key);
        if (tvIt != enumTagValues.end()) {
          tagIndex = static_cast<int>(tvIt->second);
        } else {
          for (const auto &ed : currentProgram->enums) {
            if (ed->name != enumName)
              continue;
            for (size_t vi = 0; vi < ed->variants.size(); ++vi) {
              if (ed->variants[vi].name == variantName) {
                tagIndex = static_cast<int>(vi);
                break;
              }
            }
          }
        }
      }

      llvm::Value *tagPtr =
          builder.CreateStructGEP(enumType, enumAlloca, 0, "tag.ptr");
      builder.CreateStore(builder.getInt32(tagIndex), tagPtr);

      for (size_t i = 0; i < e.arguments.size(); ++i) {
        llvm::Value *argVal = codegen(*e.arguments[i]);
        if (!argVal)
          return nullptr;

        // Field 0 is the tag; payload fields start at index 1.
        unsigned fieldIdx = static_cast<unsigned>(i) + 1;
        if (fieldIdx >= enumType->getNumElements())
          return logError("Too many arguments for enum variant constructor");

        llvm::Type *fieldTy = enumType->getElementType(fieldIdx);
        llvm::Value *payloadPtr = builder.CreateStructGEP(
            enumType, enumAlloca, fieldIdx, "payload.ptr");

        // Aggregate arguments (structs, strings, arrays) are returned as
        // AllocaInst* pointers by codegen — load the value before storing
        // it into the enum's payload slot.
        if (auto *ai = llvm::dyn_cast<llvm::AllocaInst>(argVal)) {
          llvm::Type *allocTy = ai->getAllocatedType();
          if (allocTy == fieldTy) {
            argVal = builder.CreateLoad(allocTy, ai, "payload.val");
          }
        }
        builder.CreateStore(argVal, payloadPtr);
      }

      return builder.CreateLoad(enumType, enumAlloca, "enum.val");
    }

    rawName = variantName;
  } else {
    return logError(
        "Callee expression type not supported for code generation.");
  }

  std::string calleeName = normalizeFunctionName(rawName);
  if (!enumPrefix.empty()) {
    calleeName = enumPrefix + "$" + calleeName;
  }

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

  llvm::Function *callee = module->getFunction(
      calleeName + "$" + std::to_string(e.arguments.size()));
  if (!callee)
    callee = module->getFunction(calleeName);
  if (!callee)
    return logError(("Unknown function: " + calleeName).c_str());

  auto refIt = borrowRefParams.find(callee->getName().str());
  std::vector<Value *> args;

  for (size_t i = 0; i < e.arguments.size(); ++i) {
    bool paramIsRef = refIt != borrowRefParams.end() &&
                      i < refIt->second.size() && refIt->second[i];

    auto mutIt = borrowMutParams.find(callee->getName().str());
    bool paramIsMut = mutIt != borrowMutParams.end() &&
                      i < mutIt->second.size() && mutIt->second[i];

    Value *v = nullptr;

    // BorrowArg / BorrowMutArg: pass the alloca address directly.
    if (auto *ba = dynamic_cast<const BorrowArgExpr *>(e.arguments[i].get())) {
      if (paramIsMut) {
        return logError(("Argument " + std::to_string(i + 1) + " of '" +
                         rawName +
                         "': parameter is '&mut' but '&' (immutable) was "
                         "passed use '&mut <var>'")
                            .c_str());
      }
      if (!paramIsRef) {
        return logError(("Argument " + std::to_string(i + 1) + " of '" +
                         rawName +
                         "': parameter is not a reference, remove '&'")
                            .c_str());
      }
      auto sit = namedValues.find(ba->name.token.getWord());
      if (sit == namedValues.end())
        return logError(
            ("Unknown variable: " + ba->name.token.getWord()).c_str());

      if (sit->second.isReference) {
        v = builder.CreateLoad(PointerType::get(context, 0),
                               sit->second.allocaInst,
                               ba->name.token.getWord() + ".deref");
      } else {
        v = sit->second.allocaInst;
      }
    } else if (auto *bm = dynamic_cast<const BorrowMutArgExpr *>(
                   e.arguments[i].get())) {
      if (!paramIsRef && !paramIsMut) {
        return logError(("Argument " + std::to_string(i + 1) + " of '" +
                         rawName +
                         "': parameter is not a reference, remove '&mut'")
                            .c_str());
      }
      auto sit = namedValues.find(bm->name.token.getWord());
      if (sit == namedValues.end())
        return logError(
            ("Unknown variable: " + bm->name.token.getWord()).c_str());

      if (sit->second.isReference) {
        v = builder.CreateLoad(PointerType::get(context, 0),
                               sit->second.allocaInst,
                               bm->name.token.getWord() + ".deref");
      } else {
        v = sit->second.allocaInst;
      }
    } else if (paramIsRef || paramIsMut) {
      bool isBorrowMut = dynamic_cast<const BorrowMutArgExpr *>(
                             e.arguments[i].get()) != nullptr;
      bool isBorrow =
          dynamic_cast<const BorrowArgExpr *>(e.arguments[i].get()) != nullptr;

      if (!isBorrowMut && !isBorrow) {
        return logError(("Argument " + std::to_string(i + 1) + " of '" +
                         rawName +
                         "': parameter is declared by-reference use '&mut "
                         "<var>' or '& <var>'")
                            .c_str());
      }
      if (paramIsMut && !isBorrowMut) {
        return logError(("Argument " + std::to_string(i + 1) + " of '" +
                         rawName +
                         "': parameter is '&mut' but '&' (immutable) was "
                         "passed use '&mut <var>'")
                            .c_str());
      }
      if (auto *id = dynamic_cast<const IdentExpr *>(e.arguments[i].get())) {
        auto sit = namedValues.find(id->name.token.getWord());
        if (sit != namedValues.end())
          v = sit->second.allocaInst;
      }
      if (!v) {
        v = codegen(*e.arguments[i]);
        if (!v)
          return nullptr;
        if (!v->getType()->isPointerTy()) {
          AllocaInst *tmp = createEntryAlloca(v->getType(), "ref.arg.tmp");
          builder.CreateStore(v, tmp);
          v = tmp;
        }
      }
    } else {
      v = codegen(*e.arguments[i]);
      if (!v)
        return nullptr;

      if (i < callee->arg_size()) {
        Type *expectedTy = callee->getFunctionType()->getParamType(i);
        Type *vTy = v->getType();

        if (TypeResolver::isString(vTy) && expectedTy->isPointerTy() &&
            callee->isDeclaration()) {
          v = builder.CreateExtractValue(v, {0}, "str.data");
        } else if (vTy->isPointerTy() && expectedTy->isPointerTy() &&
                   callee->isDeclaration()) {
          llvm::Type *pointeeTy = nullptr;
          if (auto *id =
                  dynamic_cast<const IdentExpr *>(e.arguments[i].get())) {
            auto sit = namedValues.find(id->name.token.getWord());
            if (sit != namedValues.end())
              pointeeTy = sit->second.type;
          }
          if (!pointeeTy) {
            if (auto *ai = llvm::dyn_cast<llvm::AllocaInst>(v))
              pointeeTy = ai->getAllocatedType();
          }

          if (pointeeTy && TypeResolver::isString(pointeeTy)) {
            Value *strVal = builder.CreateLoad(pointeeTy, v, "str.load");
            v = builder.CreateExtractValue(strVal, {0}, "str.data");
          } else if (pointeeTy && TypeResolver::isArray(pointeeTy)) {
            Value *arrVal = builder.CreateLoad(pointeeTy, v, "arr.load");
            v = builder.CreateExtractValue(arrVal, {1}, "arr.data");
          } else {
            if (auto *gep = llvm::dyn_cast<llvm::GetElementPtrInst>(v)) {
              llvm::Type *resultTy = gep->getResultElementType();
              llvm::Type *srcTy = gep->getSourceElementType();
              if (TypeResolver::isArray(resultTy) ||
                  TypeResolver::isArray(srcTy)) {
                llvm::Type *loadTy =
                    TypeResolver::isArray(resultTy) ? resultTy : srcTy;
                Value *loaded = builder.CreateLoad(loadTy, v, "arr.field.load");
                v = builder.CreateExtractValue(loaded, {1}, "arr.data");
              }
            }
          }
        } else if (vTy->isPointerTy() && (TypeResolver::isString(expectedTy) ||
                                          TypeResolver::isArray(expectedTy))) {
          v = builder.CreateLoad(expectedTy, v, "deref.arg");
        } else if (vTy->isStructTy() && expectedTy->isPointerTy() &&
                   !callee->isDeclaration()) {
          AllocaInst *tmp = createEntryAlloca(vTy, "struct.arg.tmp");
          builder.CreateStore(v, tmp);
          v = tmp;
        }
      }
    }

    if (callee->isDeclaration() && i < callee->arg_size()) {
      Type *expectedTy = callee->getFunctionType()->getParamType(i);
      if (expectedTy->isPointerTy()) {
        if (auto *id = dynamic_cast<const IdentExpr *>(e.arguments[i].get())) {
          auto sit = namedValues.find(id->name.token.getWord());
          if (sit != namedValues.end() &&
              !TypeResolver::isString(sit->second.type) &&
              !TypeResolver::isArray(sit->second.type)) {
            v = sit->second.type->isPointerTy()
                    ? builder.CreateLoad(sit->second.type,
                                         sit->second.allocaInst,
                                         id->name.token.getWord() + ".load")
                    : sit->second.allocaInst;
          }
        } else if (!v->getType()->isPointerTy()) {
          AllocaInst *tmp = createEntryAlloca(v->getType(), "ref.tmp");
          builder.CreateStore(v, tmp);
          v = tmp;
        }
      }
    }

    if (!callee->isVarArg() && i < callee->arg_size()) {
      Type *expectedTy = callee->getFunctionType()->getParamType(i);
      v = TypeResolver::coerce(builder, v, expectedTy);
    }

    args.push_back(v);
  }

  bool isVoid = callee->getReturnType()->isVoidTy();
  return builder.CreateCall(callee, args, isVoid ? "" : "call");
}

Value *CodeGenerator::visitGenericCall(const GenericCallExpr &e) {
  const std::string rawName = e.callee.token.getWord();

  std::vector<llvm::Type *> resolvedTypeArgs;
  for (const auto &td : e.typeArgs) {
    llvm::Type *t = TypeResolver::fromTypeDesc(context, td);
    if (!t)
      t = llvm::StructType::getTypeByName(context, td.base.token.getWord());
    if (!t)
      return logError(
          ("Unknown type argument: " + td.base.token.getWord()).c_str());
    resolvedTypeArgs.push_back(t);
  }

  std::string mangledName = rawName;
  for (llvm::Type *t : resolvedTypeArgs) {
    std::string typeName;
    llvm::raw_string_ostream rso(typeName);
    t->print(rso);
    mangledName += "$" + rso.str();
  }
  mangledName += "$" + std::to_string(e.arguments.size());

  auto cacheIt = genericCache.find(mangledName);
  llvm::Function *callee = nullptr;
  if (cacheIt != genericCache.end()) {
    callee = cacheIt->second;
  } else {
    if (!currentProgram)
      return logError("No program context for generic call");

    const AST_H::Function *astFn = nullptr;
    for (const auto &fn : currentProgram->functions) {
      if (fn->name.token.getWord() == rawName && !fn->typeParams.empty()) {
        astFn = fn.get();
        break;
      }
    }
    if (!astFn)
      return logError(("No generic function found: " + rawName).c_str());

    if (astFn->typeParams.size() != resolvedTypeArgs.size())
      return logError(
          ("Wrong number of type arguments for: " + rawName).c_str());

    callee = emitGenericSpecialization(*astFn, mangledName, resolvedTypeArgs);
    if (!callee)
      return nullptr;
    genericCache[mangledName] = callee;
  }

  std::vector<Value *> args;
  for (size_t i = 0; i < e.arguments.size(); ++i) {
    Value *v = codegen(*e.arguments[i]);
    if (!v)
      return nullptr;

    if (i < callee->arg_size()) {
      Type *expectedTy = callee->getFunctionType()->getParamType(i);
      Type *vTy = v->getType();

      if (vTy->isPointerTy() && (TypeResolver::isString(expectedTy) ||
                                 TypeResolver::isArray(expectedTy)))
        v = builder.CreateLoad(expectedTy, v, "deref.arg");
      else if (vTy->isStructTy() && expectedTy->isPointerTy()) {
        AllocaInst *tmp = createEntryAlloca(vTy, "struct.arg.tmp");
        builder.CreateStore(v, tmp);
        v = tmp;
      } else {
        v = TypeResolver::coerce(builder, v, expectedTy);
      }
    }
    args.push_back(v);
  }

  bool isVoid = callee->getReturnType()->isVoidTy();
  return builder.CreateCall(callee, args, isVoid ? "" : "gcall");
}

llvm::Function *CodeGenerator::emitGenericSpecialization(
    const AST_H::Function &astFn, const std::string &mangledName,
    const std::vector<llvm::Type *> &typeArgs) {

  std::unordered_map<std::string, llvm::Type *> typeSubst;
  for (size_t i = 0; i < astFn.typeParams.size(); ++i)
    typeSubst[astFn.typeParams[i]] = typeArgs[i];

  auto resolveType = [&](const TypeDesc &td) -> llvm::Type * {
    const std::string &baseName = td.base.token.getWord();
    llvm::Type *base = nullptr;
    auto subIt = typeSubst.find(baseName);
    if (subIt != typeSubst.end())
      base = subIt->second;
    else {
      base = TypeResolver::fromTypeDesc(context, td);
      if (!base)
        base = llvm::StructType::getTypeByName(context, baseName);
    }
    if (!base)
      return nullptr;
    for (int d = 0; d < td.dimensions; ++d)
      base = TypeResolver::getOrCreateArrayStruct(context, base);
    return base;
  };

  std::vector<llvm::Type *> paramTypes;
  for (const auto &p : astFn.params) {
    llvm::Type *pt = resolveType(p.type);
    if (!pt)
      return nullptr;
    paramTypes.push_back(p.isBorrowRef || passAsPointer(pt)
                             ? llvm::PointerType::get(context, 0)
                             : pt);
  }

  llvm::Type *retTy = resolveType(astFn.returnType);
  if (!retTy)
    retTy = llvm::Type::getVoidTy(context);

  auto *ft = llvm::FunctionType::get(retTy, paramTypes, false);
  llvm::Function *f = llvm::Function::Create(
      ft, llvm::Function::ExternalLinkage, mangledName, *module);
  f->addFnAttr("stackrealignment");

  std::vector<bool> paramIsRef, paramIsMut;
  for (const auto &p : astFn.params) {
    paramIsRef.push_back(p.isBorrowRef);
    paramIsMut.push_back(p.isBorrowRef && p.isMut);
  }
  borrowRefParams[mangledName] = paramIsRef;
  borrowMutParams[mangledName] = paramIsMut;

  auto savedNamedValues = namedValues;
  auto *savedBB = builder.GetInsertBlock();
  auto savedIP = builder.GetInsertPoint();

  llvm::BasicBlock *entry = llvm::BasicBlock::Create(context, "entry", f);
  builder.SetInsertPoint(entry);

  namedValues = globalValues;
  scopeMgr.reset();
  scopeMgr.pushScope();

  size_t idx = 0;
  for (auto &arg : f->args()) {
    const auto &param = astFn.params[idx++];
    const std::string pname = param.name.token.getWord();
    llvm::Type *declaredTy = resolveType(param.type);
    if (!declaredTy)
      declaredTy = arg.getType();

    llvm::AllocaInst *a = createEntryAlloca(declaredTy, pname);
    if (TypeResolver::isString(declaredTy) ||
        TypeResolver::isArray(declaredTy) || declaredTy->isStructTy()) {
      Value *val = builder.CreateLoad(declaredTy, &arg, pname + ".param");
      builder.CreateStore(val, a);
      VarInfo vi(a, declaredTy, false, false, false, param.isConst);
      vi.ownsHeap = false;
      namedValues[pname] = vi;
    } else {
      builder.CreateStore(&arg, a);
      namedValues[pname] =
          VarInfo(a, declaredTy, false, false, false, param.isConst);
    }
    scopeMgr.declare(pname);
  }

  codegen(*astFn.body);

  if (!blockHasTerminator(builder)) {
    scopeMgr.popAll();
    if (retTy->isVoidTy())
      builder.CreateRetVoid();
    else if (retTy->isFloatingPointTy())
      builder.CreateRet(llvm::ConstantFP::get(retTy, 0.0));
    else
      builder.CreateRet(llvm::ConstantInt::get(retTy, 0));
  }

  namedValues = savedNamedValues;
  if (savedBB)
    builder.SetInsertPoint(savedBB, savedIP);

  if (llvm::verifyFunction(*f, &llvm::errs())) {
    llvm::errs() << "verifyFunction failed for generic specialization: "
                 << mangledName << "\n";
    f->eraseFromParent();
    return nullptr;
  }

  return f;
}

llvm::StructType *CodeGenerator::instantiateGenericStruct(const TypeDesc &td) {
  const std::string &baseName = td.base.token.getWord();

  const StructDecl *tmpl = nullptr;
  for (const auto &s : currentProgram->structs) {
    if (s->name == baseName && !s->typeParams.empty()) {
      tmpl = s.get();
      break;
    }
  }
  if (!tmpl)
    return nullptr;

  std::string mangledName = baseName;
  std::vector<llvm::Type *> concreteArgs;
  for (const auto &arg : td.typeArgs) {
    llvm::Type *t = TypeResolver::fromTypeDesc(context, arg);
    if (!t)
      t = llvm::StructType::getTypeByName(context, arg.base.token.getWord());
    if (!t)
      return nullptr;
    mangledName += "$" + arg.base.token.getWord();
    concreteArgs.push_back(t);
  }

  if (auto *existing = llvm::StructType::getTypeByName(context, mangledName))
    return existing;

  std::unordered_map<std::string, llvm::Type *> subst;
  for (size_t i = 0; i < tmpl->typeParams.size(); ++i)
    subst[tmpl->typeParams[i]] = concreteArgs[i];

  auto *st = llvm::StructType::create(context, mangledName);
  std::vector<llvm::Type *> fieldTypes;
  for (const auto &f : tmpl->fields) {
    const std::string &fn = f.type.base.token.getWord();
    llvm::Type *ft = nullptr;
    auto it = subst.find(fn);
    if (it != subst.end()) {
      ft = it->second;
      for (int d = 0; d < f.type.dimensions; ++d)
        ft = TypeResolver::getOrCreateArrayStruct(context, ft);
    } else {
      ft = TypeResolver::fromTypeDesc(context, f.type);
      if (!ft)
        ft = llvm::StructType::getTypeByName(context, fn);
    }
    if (!ft)
      return nullptr;
    fieldTypes.push_back(ft);
  }
  st->setBody(fieldTypes);

  concreteStructFields[mangledName] = tmpl;
  auto synth = std::make_unique<StructDecl>(mangledName, tmpl->fields);
  structDefs.push_back(synth.get());
  synthStructDecls.push_back(std::move(synth));
  return st;
}

llvm::StructType *CodeGenerator::instantiateGenericEnum(
    const std::string &enumName, const std::vector<llvm::Type *> &concreteArgs,
    const std::vector<std::string> &argNames) {
  const EnumDecl *tmpl = nullptr;
  for (const auto &e : currentProgram->enums) {
    if (e->name == enumName && !e->typeParams.empty()) {
      tmpl = e.get();
      break;
    }
  }
  if (!tmpl)
    return nullptr;

  std::unordered_map<std::string, llvm::Type *> subst;
  for (size_t i = 0; i < tmpl->typeParams.size(); ++i)
    subst[tmpl->typeParams[i]] = concreteArgs[i];

  std::string mangledBase = enumName;
  for (auto &an : argNames)
    mangledBase += "$" + an;

  long long nextTag = 0;
  for (const auto &v : tmpl->variants) {
    long long tagForThis = nextTag++;
    std::string sname = mangledBase + "_" + v.name;
    if (llvm::StructType::getTypeByName(context, sname))
      continue;

    std::vector<llvm::Type *> fields;
    fields.push_back(llvm::Type::getInt32Ty(context)); // tag
    for (const auto &f : v.fields) {
      llvm::Type *ft = nullptr;
      auto it = subst.find(f.typeName);
      if (it != subst.end())
        ft = it->second;
      else {
        ft = TypeResolver::fromName(context, f.typeName);
        if (!ft)
          ft = llvm::StructType::getTypeByName(context, f.typeName);
      }
      if (!ft)
        ft = llvm::Type::getInt32Ty(context);
      fields.push_back(ft);
    }
    auto *st = llvm::StructType::create(context, sname);
    st->setBody(fields);

    enumTagValues[mangledBase + "::" + v.name] = tagForThis;
    enumTagValues[enumName + "::" + v.name] = tagForThis;
  }

  llvm::StructType *existing =
      llvm::StructType::getTypeByName(context, mangledBase);
  if (existing)
    return existing;

  size_t maxFields = 0;
  const EnumVariant *maxVariant = nullptr;
  for (const auto &v : tmpl->variants) {
    if (v.fields.size() > maxFields) {
      maxFields = v.fields.size();
      maxVariant = &v;
    }
  }

  std::vector<llvm::Type *> unionFields;
  unionFields.push_back(llvm::Type::getInt32Ty(context)); // tag
  if (maxVariant) {
    for (const auto &f : maxVariant->fields) {
      llvm::Type *ft = nullptr;
      auto it = subst.find(f.typeName);
      if (it != subst.end())
        ft = it->second;
      else {
        ft = TypeResolver::fromName(context, f.typeName);
        if (!ft)
          ft = llvm::StructType::getTypeByName(context, f.typeName);
      }
      if (ft)
        unionFields.push_back(ft);
    }
  }
  auto *unionSt = llvm::StructType::create(context, mangledBase);
  unionSt->setBody(unionFields);
  return unionSt;
}

/**
 * BorrowArg is only valid as a call argument; if it appears elsewhere the
 * caller made an error.
 */
Value *CodeGenerator::visitBorrowArg(const BorrowArgExpr &e) {
  return logError(
      ("'&" + e.name.token.getWord() + "' is only valid as a call argument")
          .c_str());
}

/**
 * BorrowMutArg is only valid as a call argument; if it appears elsewhere the
 * caller made an error.
 */
Value *CodeGenerator::visitBorrowMutArg(const BorrowMutArgExpr &e) {
  return logError(
      ("'&mut " + e.name.token.getWord() + "' is only valid as a call argument")
          .c_str());
}

/*---------------------------------------*/
/*         Struct field access           */
/*---------------------------------------*/

/**
 * Looks up the field index for a given struct name and field name.
 * Iterates over all registered struct definitions until a match is found.
 * @param defs the list of all struct declarations in the program
 * @param structName the name of the struct type
 * @param fieldName the name of the field to find
 * @param found set to true iff the field was located
 * @return the zero-based field index, or 0 (with found=false) if not found
 */
unsigned CodeGenerator::findFieldIndex(const std::vector<StructDecl *> &defs,
                                       const std::string &structName,
                                       const std::string &fieldName,
                                       bool &found) {
  found = false;
  for (const auto *s : defs) {
    if (s->name == structName) {
      for (unsigned i = 0; i < s->fields.size(); ++i) {
        if (s->fields[i].name == fieldName) {
          found = true;
          return i;
        }
      }
    }
  }
  return 0;
}

/**
 * Generates IR for a struct field read (e.g. point.x).
 * Resolves the struct pointer via resolveStructPtr(), then uses a
 * GetElementPtr + load to read the field value.
 * Aggregate fields (strings, arrays) are returned as GEP pointers.
 * @param e the field-access expression AST node
 * @return the LLVM Value* of the field, or nullptr on error
 */
Value *CodeGenerator::visitFieldAccess(const FieldAccessExpr &e) {
  // Check for enum variant access (e.g. Option.None, Code.A) BEFORE trying
  // resolveStructPtr, because the enum name is not a variable and will cause
  // resolveStructPtr to return null, masking the real intent.
  if (auto *baseId = dynamic_cast<const IdentExpr *>(e.object.get())) {
    const std::string &enumName = baseId->name.token.getWord();
    const std::string &varName = e.field;

    if (currentProgram) {
      for (const auto &ed : currentProgram->enums) {
        if (ed->name != enumName)
          continue;
        for (size_t vi = 0; vi < ed->variants.size(); ++vi) {
          if (ed->variants[vi].name != varName)
            continue;

          long long tagVal = vi;
          std::string key = enumName + "::" + varName;
          auto tvIt = enumTagValues.find(key);
          if (tvIt != enumTagValues.end())
            tagVal = tvIt->second;

          std::string sname = enumName + "_" + varName;
          llvm::StructType *varSt =
              llvm::StructType::getTypeByName(context, sname);

          if (!varSt) {
            std::string prefix = enumName + "$";
            for (auto &st : module->getIdentifiedStructTypes()) {
              std::string stName = st->getName().str();
              if (stName.rfind(prefix, 0) == 0 &&
                  stName.find("_" + varName) != std::string::npos) {
                varSt = st;
                break;
              }
            }
          }

          if (!varSt) {
            varSt = llvm::StructType::create(
                context, {llvm::Type::getInt32Ty(context)}, sname);
          }

          AllocaInst *alloca = createEntryAlloca(varSt, sname + ".val");
          builder.CreateStore(llvm::Constant::getNullValue(varSt), alloca);
          Value *tagPtr =
              builder.CreateStructGEP(varSt, alloca, 0, sname + ".tag");
          builder.CreateStore(
              llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), tagVal),
              tagPtr);
          return alloca;
        }
      }
    }
  }

  auto [structPtr, st] = resolveStructPtr(*e.object);
  if (!structPtr || !st)
    return logError("Field access requires a struct expression");
  if (auto *baseId = dynamic_cast<const IdentExpr *>(e.object.get())) {
    const std::string &enumName = baseId->name.token.getWord();
    const std::string &varName = e.field;

    if (currentProgram) {
      for (const auto &ed : currentProgram->enums) {
        if (ed->name != enumName)
          continue;
        for (size_t vi = 0; vi < ed->variants.size(); ++vi) {
          if (ed->variants[vi].name != varName)
            continue;

          long long tagVal = vi;
          std::string key = enumName + "::" + varName;
          auto tvIt = enumTagValues.find(key);
          if (tvIt != enumTagValues.end())
            tagVal = tvIt->second;

          std::string sname = enumName + "_" + varName;
          llvm::StructType *varSt =
              llvm::StructType::getTypeByName(context, sname);

          if (!varSt) {
            std::string prefix = enumName + "$";
            for (auto &st : module->getIdentifiedStructTypes()) {
              std::string stName = st->getName().str();
              if (stName.rfind(prefix, 0) == 0 &&
                  stName.find("_" + varName) != std::string::npos) {
                varSt = st;
                break;
              }
            }
          }

          if (!varSt) {
            varSt = llvm::StructType::create(
                context, {llvm::Type::getInt32Ty(context)}, sname);
          }

          AllocaInst *alloca = createEntryAlloca(varSt, sname + ".val");
          builder.CreateStore(llvm::Constant::getNullValue(varSt), alloca);
          Value *tagPtr =
              builder.CreateStructGEP(varSt, alloca, 0, sname + ".tag");
          builder.CreateStore(
              llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), tagVal),
              tagPtr);
          return alloca;
        }
      }
    }
  }
  bool found;
  unsigned idx =
      findFieldIndex(structDefs, st->getName().str(), e.field, found);
  if (!found)
    return logError(("Unknown field: " + e.field).c_str());

  Value *gep = builder.CreateStructGEP(st, structPtr, idx, e.field + ".ptr");
  Type *fieldTy = st->getElementType(idx);

  // Aggregate fields are returned as pointers to their storage.
  if (TypeResolver::isString(fieldTy) || TypeResolver::isArray(fieldTy))
    return gep;
  return builder.CreateLoad(fieldTy, gep, e.field);
}

/**
 * Resolves a (possibly complex) lvalue expression to a (pointer, struct-type)
 * pair. Handles plain identifiers, array-indexed elements, and chained field
 * accesses. Returns (nullptr, nullptr) when resolution fails.
 *
 * This is the common helper used by both visitFieldAccess and visitFieldAssign.
 *
 * @param expr the lvalue expression to resolve
 * @return a pair of (pointer to struct storage, the llvm::StructType*)
 */
std::pair<Value *, llvm::StructType *>
CodeGenerator::resolveStructPtr(const Expression &expr) {
  // Plain identifier: look up the alloca and verify it holds a struct.
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

  // Array-indexed expression: navigate to the element, then cast to struct.
  if (auto *ai = dynamic_cast<const ArrayIndexExpr *>(&expr)) {
    Value *ptr = nullptr;
    Type *ty = nullptr;

    // Ensures the type has the required number of array-wrapping dimensions.
    auto ensureArrayDepth = [&](Type *t, size_t depth) -> Type * {
      if (auto *s = llvm::dyn_cast<llvm::StructType>(t))
        if (TypeResolver::isArray(s))
          return t;
      for (size_t i = 0; i < depth; ++i) {
        t = TypeResolver::getOrCreateArrayStruct(context, t);
        if (!t)
          return nullptr;
      }
      return t;
    };

    if (ai->object) {
      // Expression base: e.g. list.elt[cpt] where base is a FieldAccessExpr.
      // codegen() on an array field returns a GEP pointer to the array struct.
      ptr = codegen(*ai->object);
      if (!ptr)
        return {nullptr, nullptr};
      if (auto *gepInst = llvm::dyn_cast<llvm::GetElementPtrInst>(ptr))
        ty = gepInst->getResultElementType();
      else if (auto *allocaInst = llvm::dyn_cast<llvm::AllocaInst>(ptr))
        ty = allocaInst->getAllocatedType();
      else
        return {nullptr, nullptr};
    } else {
      const std::string &name = ai->array.token.getWord();
      auto it = namedValues.find(name);
      if (it == namedValues.end())
        return {nullptr, nullptr};

      ptr = it->second.allocaInst;
      ty = it->second.type;

      if (it->second.isReference) {
        ptr = builder.CreateLoad(PointerType::get(context, 0), ptr);
        if (it->second.pointeeType)
          ty = it->second.pointeeType;
      }
    }
    ty = ensureArrayDepth(ty, ai->indices.size());
    if (!ty)
      return {nullptr, nullptr};

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
        // Verify the struct type is registered in structDefs.
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

  // Chained field access: recurse on the base, then GEP into the named field.
  if (auto *fa = dynamic_cast<const FieldAccessExpr *>(&expr)) {
    auto [basePtr, baseSt] = resolveStructPtr(*fa->object);
    if (!basePtr || !baseSt)
      return {nullptr, nullptr};

    bool found;
    unsigned idx =
        findFieldIndex(structDefs, baseSt->getName().str(), fa->field, found);
    if (!found)
      return {nullptr, nullptr};

    Type *fieldTy = baseSt->getElementType(idx);
    Value *gep =
        builder.CreateStructGEP(baseSt, basePtr, idx, fa->field + ".ptr");
    auto *st = llvm::dyn_cast<llvm::StructType>(fieldTy);
    if (!st || TypeResolver::isArray(st))
      return {nullptr, nullptr};
    return {gep, st};
  }

  return {nullptr, nullptr};
}

/**
 * Generates IR for a struct literal expression (e.g. Point { 1, 2 }).
 * Allocates an entry-block alloca for the struct, then stores each field
 * value. String fields transfer ownership: the source data pointer is nulled
 * to prevent a double-free.
 * @param e the struct-literal expression AST node
 * @return an AllocaInst* pointing to the initialised struct, or nullptr
 */
Value *CodeGenerator::visitStructLit(const StructLitExpr &e) {
  llvm::StructType *st = llvm::StructType::getTypeByName(context, e.typeName);
  if (!st)
    return logError(("Unknown struct type: " + e.typeName).c_str());

  AllocaInst *alloca = createEntryAlloca(st, e.typeName + ".lit");

  for (unsigned i = 0; i < e.values.size(); ++i) {
    if (i >= st->getNumElements())
      return logError("Too many values in struct literal");

    Value *val = codegen(*e.values[i]);
    if (!val)
      return nullptr;
    Type *fieldTy = st->getElementType(i);

    if (TypeResolver::isString(fieldTy)) {
      if (auto *ai = dyn_cast<AllocaInst>(val)) {
        // Clone the string so the struct owns its own heap buffer.
        Value *cloned = StringOps::clone(builder, context, module.get(), ai);
        val = builder.CreateLoad(fieldTy, cloned, "field.load");
        Value *dataGep = builder.CreateStructGEP(
            TypeResolver::getStringType(context), cloned, 0, "src.data.ptr");
        builder.CreateStore(
            llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0)),
            dataGep);
      }
    } else if (TypeResolver::isArray(fieldTy)) {
      if (auto *ai = dyn_cast<AllocaInst>(val)) {
        val = builder.CreateLoad(fieldTy, ai, "field.load");
        auto *arrSt = llvm::dyn_cast<llvm::StructType>(fieldTy);
        if (arrSt) {
          Value *srcDataGep =
              builder.CreateStructGEP(arrSt, ai, 1, "src.arr.data.ptr");
          builder.CreateStore(llvm::ConstantPointerNull::get(
                                  llvm::PointerType::get(context, 0)),
                              srcDataGep);
        }
      }
    } else {
      val = TypeResolver::coerce(builder, val, fieldTy);
    }

    Value *gep = builder.CreateStructGEP(st, alloca, i,
                                         e.typeName + ".f" + std::to_string(i));
    builder.CreateStore(val, gep);
  }
  return alloca;
}

/**
 * Generates IR for a chained comparison such as  5 < x < 10.
 *
 * Each comparison shares its middle operand with the next, so the middle
 * expression is evaluated only once.  All partial results are ANDed together.
 *
 *   5 < x < 10   →   (5 < x) && (x < 10)
 *   a <= b <= c  →   (a <= b) && (b <= c)
 *
 * @param e the chained-comparison AST node
 * @return the LLVM i1 Value* of the combined condition, or nullptr on error
 */
Value *CodeGenerator::visitChainedCmp(const ChainedCmpExpr &e) {
  // Evaluate the leftmost operand once.
  Value *prev = codegen(*e.lhs);
  if (!prev)
    return nullptr;

  Value *result = nullptr;

  for (size_t i = 0; i < e.ops.size(); ++i) {
    Value *next = codegen(*e.operands[i]);
    if (!next)
      return nullptr;

    Value *l = prev, *r = next;
    bool lf = l->getType()->isFloatingPointTy();
    bool rf = r->getType()->isFloatingPointTy();
    if (lf && r->getType()->isIntegerTy())
      r = builder.CreateSIToFP(r, Type::getDoubleTy(context), "itof");
    else if (rf && l->getType()->isIntegerTy())
      l = builder.CreateSIToFP(l, Type::getDoubleTy(context), "itof");

    Value *cmp = ArithmeticManager::generateComparison(builder, e.ops[i], l, r,
                                                       lf || rf);
    if (!cmp)
      return logError("Unsupported operator in chained comparison");

    result = result ? builder.CreateAnd(result, cmp, "chain.and") : cmp;

    prev = next;
  }

  return result;
}

/**
 * Generates IR for the Type() intrinsic, which prints the type name of its
 * first argument to stdout at runtime.
 *
 * FIX (Bug 1): The original loop used typeArgs.empty() as the upper bound,
 * which always evaluates to 0 or 1 (bool), causing the type-argument list
 * to never be fully appended. Changed to typeArgs.size().
 *
 * @param e the type-intrinsic expression AST node
 * @return always nullptr (side-effect only)
 */
Value *CodeGenerator::visitTypeIntrinsic(const TypeIntrinsicExpr &e) {
  const std::string typeName = e.typeDesc.base.token.getWord();
  std::string msg = typeName;
  if (!e.typeDesc.typeArgs.empty()) {
    msg += "<";
    // BUG FIX: was typeArgs.empty() (always 0 or 1), must be typeArgs.size()
    for (size_t i = 0; i < e.typeDesc.typeArgs.size(); ++i) {
      if (i)
        msg += ", ";
      msg += e.typeDesc.typeArgs[i].base.token.getWord();
    }
    msg += ">";
  }
  msg += "\n";

  Value *fmtStr = StringOps::fromLiteral(builder, context, module.get(), msg);
  llvm::StructType *strSt = TypeResolver::getStringType(context);
  Value *loaded = builder.CreateLoad(strSt, fmtStr, "type.str");
  Value *rawPtr = builder.CreateExtractValue(loaded, {0}, "type.ptr");

  llvm::Function *pf = module->getFunction("printf");
  if (pf)
    builder.CreateCall(pf, {rawPtr});
  return nullptr;
}

/**
 * Generates IR for a struct field write (e.g. point.x = 5).
 * Resolves the struct pointer, finds the field index, then stores the value.
 * @param e the field-assign expression AST node
 * @return the stored LLVM Value*, or nullptr on error
 */
Value *CodeGenerator::visitFieldAssign(const FieldAssignExpr &e) {
  auto [structPtr, st] = resolveStructPtr(*e.object);
  if (!structPtr || !st)
    return logError("Field assign requires a struct expression");

  bool found;
  unsigned idx =
      findFieldIndex(structDefs, st->getName().str(), e.field, found);
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
  if (TypeResolver::isString(fieldTy)) {
    llvm::Value *srcPtr = nullptr;
    if (val->getType()->isPointerTy()) {
      srcPtr = val;
    } else {
      AllocaInst *tmp = createEntryAlloca(fieldTy, e.field + ".assign.tmp");
      builder.CreateStore(val, tmp);
      srcPtr = tmp;
    }

    llvm::StructType *strTy = TypeResolver::getStringType(context);
    Value *oldDataGep =
        builder.CreateStructGEP(strTy, gep, 0, e.field + ".old.data");
    Value *oldData = builder.CreateLoad(llvm::PointerType::get(context, 0),
                                        oldDataGep, e.field + ".old.ptr");
    llvm::BasicBlock *curBB = builder.GetInsertBlock();
    llvm::Function *fn = curBB->getParent();
    auto *freeBB = llvm::BasicBlock::Create(context, e.field + ".old.free", fn);
    auto *skipBB = llvm::BasicBlock::Create(context, e.field + ".old.skip", fn);
    Value *isNull = builder.CreateICmpEQ(
        oldData,
        llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0)),
        e.field + ".old.null");
    builder.CreateCondBr(isNull, skipBB, freeBB);
    builder.SetInsertPoint(freeBB);
    builder.CreateCall(getFree(), {oldData});
    builder.CreateBr(skipBB);
    builder.SetInsertPoint(skipBB);

    Value *cloned = StringOps::clone(builder, context, module.get(), srcPtr);
    Value *freshVal = builder.CreateLoad(fieldTy, cloned, e.field + ".fresh");
    builder.CreateStore(freshVal, gep);

    if (auto *cloneAI = llvm::dyn_cast<llvm::AllocaInst>(cloned)) {
      Value *cloneDataGep =
          builder.CreateStructGEP(strTy, cloneAI, 0, e.field + ".clone.null");
      builder.CreateStore(
          llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0)),
          cloneDataGep);
    }
    return freshVal;
  }

  builder.CreateStore(TypeResolver::coerce(builder, val, fieldTy), gep);
  return val;
}

/*---------------------------------------*/
/*     StmtVisitor implementations       */
/*---------------------------------------*/

/**
 * Allocates a variable in the function's entry block.
 * Placing all allocas at the entry point enables mem2reg to promote them to
 * SSA registers, eliminating redundant loads and stores in the final IR.
 * @param ty the LLVM type to allocate
 * @param name the name to give the alloca (for readability in the IR)
 * @return the AllocaInst* for the allocated slot
 */
llvm::AllocaInst *CodeGenerator::createEntryAlloca(llvm::Type *ty,
                                                   const std::string &name) {
  llvm::Function *fn = builder.GetInsertBlock()->getParent();
  llvm::BasicBlock &entry = fn->getEntryBlock();
  llvm::IRBuilder<> tmpB(&entry, entry.begin());
  return tmpB.CreateAlloca(ty, nullptr, name);
}

/**
 * Generates IR for an expression statement (an expression used as a
 * statement, e.g. a function call whose return value is discarded).
 * @param s the expression-statement AST node
 * @return the underlying expression value, or nullptr if the statement
 *         is empty
 */
Value *CodeGenerator::visitExprStmt(const ExprStmt &s) {
  return s.expr ? codegen(*s.expr) : nullptr;
}

/**
 * Generates IR for a variable declaration, optionally with an initializer.
 * Handles all three assignment kinds (copy, move, borrow) and all value
 * categories (scalars, strings, arrays, structs).
 *
 * For strings: a fresh heap buffer is allocated for copies to ensure
 * independent ownership; moves transfer the source buffer and mark it moved.
 *
 * For arrays created with 'new': the alloca type may be narrowed to match
 * the actual element type returned by ArrayEmitter.
 *
 * @param d the variable declaration AST node
 * @return the AllocaInst* for the declared variable, or nullptr on error
 */
Value *CodeGenerator::visitVarDecl(const VarDecl &d) {
  const std::string name = d.name.token.getWord();
  const std::string typeName = d.type.base.token.getWord();

  bool inferred = (typeName == "let" || typeName.empty());

  if (inferred) {
    if (!d.initializer)
      return logError(
          ("'let' variable '" + name + "' must have an initializer").c_str());

    Value *init = codegen(*d.initializer);
    if (!init)
      return nullptr;

    Type *ty = nullptr;
    if (auto *ai = llvm::dyn_cast<llvm::AllocaInst>(init))
      ty = ai->getAllocatedType();
    else
      ty = init->getType();

    AllocaInst *alloca = createEntryAlloca(ty, name);
    VarInfo vi(alloca, ty, false, false, false, d.isConst);

    if (TypeResolver::isString(ty) || TypeResolver::isArray(ty)) {
      Value *val = builder.CreateLoad(ty, init, name + ".infer.load");
      builder.CreateStore(val, alloca);
      vi.ownsHeap = true;
    } else if (ty->isStructTy()) {
      Value *val = init->getType()->isPointerTy()
                       ? builder.CreateLoad(ty, init, name + ".infer.load")
                       : init;
      builder.CreateStore(val, alloca);
    } else {
      builder.CreateStore(TypeResolver::coerce(builder, init, ty), alloca);
    }

    namedValues[name] = vi;
    scopeMgr.declare(name);
    return alloca;
  }

  Type *ty = nullptr;
  if (!d.type.typeArgs.empty()) {
    ty = instantiateGenericStruct(d.type);
    if (!ty) {
      // Try as a generic enum
      std::vector<llvm::Type *> concreteArgs;
      std::vector<std::string> argNames;
      for (const auto &arg : d.type.typeArgs) {
        llvm::Type *t = TypeResolver::fromTypeDesc(context, arg);
        if (!t)
          t = llvm::StructType::getTypeByName(context,
                                              arg.base.token.getWord());
        if (t) {
          concreteArgs.push_back(t);
          argNames.push_back(arg.base.token.getWord());
        }
      }
      if (!concreteArgs.empty())
        ty = instantiateGenericEnum(typeName, concreteArgs, argNames);
    }
  } else {
    ty = TypeResolver::fromTypeDesc(context, d.type);
    if (!ty)
      ty = llvm::StructType::getTypeByName(context, typeName);
  }
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

  if (auto *slit = dynamic_cast<StructLitExpr *>(d.initializer.get())) {
    if (!d.type.typeArgs.empty()) {
      auto *concreteSt = llvm::dyn_cast<llvm::StructType>(ty);
      if (concreteSt)
        slit->typeName = concreteSt->getName().str();
    }
  }

  Value *init = codegen(*d.initializer);
  if (!init)
    return nullptr;

  switch (d.kind) {
  case AssignKind::Copy: {
    if (TypeResolver::isString(ty)) {
      Value *src = init;
      if (!src->getType()->isPointerTy()) {
        AllocaInst *tmp = builder.CreateAlloca(ty, nullptr, name + ".src.tmp");
        builder.CreateStore(src, tmp);
        src = tmp;
      }

      auto *srcAI = dyn_cast<AllocaInst>(src);
      bool isTmp = srcAI && [&]() {
        for (auto &kv : namedValues)
          if (kv.second.allocaInst == srcAI)
            return false;
        return true;
      }();

      if (isTmp) {
        Value *freshVal = builder.CreateLoad(ty, src, name + ".fresh");
        builder.CreateStore(freshVal, alloca);
        llvm::StructType *st = TypeResolver::getStringType(context);
        Value *dataPtr = builder.CreateStructGEP(st, srcAI, 0, "tmp.data");
        builder.CreateStore(
            ConstantPointerNull::get(PointerType::get(context, 0)), dataPtr);
      } else {
        Value *fresh = StringOps::clone(builder, context, module.get(), src);
        Value *freshVal = builder.CreateLoad(ty, fresh, name + ".fresh");
        builder.CreateStore(freshVal, alloca);
        if (auto *ai = dyn_cast<AllocaInst>(fresh)) {
          llvm::StructType *st = TypeResolver::getStringType(context);
          Value *dataPtr = builder.CreateStructGEP(st, ai, 0, "tmp.data");
          builder.CreateStore(
              ConstantPointerNull::get(PointerType::get(context, 0)), dataPtr);
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
      if (isNew && init->getType()->isPointerTy())
        builder.CreateCall(getFree(), {init});
      vi.ownsHeap = isNew;

    } else if (ty->isStructTy()) {
      Value *structVal = init->getType()->isPointerTy()
                             ? builder.CreateLoad(ty, init, name + ".structval")
                             : init;
      builder.CreateStore(structVal, alloca);
      vi.ownsHeap = false;
      if (auto *st = llvm::dyn_cast<llvm::StructType>(ty)) {
        for (unsigned i = 0; i < st->getNumElements(); ++i) {
          if (TypeResolver::isString(st->getElementType(i)) ||
              TypeResolver::isArray(st->getElementType(i))) {
            vi.ownsHeap = true;
            break;
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
      builder.CreateStore(init, alloca);
    }
    srcInfo.isMoved = true;
    vi.ownsHeap = srcInfo.ownsHeap;
    srcInfo.ownsHeap = false;
    break;
  }

  case AssignKind::Borrow: {
    Value *src = init;
    llvm::Value *srcAlloca = nullptr;
    llvm::Type *srcTy = ty;
    if (dyn_cast<AllocaInst>(src) || src->getType()->isPointerTy()) {
      srcAlloca = src;
    } else if (!srcName.empty()) {
      auto it = namedValues.find(srcName);
      if (it != namedValues.end()) {
        srcAlloca = it->second.allocaInst;
        srcTy = it->second.type;
      }
    }
    if (!srcAlloca)
      return logError("Borrow requires an addressable expression");
    vi = {srcAlloca, srcTy, true, false};
    vi.ownsHeap = false;
    if (!srcName.empty() && namedValues.count(srcName))
      namedValues[srcName].isBorrowed = true;
    break;
  }
  }

  namedValues[name] = vi;
  scopeMgr.declare(name);
  return alloca;
}

/*---------------------------------------*/
/*             Scope block               */
/*---------------------------------------*/

/**
 * Generates IR for a block (compound statement).
 * A new scope is pushed so that variables declared inside are destroyed
 * (and their heap buffers freed) when the block exits.
 * @param b the block AST node
 * @return the value produced by the last statement, or nullptr
 */
Value *CodeGenerator::visitBlock(const Block &b) {
  scopeMgr.pushScope();
  Value *last = nullptr;
  for (const auto &stmt : b.statements) {
    last = codegen(*stmt);
    if (blockHasTerminator(builder))
      break;
  }
  scopeMgr.popScope();
  return last;
}

/*---------------------------------------*/
/*       Control flow statements         */
/*---------------------------------------*/

/**
 * Generates IR for an if / else statement.
 * Creates three basic blocks: then, else, and merge. The condition is
 * coerced to i1 if needed. Missing else branches fall straight to merge.
 * @param s the if-statement AST node
 * @return always nullptr (control flow statements do not produce values)
 */
Value *CodeGenerator::visitIfStmt(const IfStmt &s) {
  Value *cond = codegen(*s.condition);
  if (!cond)
    return nullptr;

  // Coerce non-boolean condition to i1.
  if (!cond->getType()->isIntegerTy(1))
    cond = builder.CreateICmpNE(cond, ConstantInt::get(cond->getType(), 0),
                                "tobool");

  llvm::Function *fn = builder.GetInsertBlock()->getParent();
  BasicBlock *thenBB = BasicBlock::Create(context, "then", fn);
  BasicBlock *elseBB = BasicBlock::Create(context, "else");
  BasicBlock *mergeBB = BasicBlock::Create(context, "merge");

  builder.CreateCondBr(cond, thenBB, elseBB);

  // Emit the 'then' branch.
  builder.SetInsertPoint(thenBB);
  codegen(*s.thenBranch);
  if (!blockHasTerminator(builder))
    builder.CreateBr(mergeBB);

  // Emit the 'else' branch (may be empty).
  fn->insert(fn->end(), elseBB);
  builder.SetInsertPoint(elseBB);
  if (s.elseBranch)
    codegen(*s.elseBranch);
  if (!blockHasTerminator(builder))
    builder.CreateBr(mergeBB);

  fn->insert(fn->end(), mergeBB);
  builder.SetInsertPoint(mergeBB);
  return nullptr;
}

/**
 * Generates IR for a while loop.
 * Structure:  entry → cond → body → cond (back-edge) → exit.
 * break/continue use the loop stack to branch to exit/cond respectively.
 * @param s the while-statement AST node
 * @return always nullptr
 */
Value *CodeGenerator::visitWhileStmt(const WhileStmt &s) {
  llvm::Function *fn = builder.GetInsertBlock()->getParent();
  BasicBlock *condBB = BasicBlock::Create(context, "while.cond", fn);
  BasicBlock *bodyBB = BasicBlock::Create(context, "while.body");
  BasicBlock *exitBB = BasicBlock::Create(context, "while.exit");

  builder.CreateBr(condBB);

  // Condition block: evaluate the loop guard and branch.
  builder.SetInsertPoint(condBB);
  Value *cond = codegen(*s.condition);
  if (!cond)
    return nullptr;
  if (!cond->getType()->isIntegerTy(1))
    cond = builder.CreateICmpNE(cond, ConstantInt::get(cond->getType(), 0),
                                "tobool");
  builder.CreateCondBr(cond, bodyBB, exitBB);

  // Body block.
  fn->insert(fn->end(), bodyBB);
  builder.SetInsertPoint(bodyBB);
  loopStack.push_back({condBB, exitBB});
  codegen(*s.doBranch);
  loopStack.pop_back();
  if (!blockHasTerminator(builder))
    builder.CreateBr(condBB);

  fn->insert(fn->end(), exitBB);
  builder.SetInsertPoint(exitBB);
  return nullptr;
}

/**
 * Generates IR for a Match statement.
 *
 * Reads the tag field of the subject enum, then dispatches to the
 * appropriate arm via an LLVM switch instruction. For arms with payload
 * bindings, the variant struct is looked up using the mangled name
 * (enumName$typeArg_VariantName) so generic enums like Option<Animal>
 * resolve correctly.
 *
 * FIX (Bug 2): The variant struct lookup now tries both the plain name
 * (EnumName_Variant) and the mangled generic name (EnumName$T_Variant)
 * so that bindings inside match arms work for instantiated generic enums.
 *
 * FIX (Bug 4): The tag value search now checks ed->name == arm.enumName
 * before accepting a variant match, preventing wrong tag assignment when
 * multiple enums share a variant name.
 *
 * @param s the Match statement AST node
 * @return always nullptr
 */
Value *CodeGenerator::visitMatchStmt(const MatchStmt &s) {
  Value *subjectPtr = codegen(*s.subject);
  if (!subjectPtr)
    return nullptr;

  llvm::Type *i32 = Type::getInt32Ty(context);
  llvm::Function *fn = builder.GetInsertBlock()->getParent();
  llvm::BasicBlock *exitBB = llvm::BasicBlock::Create(context, "match.exit");

  llvm::BasicBlock *defaultBB = exitBB; // fall-through when no wildcard
  std::vector<std::pair<llvm::ConstantInt *, llvm::BasicBlock *>> cases;
  std::vector<std::pair<const MatchArm *, llvm::BasicBlock *>> armBlocks;

  for (const auto &arm : s.arms) {
    std::string label =
        arm.isWildcard ? "match.wildcard" : "match.arm." + arm.variantName;
    auto *bb = llvm::BasicBlock::Create(context, label, fn);
    armBlocks.push_back({&arm, bb});

    if (arm.isWildcard) {
      defaultBB = bb;
    } else {
      int tagVal = -1;

      // First try the enumTagValues cache keyed by "EnumName::VariantName".
      {
        std::string key = arm.enumName + "::" + arm.variantName;
        auto tvIt = enumTagValues.find(key);
        if (tvIt != enumTagValues.end())
          tagVal = static_cast<int>(tvIt->second);
      }

      // FIX (Bug 4): If not cached, search by matching both the enum name
      // and the variant name — do NOT accept a match on variant name alone,
      // as that picks the wrong tag when two enums share a variant name.
      if (tagVal < 0) {
        for (const auto &ed : currentProgram->enums) {
          // BUG FIX: was missing this enum-name guard; now requires the enum
          // name to match arm.enumName before accepting the variant.
          if (ed->name != arm.enumName)
            continue;
          for (size_t vi = 0; vi < ed->variants.size(); ++vi) {
            if (ed->variants[vi].name == arm.variantName) {
              tagVal = static_cast<int>(vi);
              break;
            }
          }
          if (tagVal >= 0)
            break;
        }
      }

      if (tagVal < 0) {
        logError(("match: unknown variant '" + arm.variantName + "'").c_str());
        return nullptr;
      }
      cases.push_back(std::make_pair(
          llvm::cast<llvm::ConstantInt>(llvm::ConstantInt::get(i32, tagVal)),
          bb));
    }
  }

  llvm::StructType *tagView =
      llvm::StructType::get(context, {i32}, /*isPacked=*/false);
  Value *tagPtr =
      builder.CreateStructGEP(tagView, subjectPtr, 0, "match.tag.ptr");
  Value *tag = builder.CreateLoad(i32, tagPtr, "match.tag");

  auto *sw =
      builder.CreateSwitch(tag, defaultBB, static_cast<unsigned>(cases.size()));
  for (auto &[val, bb] : cases)
    sw->addCase(val, bb);

  for (auto &[arm, bb] : armBlocks) {
    fn->insert(fn->end(), bb);
    builder.SetInsertPoint(bb);

    if (!arm->isWildcard && !arm->bindings.empty()) {
      // FIX (Bug 2): For generic enums (e.g. Option<Animal>), the per-variant
      // struct is named "Option$Animal_Some", not "Option_Some". Try the plain
      // name first, then scan all identified struct types for one whose name
      // starts with the enum name (with either '_' or '$' separator) and ends
      // with '_VariantName', to handle both concrete and instantiated enums.
      std::string plainName = arm->enumName + "_" + arm->variantName;
      llvm::StructType *variantSt =
          llvm::StructType::getTypeByName(context, plainName);

      if (!variantSt) {
        // Search for a mangled variant struct: EnumName$TypeArg_VariantName
        std::string suffix = "_" + arm->variantName;
        for (auto *st : module->getIdentifiedStructTypes()) {
          std::string stName = st->getName().str();
          // Must start with the enum name and end with _VariantName.
          if (stName.rfind(arm->enumName, 0) == 0 &&
              stName.size() > suffix.size() &&
              stName.compare(stName.size() - suffix.size(), suffix.size(),
                             suffix) == 0) {
            variantSt = st;
            break;
          }
        }
      }

      if (variantSt) {
        for (size_t fi = 0; fi < arm->bindings.size(); ++fi) {
          // Field 0 is the tag; payload starts at index 1.
          unsigned fieldIdx = static_cast<unsigned>(fi) + 1;
          if (fieldIdx >= variantSt->getNumElements())
            break;
          llvm::Type *fieldTy = variantSt->getElementType(fieldIdx);
          Value *fieldPtr = builder.CreateStructGEP(
              variantSt, subjectPtr, fieldIdx, arm->bindings[fi] + ".fptr");
          AllocaInst *alloca = createEntryAlloca(fieldTy, arm->bindings[fi]);
          Value *loaded =
              builder.CreateLoad(fieldTy, fieldPtr, arm->bindings[fi]);
          builder.CreateStore(loaded, alloca);
          namedValues[arm->bindings[fi]] =
              VarInfo(alloca, fieldTy, false, false, false, false);
        }
      }
    }

    // Codegen the arm body statements.
    for (const auto &stmt : arm->body->statements)
      stmt->accept(*this);

    // Branch to exit if the arm didn't already terminate.
    if (!builder.GetInsertBlock()->getTerminator())
      builder.CreateBr(exitBB);
  }

  fn->insert(fn->end(), exitBB);
  builder.SetInsertPoint(exitBB);
  return nullptr;
}

/**
 * Generates IR for a for-range loop (for i in start..end [step s]).
 * Structure: entry → cond → body → step → cond (back-edge) → exit.
 *
 * The loop variable is allocated at the entry block (enabling mem2reg
 * promotion). The step defaults to 1; when the step is a runtime value,
 * a dynamic comparison selects between forward and backward iteration.
 *
 * @param s the for-range statement AST node
 * @return always nullptr
 */
Value *CodeGenerator::visitForRange(const ForRangeStmt &s) {
  const std::string &vname = s.varName.token.getWord();

  Type *varTy = TypeResolver::fromTypeDesc(context, s.varType);
  if (!varTy)
    varTy = Type::getInt32Ty(context);

  Value *startVal = codegen(*s.start);
  Value *endVal = codegen(*s.end);
  Value *stepVal = s.step ? codegen(*s.step) : ConstantInt::get(varTy, 1);
  if (!startVal || !endVal || !stepVal)
    return nullptr;

  startVal = TypeResolver::coerce(builder, startVal, varTy);
  endVal = TypeResolver::coerce(builder, endVal, varTy);
  stepVal = TypeResolver::coerce(builder, stepVal, varTy);

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

  // Condition block: compare loop variable against end value.
  builder.SetInsertPoint(condBB);
  Value *cur = builder.CreateLoad(varTy, varAlloca, vname + ".cur");
  Value *cond;
  bool isFloat = varTy->isFloatingPointTy();
  if (isFloat) {
    cond = builder.CreateFCmpOLT(cur, endVal, "for.cond.f");
  } else {
    if (auto *cs = llvm::dyn_cast<llvm::ConstantInt>(stepVal)) {
      // Constant step: the direction is known at compile time.
      cond = cs->getSExtValue() >= 0
                 ? builder.CreateICmpSLT(cur, endVal, "for.cond.lt")
                 : builder.CreateICmpSGT(cur, endVal, "for.cond.gt");
    } else {
      // Dynamic step: emit a select to pick the right comparison at runtime.
      Value *zero = ConstantInt::get(varTy, 0);
      Value *stepPos = builder.CreateICmpSGT(stepVal, zero, "step.pos");
      Value *ltEnd = builder.CreateICmpSLT(cur, endVal, "cur.lt.end");
      Value *gtEnd = builder.CreateICmpSGT(cur, endVal, "cur.gt.end");
      cond = builder.CreateSelect(stepPos, ltEnd, gtEnd, "for.cond.dyn");
    }
  }
  builder.CreateCondBr(cond, bodyBB, exitBB);

  // Body block.
  fn->insert(fn->end(), bodyBB);
  builder.SetInsertPoint(bodyBB);
  loopStack.push_back({stepBB, exitBB});
  codegen(*s.body);
  loopStack.pop_back();
  bool needsBr = !blockHasTerminator(builder);
  if (needsBr)
    builder.CreateBr(stepBB);

  // Step block: increment the loop variable and branch back to the condition.
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

/**
 * Generates a branch to the innermost loop's exit block.
 * @return always nullptr
 */
Value *CodeGenerator::visitBreak(const Break &) {
  if (loopStack.empty())
    return logError("'break' outside loop");
  builder.CreateBr(loopStack.back().exitBB);
  return nullptr;
}

/**
 * Generates a branch to the innermost loop's condition block.
 * @return always nullptr
 */
Value *CodeGenerator::visitContinue(const Continue &) {
  if (loopStack.empty())
    return logError("'continue' outside loop");
  builder.CreateBr(loopStack.back().condBB);
  return nullptr;
}

/**
 * Generates IR for a return statement.
 * Before returning, the scope manager emits destructors for all live strings
 * so heap memory is freed. The return value is coerced to the function's
 * declared return type. A missing value yields a void return.
 * @param s the return statement AST node
 * @return always nullptr
 */
Value *CodeGenerator::visitReturn(const Return &s) {
  Value *retVal = nullptr;
  if (s.value)
    retVal = codegen(**s.value);

  if (auto *ai = llvm::dyn_cast_or_null<llvm::AllocaInst>(retVal)) {
    llvm::Type *allocTy = ai->getAllocatedType();

    if (auto *st = llvm::dyn_cast<llvm::StructType>(allocTy)) {
      llvm::Function *fn = builder.GetInsertBlock()->getParent();
      Type *retTy = fn->getReturnType();
      retVal = builder.CreateLoad(retTy, ai, "ret.load");

      for (unsigned i = 0; i < st->getNumElements(); ++i) {
        llvm::Type *elemTy = st->getElementType(i);
        if (TypeResolver::isString(elemTy) || TypeResolver::isArray(elemTy)) {
          llvm::StructType *innerSt =
              TypeResolver::isString(elemTy)
                  ? TypeResolver::getStringType(context)
                  : llvm::dyn_cast<llvm::StructType>(elemTy);
          if (innerSt) {
            Value *fieldGep = builder.CreateStructGEP(
                st, ai, i, "ret.field." + std::to_string(i));

            unsigned dataMemberIdx = TypeResolver::isString(elemTy) ? 0 : 1;

            Value *dataGep = builder.CreateStructGEP(
                innerSt, fieldGep, dataMemberIdx, "ret.null.data");
            builder.CreateStore(llvm::ConstantPointerNull::get(
                                    llvm::PointerType::get(context, 0)),
                                dataGep);
          }
        }
      }
    } else {
      for (auto &kv : namedValues) {
        if (kv.second.allocaInst == ai) {
          kv.second.isMoved = true;
          break;
        }
      }
    }
  }

  scopeMgr.emitAllDestructors();

  if (retVal) {
    llvm::Function *fn = builder.GetInsertBlock()->getParent();
    Type *retTy = fn->getReturnType();
    if (retVal->getType()->isPointerTy() &&
        (TypeResolver::isString(retTy) || TypeResolver::isArray(retTy) ||
         retTy->isStructTy()))
      retVal = builder.CreateLoad(retTy, retVal, "ret.load");
    else if (!retVal->getType()->isStructTy())
      retVal = TypeResolver::coerce(builder, retVal, retTy);
    builder.CreateRet(retVal);
  } else {
    builder.CreateRetVoid();
  }

  llvm::Function *fn = builder.GetInsertBlock()->getParent();
  llvm::BasicBlock *dead = llvm::BasicBlock::Create(context, "ret.dead", fn);
  builder.SetInsertPoint(dead);
  builder.CreateUnreachable();

  return nullptr;
}

/*---------------------------------------*/
/*          Function code generation     */
/*---------------------------------------*/

/**
 * Lowers a single function definition to an LLVM Function.
 *
 * Steps:
 *  1. Resolve the return type and parameter types.
 *  2. Look up or create the forward-declared LLVM Function.
 *  3. Create the entry basic block and initialise the scope.
 *  4. Materialise each parameter as an alloca at the entry block so that
 *     mem2reg can later promote them to SSA registers.
 *  5. Generate the function body.
 *  6. Append a fallthrough return if the last block has no terminator.
 *  7. Verify the function with LLVM's built-in checker.
 *
 * Reference parameters are stored as pointer allocas so that load/store
 * of the pointer itself is separate from load/store of the pointee.
 *
 * @param func the Function AST node to lower
 * @return the populated llvm::Function*, or nullptr on error
 */
llvm::Function *CodeGenerator::codegen(const AST_H::Function &func) {
  const std::string fname = mangleName(func.name.token.getWord(), func.params);

  Type *retTy = TypeResolver::fromTypeDesc(context, func.returnType);
  if (!retTy)
    retTy = llvm::StructType::getTypeByName(
        context, func.returnType.base.token.getWord());

  // Generic return types (e.g. Option<Animal>) must be instantiated if not
  // already present. The forward-declaration pass runs first so the mangled
  // struct should already exist; this is a safety fallback for both passes.
  if (!retTy && !func.returnType.typeArgs.empty()) {
    const std::string &retBase = func.returnType.base.token.getWord();
    std::vector<llvm::Type *> concreteArgs;
    std::vector<std::string> argNames;
    for (const auto &arg : func.returnType.typeArgs) {
      llvm::Type *t = TypeResolver::fromTypeDesc(context, arg);
      if (!t)
        t = llvm::StructType::getTypeByName(context, arg.base.token.getWord());
      if (t) {
        concreteArgs.push_back(t);
        argNames.push_back(arg.base.token.getWord());
      }
    }
    if (!concreteArgs.empty()) {
      retTy = instantiateGenericEnum(retBase, concreteArgs, argNames);
      if (!retTy)
        retTy = instantiateGenericStruct(func.returnType);
    }
  }

  if (!retTy && fname == "main")
    retTy = Type::getInt32Ty(context);
  if (!retTy)
    return nullptr;

  std::vector<Type *> paramTypes;
  std::vector<bool> paramIsRef;
  std::vector<bool> paramIsMut;

  for (const auto &p : func.params) {
    Type *pt = TypeResolver::fromTypeDesc(context, p.type);
    if (!pt)
      pt =
          llvm::StructType::getTypeByName(context, p.type.base.token.getWord());
    if (!pt)
      return nullptr;

    paramTypes.push_back(
        p.isBorrowRef || passAsPointer(pt) ? PointerType::get(context, 0) : pt);
    paramIsRef.push_back(p.isBorrowRef);
    paramIsMut.push_back(p.isBorrowRef && p.isMut);
  }
  borrowRefParams[fname] = paramIsRef;
  borrowMutParams[fname] = paramIsMut;

  llvm::Function *f = module->getFunction(fname);
  if (!f) {
    auto *ft = FunctionType::get(retTy, paramTypes, false);
    f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, fname,
                               *module);
  }
  if (!f->empty()) {
    logError(("Function already defined: " + fname).c_str());
    return nullptr;
  }

  f->addFnAttr("stackrealignment");

  BasicBlock *entry = BasicBlock::Create(context, "entry", f);
  builder.SetInsertPoint(entry);

  if (fname == "main")
    BuiltinEmitter::emitRuntimeInit(builder, context, module.get());

  // Restore global scope and reset per-function tracking.
  namedValues = globalValues;
  scopeMgr.reset();
  scopeMgr.pushScope();

  // Materialise parameters as allocas so mem2reg can promote them to SSA.
  size_t idx = 0;
  for (auto &arg : f->args()) {
    const auto &param = func.params[idx++];
    const std::string pname = param.name.token.getWord();

    if (param.isBorrowRef) {
      // Reference parameter: store the incoming pointer into a pointer alloca.
      AllocaInst *ptrAlloca = builder.CreateAlloca(PointerType::get(context, 0),
                                                   nullptr, pname + ".refptr");
      builder.CreateStore(&arg, ptrAlloca);

      Type *pointee = TypeResolver::fromTypeDesc(context, param.type);
      if (!pointee)
        pointee = llvm::StructType::getTypeByName(
            context, param.type.base.token.getWord());

      VarInfo vi(ptrAlloca, PointerType::get(context, 0), false, false, true,
                 !param.isMut);
      vi.pointeeType = pointee;
      namedValues[pname] = vi;
      scopeMgr.declare(pname);
    } else {
      Type *declaredTy = TypeResolver::fromTypeDesc(context, param.type);
      if (!declaredTy)
        declaredTy = llvm::StructType::getTypeByName(
            context, param.type.base.token.getWord());
      if (!declaredTy)
        declaredTy = arg.getType();

      AllocaInst *a = createEntryAlloca(declaredTy, pname);

      if (TypeResolver::isString(declaredTy) ||
          TypeResolver::isArray(declaredTy) || declaredTy->isStructTy()) {
        // Aggregate: load the entire value and store to the alloca.
        Value *val = builder.CreateLoad(declaredTy, &arg, pname + ".param");
        builder.CreateStore(val, a);
        VarInfo vi(a, declaredTy, false, false, false, param.isConst);
        vi.ownsHeap = false;
        namedValues[pname] = vi;
      } else {
        builder.CreateStore(&arg, a);
        namedValues[pname] =
            VarInfo(a, declaredTy, false, false, false, param.isConst);
      }
      scopeMgr.declare(pname);
    }
  }

  codegen(*func.body);

  // Emit a fallthrough return if the last block has no terminator.
  if (!blockHasTerminator(builder)) {
    scopeMgr.popAll();
    if (retTy->isVoidTy())
      builder.CreateRetVoid();
    else if (retTy->isFloatingPointTy())
      builder.CreateRet(llvm::ConstantFP::get(retTy, 0.0));
    else
      builder.CreateRet(ConstantInt::get(retTy, 0));
  }

  if (verifyFunction(*f, &errs())) {
    errs() << "verifyFunction failed for: " << fname << "\n";
    f->print(errs());
    f->eraseFromParent();
    return nullptr;
  }
  return f;
}

/*---------------------------------------*/
/*          Top-level generate           */
/*---------------------------------------*/

/**
 * Lowers an entire parsed program to an LLVM IR file (.ll).
 *
 * Steps performed in order:
 *  1. Declare C runtime functions (printf, strcmp, scanf).
 *  2. Forward-declare all struct types so they can cross-reference each other.
 *  3. Fill in struct bodies once all names are visible.
 *  4. Register extern block declarations.
 *  5. Emit global variable definitions with constant initialisers.
 *  6. Forward-declare all user functions so calls can precede definitions.
 *  7. Emit function bodies.
 *  8. Run the optimisation pipeline (O2).
 *  9. Write the resulting IR to <outputFilename>.ll.
 *
 * @param program the fully-parsed program AST
 * @param outputFilename the base path for the output file (without extension)
 * @return true on success, false if any step failed
 */
bool CodeGenerator::generate(const Program &program,
                             const std::string &outputFilename) {
  currentProgram = &program;
  namedValues.clear();
  structDefs.clear();
  for (const auto &s : program.structs)
    structDefs.push_back(s.get());

  Type *ptrTy = PointerType::get(context, 0);
  Type *i32 = Type::getInt32Ty(context);

  // Declare C runtime functions that the language always needs.
  auto declareIfAbsent = [&](const std::string &name, llvm::FunctionType *ft) {
    if (!module->getFunction(name))
      llvm::Function::Create(ft, llvm::Function::ExternalLinkage, name,
                             *module);
  };
  declareIfAbsent("printf", FunctionType::get(i32, {ptrTy}, true));
  declareIfAbsent("strcmp", FunctionType::get(i32, {ptrTy, ptrTy}, false));
  declareIfAbsent("scanf", FunctionType::get(i32, {ptrTy, ptrTy}, false));

  // Forward-declare all struct types so they can reference each other.
  for (const auto &s : program.structs)
    if (!llvm::StructType::getTypeByName(context, s->name))
      llvm::StructType::create(context, s->name);

  for (const auto &e : program.enums) {
    if (!e->typeParams.empty())
      continue;

    long long nextTag = 0;
    for (size_t vi = 0; vi < e->variants.size(); ++vi) {
      const auto &v = e->variants[vi];
      if (v.explicitVal.has_value())
        nextTag = *v.explicitVal;
      long long tagForThisVariant = nextTag++;
      enumTagValues[e->name + "::" + v.name] = tagForThisVariant;
      std::string sname = e->name + "_" + v.name;
      if (llvm::StructType::getTypeByName(context, sname))
        continue;
      std::vector<llvm::Type *> fields;
      fields.push_back(Type::getInt32Ty(context));
      for (const auto &f : v.fields) {
        llvm::Type *ft = TypeResolver::fromName(context, f.typeName);
        if (!ft) {
          ft = llvm::StructType::getTypeByName(context, f.typeName);
        }
        if (!ft) {
          errs() << "CodeGen warning: cannot resolve type '" << f.typeName
                 << "' for enum '" << e->name << "::" << v.name
                 << "' — defaulting to i32\n";
          ft = Type::getInt32Ty(context);
        }
        fields.push_back(ft);
      }
      auto *st = llvm::StructType::create(context, sname);
      st->setBody(fields);
    }
  }

  for (const auto &s : program.structs) {
    if (!s->typeParams.empty())
      continue;
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

  // Register extern block declarations.
  for (const auto &block : program.externBlocks) {
    for (const auto &decl : block.decls) {
      if (module->getFunction(decl.name))
        continue;
      std::vector<llvm::Type *> pts;
      for (const auto &p : decl.paramTypes) {
        const std::string tname = p.base.token.getWord();
        pts.push_back(p.isPtr || p.dimensions > 0 || tname == "str" ||
                              tname == "string"
                          ? llvm::PointerType::get(context, 0)
                          : TypeResolver::fromName(context, tname));
      }
      auto *retTy =
          TypeResolver::fromName(context, decl.returnType.base.token.getWord());
      llvm::Function::Create(llvm::FunctionType::get(retTy, pts, false),
                             llvm::Function::ExternalLinkage, decl.name,
                             *module);
    }
  }

  // Emit global variables.
  for (const auto &gv : program.globals) {
    llvm::Type *ty = TypeResolver::fromTypeDesc(context, gv->type);
    if (!ty) {
      logError(("Global variable '" + gv->name + "': unknown type '" +
                gv->type.base.token.getWord() + "'")
                   .c_str());
      return false;
    }

    // Evaluates a constant field expression (literals only; negation allowed).
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

      // Float constant may have been created as double — narrow it if needed.
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
        llvm::Constant *fc =
            evalConstField(slExpr->values[i].get(), st->getElementType(i));
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

  // Forward-declare all user functions so calls can precede their definitions.
  for (const auto &fn : program.functions) {
    const std::string fname = mangleName(fn->name.token.getWord(), fn->params);
    if (module->getFunction(fname))
      continue;

    Type *retTy = TypeResolver::fromTypeDesc(context, fn->returnType);
    if (!retTy)
      retTy = llvm::StructType::getTypeByName(
          context, fn->returnType.base.token.getWord());

    // For generic return types like Option<Animal>, TypeResolver and a plain
    // name lookup both fail because the instantiated struct hasn't been
    // registered yet under "Option". Instantiate it now so the forward
    // declaration gets the correct concrete return type instead of void.
    if (!retTy && !fn->returnType.typeArgs.empty()) {
      const std::string &retBase = fn->returnType.base.token.getWord();
      std::vector<llvm::Type *> concreteArgs;
      std::vector<std::string> argNames;
      for (const auto &arg : fn->returnType.typeArgs) {
        llvm::Type *t = TypeResolver::fromTypeDesc(context, arg);
        if (!t)
          t = llvm::StructType::getTypeByName(context,
                                              arg.base.token.getWord());
        if (t) {
          concreteArgs.push_back(t);
          argNames.push_back(arg.base.token.getWord());
        }
      }
      if (!concreteArgs.empty()) {
        // Try as a generic enum first, then as a generic struct.
        retTy = instantiateGenericEnum(retBase, concreteArgs, argNames);
        if (!retTy)
          retTy = instantiateGenericStruct(fn->returnType);
      }
    }

    if (!retTy && fname == "main")
      retTy = Type::getInt32Ty(context);
    if (!retTy)
      retTy = llvm::Type::getVoidTy(context);

    std::vector<Type *> paramTypes;
    for (const auto &p : fn->params) {
      Type *pt = TypeResolver::fromTypeDesc(context, p.type);
      if (!pt)
        pt = llvm::StructType::getTypeByName(context,
                                             p.type.base.token.getWord());
      if (!pt)
        continue;
      paramTypes.push_back(p.isBorrowRef || passAsPointer(pt)
                               ? PointerType::get(context, 0)
                               : pt);
    }
    llvm::Function::Create(FunctionType::get(retTy, paramTypes, false),
                           llvm::Function::ExternalLinkage, fname, *module);
  }

  // Emit function bodies.
  for (const auto &fn : program.functions) {
    const std::string fname = mangleName(fn->name.token.getWord(), fn->params);
    if (!codegen(*fn))
      return false;
  }

  std::error_code ec;
  raw_fd_ostream out(outputFilename + ".ll", ec, sys::fs::OF_None);
  if (ec) {
    errs() << "Cannot open output file: " << ec.message() << "\n";
    return false;
  }
  module->print(out, nullptr);
  return true;
}
