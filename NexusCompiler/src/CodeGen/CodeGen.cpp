#include "CodeGen.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include <memory>
#include <regex>
#include <unordered_map>

using namespace llvm;

// ─────────────────────────────────────────────────────────────────────────────
// Static helpers
// ─────────────────────────────────────────────────────────────────────────────

static std::string normalizeFunctionName(const std::string &name) {
  static const std::unordered_map<std::string, std::string> kMap = {
      {"Main", "main"},
      {"Printf", "printf"},
      {"Print", "print_literal"},
      {"printf", "printf"},
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

// ─────────────────────────────────────────────────────────────────────────────
// TypeResolver  –  converts language type names to LLVM types
// ─────────────────────────────────────────────────────────────────────────────
class TypeResolver {
public:
  // ── Scalar / built-in types ───────────────────────────────────────────────
  static Type *fromName(LLVMContext &ctx, const std::string &t) {
    if (t == "i32" || t == "int" || t == "integer")
      return Type::getInt32Ty(ctx);
    if (t == "i64" || t == "long")
      return Type::getInt64Ty(ctx);
    if (t == "i16" || t == "short")
      return Type::getInt16Ty(ctx);
    if (t == "i8" || t == "char")
      return Type::getInt8Ty(ctx);
    if (t == "f32" || t == "float")
      return Type::getFloatTy(ctx);
    if (t == "f64" || t == "double")
      return Type::getDoubleTy(ctx);
    if (t == "bool")
      return Type::getInt1Ty(ctx);
    if (t == "void")
      return Type::getVoidTy(ctx);

    if (t == "str" || t == "string")
      return getStringType(ctx);

    // "array.T"  or  "array.array.T"
    if (t.size() > 6 && t.substr(0, 6) == "array.")
      return getOrCreateArrayStruct(ctx, t);

    return nullptr;
  }

  static Type *fromTypeDesc(LLVMContext &ctx, const TypeDesc &td) {
    return fromName(ctx, td.fullName());
  }

  // ── String struct:  { i8* data, i64 length, i64 capacity } ───────────────
  static StructType *getStringType(LLVMContext &ctx) {
    StructType *st = StructType::getTypeByName(ctx, "string");
    if (!st) {
      st = StructType::create(ctx, "string");
      st->setBody({PointerType::get(ctx, 0), Type::getInt64Ty(ctx),
                   Type::getInt64Ty(ctx)});
    }
    return st;
  }

  // ── Array struct:  { i64 length, i8* data } ──────────────────────────────
  static StructType *getOrCreateArrayStruct(LLVMContext &ctx,
                                            const std::string &name) {
    StructType *st = StructType::getTypeByName(ctx, name);
    if (!st) {
      st = StructType::create(ctx, name);
      st->setBody({Type::getInt64Ty(ctx), PointerType::get(ctx, 0)});
    }
    return st;
  }

  // ── Predicates ────────────────────────────────────────────────────────────
  static bool isString(Type *ty) {
    if (!ty || !ty->isStructTy())
      return false;
    return cast<StructType>(ty)->getName() == "string";
  }

  static bool isArray(Type *ty) {
    if (!ty || !ty->isStructTy())
      return false;
    return cast<StructType>(ty)->getName().starts_with("array.");
  }

  static bool isNumeric(Type *ty) {
    return ty && (ty->isIntegerTy() || ty->isFloatingPointTy());
  }

  // ── Element type of an array struct ──────────────────────────────────────
  // "array.i32"        → i32
  // "array.array.i32"  → array.i32  struct
  static Type *elemType(LLVMContext &ctx, StructType *arrTy) {
    StringRef name = arrTy->getName();
    if (!name.starts_with("array."))
      return nullptr;
    return fromName(ctx, name.substr(6).str());
  }

  // ── Numeric promotion / conversion ───────────────────────────────────────
  static Type *largerType(Type *a, Type *b) {
    if (!a || !b)
      return a ? a : b;
    if (a == b)
      return a;
    if (isArray(a) || isArray(b) || isString(a) || isString(b))
      return a;
    if (a->isPointerTy() || b->isPointerTy())
      return a->isPointerTy() ? a : b;
    if (a->isFloatingPointTy() || b->isFloatingPointTy()) {
      if (a->isDoubleTy() || b->isDoubleTy())
        return Type::getDoubleTy(a->getContext());
      return Type::getFloatTy(a->getContext());
    }
    if (a->isIntegerTy() && b->isIntegerTy()) {
      unsigned bits =
          std::max({a->getIntegerBitWidth(), b->getIntegerBitWidth(), 32u});
      return Type::getIntNTy(a->getContext(), bits);
    }
    return a;
  }

  static Value *coerce(IRBuilder<> &B, Value *val, Type *target) {
    if (!val || !target)
      return val;
    Type *src = val->getType();
    if (src == target)
      return val;
    if (target->isPointerTy() && src->isPointerTy())
      return val;

    if (target->isFloatingPointTy()) {
      if (src->isFloatingPointTy()) {
        if (target->isDoubleTy() && src->isFloatTy())
          return B.CreateFPExt(val, target, "f2d");
        if (target->isFloatTy() && src->isDoubleTy())
          return B.CreateFPTrunc(val, target, "d2f");
      } else if (src->isIntegerTy()) {
        return B.CreateSIToFP(val, target,
                              target->isDoubleTy() ? "i2d" : "i2f");
      }
    }
    if (target->isIntegerTy()) {
      if (src->isIntegerTy()) {
        unsigned tb = target->getIntegerBitWidth(),
                 sb = src->getIntegerBitWidth();
        if (tb > sb)
          return B.CreateSExt(val, target, "ext");
        if (tb < sb)
          return B.CreateTrunc(val, target, "trunc");
      } else if (src->isFloatingPointTy()) {
        return B.CreateFPToSI(val, target, "f2i");
      }
    }
    return val;
  }

  // ── String data pointer ───────────────────────────────────────────────────
  static Value *stringDataPtr(IRBuilder<> &B, LLVMContext &ctx,
                              Value *strAlloca) {
    StructType *st = getStringType(ctx);
    Value *gep = B.CreateStructGEP(st, strAlloca, 0, "str.data.gep");
    return B.CreateLoad(PointerType::get(ctx, 0), gep, "str.data");
  }

private:
  static std::string typeName(Type *ty) {
    if (ty->isIntegerTy(64))
      return "i64";
    if (ty->isIntegerTy(32))
      return "i32";
    if (ty->isIntegerTy(16))
      return "i16";
    if (ty->isIntegerTy(8))
      return "i8";
    if (ty->isIntegerTy(1))
      return "bool";
    if (ty->isFloatTy())
      return "f32";
    if (ty->isDoubleTy())
      return "f64";
    if (auto *st = dyn_cast<StructType>(ty))
      return st->getName().str();
    return "unknown";
  }
};

// ─────────────────────────────────────────────────────────────────────────────
// Runtime function declarations
// ─────────────────────────────────────────────────────────────────────────────
namespace RTDecl {

static llvm::Function *malloc_(Module *M, LLVMContext &ctx) {
  llvm::Function *f = M->getFunction("malloc");
  if (!f) {
    auto *ft = FunctionType::get(PointerType::get(ctx, 0),
                                 {Type::getInt64Ty(ctx)}, false);
    f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "malloc",
                               M);
  }
  return f;
}

static llvm::Function *memcpy_(Module *M, LLVMContext &ctx) {
  const std::string name = "llvm.memcpy.p0.p0.i64";
  llvm::Function *f = M->getFunction(name);
  if (!f) {
    Type *ptrTy = PointerType::get(ctx, 0);
    auto *ft = FunctionType::get(
        Type::getVoidTy(ctx),
        {ptrTy, ptrTy, Type::getInt64Ty(ctx), Type::getInt1Ty(ctx)}, false);
    f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, name, M);
    f->addFnAttr(Attribute::NoUnwind);
  }
  return f;
}

static llvm::Function *sprintf_(Module *M, LLVMContext &ctx) {
  llvm::Function *f = M->getFunction("sprintf");
  if (!f) {
    Type *ptrTy = PointerType::get(ctx, 0);
    auto *ft = FunctionType::get(Type::getInt32Ty(ctx), {ptrTy, ptrTy}, true);
    f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "sprintf",
                               M);
  }
  return f;
}

static llvm::Function *strlen_(Module *M, LLVMContext &ctx) {
  llvm::Function *f = M->getFunction("strlen");
  if (!f) {
    auto *ft = FunctionType::get(Type::getInt64Ty(ctx),
                                 {PointerType::get(ctx, 0)}, false);
    f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "strlen",
                               M);
  }
  return f;
}

} // namespace RTDecl

// ─────────────────────────────────────────────────────────────────────────────
// StringOps  –  LLVM-IR level string operations
// ─────────────────────────────────────────────────────────────────────────────
class StringOps {
public:
  /// Build a heap-allocated string struct from a C-string literal.
  static Value *fromLiteral(IRBuilder<> &B, LLVMContext &ctx, Module *M,
                            const std::string &literal) {
    Value *ptr = B.CreateGlobalString(literal, "strl");
    Value *len = ConstantInt::get(Type::getInt64Ty(ctx), literal.size());
    return fromParts(B, ctx, M, ptr, len);
  }

  /// Build a string struct from raw ptr + length (deep-copies into malloc).
  static Value *fromParts(IRBuilder<> &B, LLVMContext &ctx, Module *M,
                          Value *dataPtr, Value *length) {
    Type *i64 = Type::getInt64Ty(ctx);
    Value *cap = B.CreateAdd(length, ConstantInt::get(i64, 1), "cap");
    Value *mem = B.CreateCall(RTDecl::malloc_(M, ctx), {cap}, "str.alloc");

    B.CreateCall(RTDecl::memcpy_(M, ctx),
                 {mem, dataPtr, length, ConstantInt::getFalse(ctx)});
    // null-terminate
    Value *nullPos = B.CreateGEP(Type::getInt8Ty(ctx), mem, length);
    B.CreateStore(ConstantInt::get(Type::getInt8Ty(ctx), 0), nullPos);

    StructType *st = TypeResolver::getStringType(ctx);
    AllocaInst *s = B.CreateAlloca(st, nullptr, "string");
    B.CreateStore(mem, B.CreateStructGEP(st, s, 0));
    B.CreateStore(length, B.CreateStructGEP(st, s, 1));
    B.CreateStore(cap, B.CreateStructGEP(st, s, 2));
    return s;
  }

  /// Concatenate two string structs.
  static Value *concat(IRBuilder<> &B, LLVMContext &ctx, Module *M,
                       Value *leftStruct, Value *rightStruct) {
    StructType *st = TypeResolver::getStringType(ctx);
    Value *lv = B.CreateLoad(st, leftStruct, "ls");
    Value *rv = B.CreateLoad(st, rightStruct, "rs");
    Value *ld = B.CreateExtractValue(lv, {0}, "ld");
    Value *ll = B.CreateExtractValue(lv, {1}, "ll");
    Value *rd = B.CreateExtractValue(rv, {0}, "rd");
    Value *rl = B.CreateExtractValue(rv, {1}, "rl");

    Type *i64 = Type::getInt64Ty(ctx);
    Value *total = B.CreateAdd(ll, rl, "tlen");
    Value *cap = B.CreateAdd(total, ConstantInt::get(i64, 1), "tcap");
    Value *mem = B.CreateCall(RTDecl::malloc_(M, ctx), {cap}, "cat.alloc");

    llvm::Function *mc = RTDecl::memcpy_(M, ctx);
    B.CreateCall(mc, {mem, ld, ll, ConstantInt::getFalse(ctx)});
    Value *mid = B.CreateGEP(Type::getInt8Ty(ctx), mem, ll);
    B.CreateCall(mc, {mid, rd, rl, ConstantInt::getFalse(ctx)});
    Value *null = B.CreateGEP(Type::getInt8Ty(ctx), mem, total);
    B.CreateStore(ConstantInt::get(Type::getInt8Ty(ctx), 0), null);

    AllocaInst *res = B.CreateAlloca(st, nullptr, "cat.res");
    B.CreateStore(mem, B.CreateStructGEP(st, res, 0));
    B.CreateStore(total, B.CreateStructGEP(st, res, 1));
    B.CreateStore(cap, B.CreateStructGEP(st, res, 2));
    return res;
  }

  /// Convert any scalar value to a string struct.
  static Value *fromValue(IRBuilder<> &B, LLVMContext &ctx, Module *M,
                          Value *val) {
    Type *ty = val->getType();
    if (TypeResolver::isString(ty))
      return val;
    if (ty->isIntegerTy())
      return intToStr(B, ctx, M, val);
    if (ty->isFloatingPointTy())
      return floatToStr(B, ctx, M, val);
    if (ty->isPointerTy()) {
      Value *len = B.CreateCall(RTDecl::strlen_(M, ctx), {val}, "sl");
      return fromParts(B, ctx, M, val, len);
    }
    return fromLiteral(B, ctx, M, "[unprintable]");
  }

private:
  static Value *intToStr(IRBuilder<> &B, LLVMContext &ctx, Module *M,
                         Value *v) {
    if (v->getType()->getIntegerBitWidth() < 64)
      v = B.CreateSExt(v, Type::getInt64Ty(ctx));
    Value *fmt = B.CreateGlobalString("%lld", "ifmt");
    auto *buf = B.CreateAlloca(llvm::ArrayType::get(Type::getInt8Ty(ctx), 32),
                               nullptr, "ibuf");
    B.CreateCall(RTDecl::sprintf_(M, ctx), {buf, fmt, v});
    Value *len = B.CreateCall(RTDecl::strlen_(M, ctx), {buf}, "il");
    return fromParts(B, ctx, M, buf, len);
  }

  static Value *floatToStr(IRBuilder<> &B, LLVMContext &ctx, Module *M,
                           Value *v) {
    if (v->getType()->isFloatTy())
      v = B.CreateFPExt(v, Type::getDoubleTy(ctx));
    Value *fmt = B.CreateGlobalString("%g", "ffmt");
    auto *buf = B.CreateAlloca(llvm::ArrayType::get(Type::getInt8Ty(ctx), 64),
                               nullptr, "fbuf");
    B.CreateCall(RTDecl::sprintf_(M, ctx), {buf, fmt, v});
    Value *len = B.CreateCall(RTDecl::strlen_(M, ctx), {buf}, "fl");
    return fromParts(B, ctx, M, buf, len);
  }
};

// ─────────────────────────────────────────────────────────────────────────────
// String-interpolation helpers
// ─────────────────────────────────────────────────────────────────────────────

struct FmtArg {
  std::string spec;
  Value *value;
};

/// Try to evaluate a simple {ident} or {number} interpolation expression.
static Value *evalInterp(const std::string &inner, LLVMContext &ctx,
                         IRBuilder<> &B,
                         const std::map<std::string, VarInfo> &vars) {
  // Simple identifier
  bool isIdent = !inner.empty();
  for (char c : inner)
    if (!std::isalnum((unsigned char)c) && c != '_') {
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
  // Integer constant
  try {
    size_t pos;
    long long v = std::stoll(inner, &pos);
    if (pos == inner.size())
      return ConstantInt::get(Type::getInt32Ty(ctx), v);
  } catch (...) {
  }
  // Float constant
  try {
    size_t pos;
    double v = std::stod(inner, &pos);
    if (pos == inner.size())
      return ConstantFP::get(Type::getDoubleTy(ctx), v);
  } catch (...) {
  }
  return nullptr;
}

/// Process a Printf format string with {expr} interpolations.
/// Writes printf format specifiers into `fmt` and argument values into `args`.
static std::string buildPrintfFmt(const std::string &raw, LLVMContext &ctx,
                                  IRBuilder<> &B,
                                  const std::map<std::string, VarInfo> &vars,
                                  std::vector<FmtArg> &args) {
  std::string fmt;
  size_t i = 0;
  while (i < raw.size()) {
    if (raw[i] == '\\' && i + 1 < raw.size() && raw[i + 1] == '{') {
      fmt += '{';
      i += 2;
      continue;
    }
    if (raw[i] != '{') {
      if (raw[i] == '%')
        fmt += '%';
      fmt += raw[i++];
      continue;
    }
    if (i + 1 < raw.size() && raw[i + 1] == '{') {
      fmt += '{';
      i += 2;
      continue;
    }
    // Scan to matching '}'
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
      fmt += raw[i++];
      continue;
    }

    std::string inner = raw.substr(start, j - start);
    Value *val = evalInterp(inner, ctx, B, vars);
    if (val) {
      Type *ty = val->getType();
      if (ty->isIntegerTy(1)) {
        Value *t = B.CreateGlobalString("true", "bt");
        Value *f = B.CreateGlobalString("false", "bf");
        args.push_back({"%s", B.CreateSelect(val, t, f, "bstr")});
        fmt += "%s";
      } else if (ty->isIntegerTy()) {
        if (ty->getIntegerBitWidth() < 64)
          val = B.CreateSExt(val, Type::getInt64Ty(ctx));
        args.push_back({"%lld", val});
        fmt += "%lld";
      } else if (ty->isFloatingPointTy()) {
        if (!ty->isDoubleTy())
          val = B.CreateFPExt(val, Type::getDoubleTy(ctx));
        args.push_back({"%g", val});
        fmt += "%g";
      } else if (ty->isPointerTy()) {
        args.push_back({"%s", val});
        fmt += "%s";
      } else {
        // Struct — assume string-like, extract data ptr
        if (auto *ai = dyn_cast<AllocaInst>(val)) {
          if (TypeResolver::isString(ai->getAllocatedType())) {
            StructType *st = cast<StructType>(ai->getAllocatedType());
            Value *loaded = B.CreateLoad(st, ai);
            Value *ptr = B.CreateExtractValue(loaded, {0});
            args.push_back({"%s", ptr});
            fmt += "%s";
          }
        }
      }
    } else {
      fmt += '{' + inner + '}';
    }
    i = j + 1;
  }
  return fmt;
}

/// Evaluate a string literal that may contain {expr} interpolations,
/// producing a string struct.
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

// ─────────────────────────────────────────────────────────────────────────────
// Array allocation helpers
// ─────────────────────────────────────────────────────────────────────────────
namespace ArrayAlloc {

/// Allocate a 1-D array struct on the stack (heap data via malloc).
static AllocaInst *make1D(IRBuilder<> &B, LLVMContext &ctx, Module *M,
                          Type *elemType, Value *count,
                          const std::string &name = "arr") {
  Type *i64 = Type::getInt64Ty(ctx);
  uint64_t esz = M->getDataLayout().getTypeAllocSize(elemType);
  Value *esize = ConstantInt::get(i64, esz);
  if (count->getType()->isIntegerTy(32))
    count = B.CreateSExt(count, i64);
  Value *bytes = B.CreateMul(count, esize, "arr.bytes");
  Value *data = B.CreateCall(RTDecl::malloc_(M, ctx), {bytes}, "arr.data");

  // Derive struct name from element type
  std::string sname = "array.";
  if (auto *st = dyn_cast<StructType>(elemType))
    sname += st->getName().str();
  else if (elemType->isIntegerTy(32))
    sname += "i32";
  else if (elemType->isIntegerTy(64))
    sname += "i64";
  else if (elemType->isIntegerTy(16))
    sname += "i16";
  else if (elemType->isIntegerTy(8))
    sname += "i8";
  else if (elemType->isIntegerTy(1))
    sname += "bool";
  else if (elemType->isFloatTy())
    sname += "f32";
  else if (elemType->isDoubleTy())
    sname += "f64";
  else
    sname += "unknown";

  StructType *st = TypeResolver::getOrCreateArrayStruct(ctx, sname);
  AllocaInst *a = B.CreateAlloca(st, nullptr, name);
  B.CreateStore(count, B.CreateStructGEP(st, a, 0));
  B.CreateStore(data, B.CreateStructGEP(st, a, 1));
  return a;
}

/// Allocate a 2-D array (array of 1-D array structs).
static AllocaInst *make2D(IRBuilder<> &B, LLVMContext &ctx, Module *M,
                          Type *elemType, Value *rows, Value *cols,
                          const std::string &name = "arr2d") {
  Type *i64 = Type::getInt64Ty(ctx);
  if (rows->getType()->isIntegerTy(32))
    rows = B.CreateSExt(rows, i64);
  if (cols->getType()->isIntegerTy(32))
    cols = B.CreateSExt(cols, i64);

  // Inner struct: array.T
  std::string innerName = "array.";
  if (auto *st = dyn_cast<StructType>(elemType))
    innerName += st->getName().str();
  else if (elemType->isIntegerTy(32))
    innerName += "i32";
  else if (elemType->isIntegerTy(64))
    innerName += "i64";
  else if (elemType->isIntegerTy(16))
    innerName += "i16";
  else if (elemType->isIntegerTy(8))
    innerName += "i8";
  else if (elemType->isFloatTy())
    innerName += "f32";
  else if (elemType->isDoubleTy())
    innerName += "f64";
  else
    innerName += "unknown";

  StructType *innerSt = TypeResolver::getOrCreateArrayStruct(ctx, innerName);
  std::string outerName = "array." + innerName;
  StructType *outerSt = TypeResolver::getOrCreateArrayStruct(ctx, outerName);

  // Allocate outer data: rows * sizeof(inner struct)
  uint64_t innerSz = M->getDataLayout().getTypeAllocSize(innerSt);
  Value *outerBytes = B.CreateMul(rows, ConstantInt::get(i64, innerSz));
  Value *outerData =
      B.CreateCall(RTDecl::malloc_(M, ctx), {outerBytes}, "arr2d.outer");

  // Loop to allocate each row
  llvm::Function *func = B.GetInsertBlock()->getParent();
  BasicBlock *init = BasicBlock::Create(ctx, "arr2d.init", func);
  BasicBlock *cond = BasicBlock::Create(ctx, "arr2d.cond", func);
  BasicBlock *body = BasicBlock::Create(ctx, "arr2d.body", func);
  BasicBlock *done = BasicBlock::Create(ctx, "arr2d.done", func);

  B.CreateBr(init);
  B.SetInsertPoint(init);
  AllocaInst *idx = B.CreateAlloca(i64, nullptr, "ri");
  B.CreateStore(ConstantInt::get(i64, 0), idx);
  B.CreateBr(cond);

  B.SetInsertPoint(cond);
  Value *i = B.CreateLoad(i64, idx, "i");
  Value *ok = B.CreateICmpSLT(i, rows, "rc");
  B.CreateCondBr(ok, body, done);

  B.SetInsertPoint(body);
  uint64_t eSz = M->getDataLayout().getTypeAllocSize(elemType);
  Value *rowBytes = B.CreateMul(cols, ConstantInt::get(i64, eSz), "rbytes");
  Value *rowData =
      B.CreateCall(RTDecl::malloc_(M, ctx), {rowBytes}, "row.data");

  Value *rowPtr = B.CreateGEP(innerSt, outerData, i, "row.ptr");
  B.CreateStore(cols, B.CreateStructGEP(innerSt, rowPtr, 0));
  B.CreateStore(rowData, B.CreateStructGEP(innerSt, rowPtr, 1));

  Value *ni = B.CreateAdd(i, ConstantInt::get(i64, 1));
  B.CreateStore(ni, idx);
  B.CreateBr(cond);

  B.SetInsertPoint(done);
  AllocaInst *out = B.CreateAlloca(outerSt, nullptr, name);
  B.CreateStore(rows, B.CreateStructGEP(outerSt, out, 0));
  B.CreateStore(outerData, B.CreateStructGEP(outerSt, out, 1));
  return out;
}

} // namespace ArrayAlloc

// ─────────────────────────────────────────────────────────────────────────────
// Colour helpers
// ─────────────────────────────────────────────────────────────────────────────

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
  std::regex re(R"(#([0-9a-fA-F]{6}))");
  std::string result;
  std::sregex_iterator it(input.begin(), input.end(), re), end;
  size_t last = 0;
  for (; it != end; ++it) {
    auto &m = *it;
    result += input.substr(last, m.position() - last);
    result += hexToAnsi(m.str());
    last = m.position() + m.length();
  }
  return result + input.substr(last);
}

// ─────────────────────────────────────────────────────────────────────────────
// CodeGenerator constructor
// ─────────────────────────────────────────────────────────────────────────────
CodeGenerator::CodeGenerator()
    : module(std::make_unique<Module>("nexus", context)), builder(context) {
  module->setTargetTriple(Triple(LLVM_HOST_TRIPLE));
}

bool CodeGenerator::isCStringPointer(Type *ty) { return ty->isPointerTy(); }

Value *CodeGenerator::logError(const char *msg) {
  errs() << "\033[31mCodeGen error: " << msg << "\033[0m\n";
  return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// Arithmetic helpers
// ─────────────────────────────────────────────────────────────────────────────
Value *CodeGenerator::promoteToDouble(Value *v) {
  if (v->getType()->isDoubleTy())
    return v;
  if (v->getType()->isFloatTy())
    return builder.CreateFPExt(v, Type::getDoubleTy(context), "f2d");
  return builder.CreateSIToFP(v, Type::getDoubleTy(context), "i2d");
}

Value *CodeGenerator::promoteToInt(Value *v) {
  if (v->getType()->isIntegerTy())
    return v;
  return builder.CreateFPToSI(v, Type::getInt32Ty(context), "f2i");
}

Value *CodeGenerator::generateComparison(BinaryOp op, Value *l, Value *r,
                                         bool isFloat) {
  if (isFloat) {
    l = promoteToDouble(l);
    r = promoteToDouble(r);
    switch (op) {
    case BinaryOp::Lt:
      return builder.CreateFCmpOLT(l, r, "flt");
    case BinaryOp::Le:
      return builder.CreateFCmpOLE(l, r, "fle");
    case BinaryOp::Gt:
      return builder.CreateFCmpOGT(l, r, "fgt");
    case BinaryOp::Ge:
      return builder.CreateFCmpOGE(l, r, "fge");
    case BinaryOp::Eq:
      return builder.CreateFCmpOEQ(l, r, "feq");
    case BinaryOp::Ne:
      return builder.CreateFCmpONE(l, r, "fne");
    default:
      return nullptr;
    }
  } else {
    switch (op) {
    case BinaryOp::Lt:
      return builder.CreateICmpSLT(l, r, "ilt");
    case BinaryOp::Le:
      return builder.CreateICmpSLE(l, r, "ile");
    case BinaryOp::Gt:
      return builder.CreateICmpSGT(l, r, "igt");
    case BinaryOp::Ge:
      return builder.CreateICmpSGE(l, r, "ige");
    case BinaryOp::Eq:
      return builder.CreateICmpEQ(l, r, "ieq");
    case BinaryOp::Ne:
      return builder.CreateICmpNE(l, r, "ine");
    default:
      return nullptr;
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Expression visitors
// ─────────────────────────────────────────────────────────────────────────────

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

Value *CodeGenerator::visitStrLit(const StrLitExpr &e) {
  std::string raw = unescapeString(e.lit.getWord());

  // Detect interpolations (skip escaped \{ )
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

  // No interpolation – collapse {{ → {
  std::string processed;
  for (size_t i = 0; i < raw.size(); ++i) {
    if (raw[i] == '{' && i + 1 < raw.size() && raw[i + 1] == '{') {
      processed += '{';
      ++i;
    } else {
      processed += raw[i];
    }
  }
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
    // Load the stored pointer then dereference
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

Value *CodeGenerator::visitBinary(const BinaryExpr &e) {
  // ── String concatenation and comparison ──────────────────────────────────
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

  if (e.op == BinaryOp::Add && (ls || rs)) {
    Value *l = codegen(*e.left), *r = codegen(*e.right);
    if (!l || !r)
      return nullptr;
    Value *lstr =
        ls ? l : StringOps::fromValue(builder, context, module.get(), l);
    Value *rstr =
        rs ? r : StringOps::fromValue(builder, context, module.get(), r);
    return StringOps::concat(builder, context, module.get(), lstr, rstr);
  }

  if ((e.op == BinaryOp::Eq || e.op == BinaryOp::Ne) && (ls || rs)) {
    Value *l = codegen(*e.left), *r = codegen(*e.right);
    if (!l || !r)
      return nullptr;
    auto charPtr = [&](Value *v) -> Value * {
      if (auto *ai = dyn_cast<AllocaInst>(v)) {
        Type *at = ai->getAllocatedType();
        if (TypeResolver::isString(at)) {
          Value *loaded = builder.CreateLoad(cast<StructType>(at), v);
          return builder.CreateExtractValue(loaded, {0});
        }
      }
      return v;
    };
    llvm::Function *strcmpF = module->getFunction("strcmp");
    if (!strcmpF)
      return logError("strcmp not declared");
    Value *cmp = builder.CreateCall(strcmpF, {charPtr(l), charPtr(r)}, "scmp");
    Value *eq =
        builder.CreateICmpEQ(cmp, ConstantInt::get(cmp->getType(), 0), "seq");
    return e.op == BinaryOp::Eq ? eq : builder.CreateNot(eq, "sne");
  }

  // ── Numeric ──────────────────────────────────────────────────────────────
  Value *l = codegen(*e.left), *r = codegen(*e.right);
  if (!l || !r)
    return nullptr;

  // Widen integers to same width
  if (l->getType()->isIntegerTy() && r->getType()->isIntegerTy()) {
    unsigned lb = l->getType()->getIntegerBitWidth();
    unsigned rb = r->getType()->getIntegerBitWidth();
    if (lb < rb)
      l = builder.CreateSExt(l, r->getType());
    else if (rb < lb)
      r = builder.CreateSExt(r, l->getType());
  }

  bool lf = l->getType()->isFloatingPointTy();
  bool rf = r->getType()->isFloatingPointTy();
  if (lf && r->getType()->isIntegerTy())
    r = builder.CreateSIToFP(r, Type::getDoubleTy(context));
  else if (rf && l->getType()->isIntegerTy())
    l = builder.CreateSIToFP(l, Type::getDoubleTy(context));

  switch (e.op) {
  case BinaryOp::Lt:
  case BinaryOp::Le:
  case BinaryOp::Gt:
  case BinaryOp::Ge:
  case BinaryOp::Eq:
  case BinaryOp::Ne:
    return generateComparison(e.op, l, r, lf || rf);
  case BinaryOp::Add:
    if (lf || rf) {
      return builder.CreateFAdd(promoteToDouble(l), promoteToDouble(r), "fadd");
    }
    return builder.CreateAdd(l, r, "add");
  case BinaryOp::Sub:
    if (lf || rf) {
      return builder.CreateFSub(promoteToDouble(l), promoteToDouble(r), "fsub");
    }
    return builder.CreateSub(l, r, "sub");
  case BinaryOp::Mul:
    if (lf || rf) {
      return builder.CreateFMul(promoteToDouble(l), promoteToDouble(r), "fmul");
    }
    return builder.CreateMul(l, r, "mul");
  case BinaryOp::Div:
    return builder.CreateFDiv(promoteToDouble(l), promoteToDouble(r), "fdiv");
  case BinaryOp::DivFloor:
    return builder.CreateSDiv(promoteToInt(l), promoteToInt(r), "sdiv");
  case BinaryOp::Mod:
    return builder.CreateSRem(promoteToInt(l), promoteToInt(r), "srem");
  default:
    return logError("Unknown binary op");
  }
}

Value *CodeGenerator::visitUnary(const UnaryExpr &e) {
  Value *v = codegen(*e.operand);
  if (!v)
    return nullptr;
  switch (e.op) {
  case UnaryOp::Negate:
    return v->getType()->isFloatingPointTy() ? builder.CreateFNeg(v, "fneg")
                                             : builder.CreateNeg(v, "neg");
  }
  return logError("Unknown unary op");
}

Value *CodeGenerator::visitAssign(const AssignExpr &e) {
  const std::string &tgt = e.target.token.getWord();
  auto it = namedValues.find(tgt);
  if (it == namedValues.end())
    return logError(("Undeclared variable: " + tgt).c_str());

  // Const check
  if (it->second.isConst)
    return logError(("Cannot reassign const variable: " + tgt).c_str());

  // Borrow-ref: write through the pointer
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

  // String / array: store the whole struct
  if (TypeResolver::isString(targetTy) || TypeResolver::isArray(targetTy)) {
    Value *loaded = builder.CreateLoad(targetTy, val);
    builder.CreateStore(loaded, it->second.allocaInst);
    return it->second.allocaInst;
  }

  // Scalar coercion
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

// ─────────────────────────────────────────────────────────────────────────────
// Array / string indexing
// ─────────────────────────────────────────────────────────────────────────────

/// arr[i]  –  works for 1-D arrays AND strings (returns i8 char).
Value *CodeGenerator::visitArrayIndex(const ArrayIndexExpr &e) {
  const std::string &name = e.array.token.getWord();
  auto it = namedValues.find(name);
  if (it == namedValues.end())
    return logError(("Unknown variable: " + name).c_str());

  Type *ty = it->second.type;

  // ── String: return the character at index ─────────────────────────────────
  if (TypeResolver::isString(ty)) {
    StructType *st = cast<StructType>(ty);
    Value *loaded = builder.CreateLoad(st, it->second.allocaInst, "str.load");
    Value *dataPtr = builder.CreateExtractValue(loaded, {0}, "str.data");
    Value *idx = codegen(*e.index);
    if (!idx)
      return nullptr;
    if (idx->getType()->isIntegerTy(32))
      idx = builder.CreateSExt(idx, Type::getInt64Ty(context));
    Value *charPtr =
        builder.CreateGEP(Type::getInt8Ty(context), dataPtr, idx, "char.ptr");
    return builder.CreateLoad(Type::getInt8Ty(context), charPtr, "char");
  }

  // ── Array struct ─────────────────────────────────────────────────────────
  Value *arrAlloca = it->second.allocaInst;
  if (it->second.isReference)
    arrAlloca = builder.CreateLoad(ty, arrAlloca, name + ".ref");

  auto *arrSt = dyn_cast<StructType>(ty);
  if (!arrSt || !TypeResolver::isArray(arrSt))
    return logError(("Not an array or string: " + name).c_str());

  Value *loaded = builder.CreateLoad(arrSt, arrAlloca, "arr.load");
  Value *dataPtr = builder.CreateExtractValue(loaded, {1}, "arr.data");

  Value *idx = codegen(*e.index);
  if (!idx)
    return nullptr;
  if (idx->getType()->isIntegerTy(32))
    idx = builder.CreateSExt(idx, Type::getInt64Ty(context));

  Type *elem = TypeResolver::elemType(context, arrSt);
  if (!elem)
    return logError("Cannot resolve array element type");

  Value *elemPtr = builder.CreateGEP(elem, dataPtr, idx, "elem.ptr");
  if (TypeResolver::isArray(elem) || TypeResolver::isString(elem))
    return elemPtr;
  return builder.CreateLoad(elem, elemPtr, "elem");
}

/// arr[i][j]  –  2-D array access.
Value *CodeGenerator::visitArray2DIndex(const Array2DIndexExpr &e) {
  const std::string &name = e.array.token.getWord();
  auto it = namedValues.find(name);
  if (it == namedValues.end())
    return logError(("Unknown variable: " + name).c_str());

  auto *outerSt = dyn_cast<StructType>(it->second.type);
  if (!outerSt || !TypeResolver::isArray(outerSt))
    return logError(("Not a 2-D array: " + name).c_str());

  Type *innerElem = TypeResolver::elemType(context, outerSt);
  auto *innerSt = dyn_cast<StructType>(innerElem);
  if (!innerSt)
    return logError("Inner element type is not a struct");

  Type *elemType = TypeResolver::elemType(context, innerSt);
  if (!elemType)
    return logError("Cannot resolve leaf element type");

  Value *outerLoaded =
      builder.CreateLoad(outerSt, it->second.allocaInst, "outer.load");
  Value *outerData = builder.CreateExtractValue(outerLoaded, {1}, "outer.data");

  Value *row = codegen(*e.rowIndex);
  if (!row)
    return nullptr;
  if (row->getType()->isIntegerTy(32))
    row = builder.CreateSExt(row, Type::getInt64Ty(context));

  Value *rowPtr = builder.CreateGEP(innerSt, outerData, row, "row.ptr");
  Value *innerLoaded = builder.CreateLoad(innerSt, rowPtr, "inner.load");
  Value *innerData = builder.CreateExtractValue(innerLoaded, {1}, "inner.data");

  Value *col = codegen(*e.colIndex);
  if (!col)
    return nullptr;
  if (col->getType()->isIntegerTy(32))
    col = builder.CreateSExt(col, Type::getInt64Ty(context));

  Value *elemPtr = builder.CreateGEP(elemType, innerData, col, "elem.ptr");
  if (TypeResolver::isArray(elemType) || TypeResolver::isString(elemType))
    return elemPtr;
  return builder.CreateLoad(elemType, elemPtr, "elem");
}

Value *CodeGenerator::visitArrayIndexAssign(const ArrayIndexAssignExpr &e) {
  const std::string &name = e.array.token.getWord();
  auto it = namedValues.find(name);
  if (it == namedValues.end())
    return logError(("Unknown variable: " + name).c_str());
  if (it->second.isConst)
    return logError(
        ("Cannot assign to element of const array: " + name).c_str());

  Value *arrAlloca = it->second.allocaInst;
  Type *arrTy = it->second.type;
  if (it->second.isReference) {
    arrAlloca = builder.CreateLoad(PointerType::get(context, 0), arrAlloca);
    if (it->second.pointeeType)
      arrTy = it->second.pointeeType;
  }

  auto *arrSt = dyn_cast<StructType>(arrTy);
  if (!arrSt || !TypeResolver::isArray(arrSt))
    return logError(("Not an array: " + name).c_str());

  Value *loaded = builder.CreateLoad(arrSt, arrAlloca, "arr.load");
  Value *dataPtr = builder.CreateExtractValue(loaded, {1}, "arr.data");

  Type *elem = TypeResolver::elemType(context, arrSt);
  if (!elem)
    return logError("Cannot resolve array element type");

  Value *idx = codegen(*e.index);
  if (!idx)
    return nullptr;
  if (idx->getType()->isIntegerTy(32))
    idx = builder.CreateSExt(idx, Type::getInt64Ty(context));

  Value *val = codegen(*e.value);
  if (!val)
    return nullptr;

  if (TypeResolver::isArray(elem) || TypeResolver::isString(elem))
    val = builder.CreateLoad(elem, val);
  else
    val = TypeResolver::coerce(builder, val, elem);

  Value *elemPtr = builder.CreateGEP(elem, dataPtr, idx, "elem.ptr");
  builder.CreateStore(val, elemPtr);
  return val;
}

Value *CodeGenerator::visitArray2DIndexAssign(const Array2DIndexAssignExpr &e) {
  const std::string &name = e.array.token.getWord();
  auto it = namedValues.find(name);
  if (it == namedValues.end())
    return logError(("Unknown variable: " + name).c_str());
  if (it->second.isConst)
    return logError(
        ("Cannot assign to element of const 2-D array: " + name).c_str());

  auto *outerSt = dyn_cast<StructType>(it->second.type);
  if (!outerSt || !TypeResolver::isArray(outerSt))
    return logError(("Not a 2-D array: " + name).c_str());

  Type *innerElem = TypeResolver::elemType(context, outerSt);
  auto *innerSt = dyn_cast<StructType>(innerElem);
  if (!innerSt)
    return logError("Inner element type is not a struct");
  Type *elemType = TypeResolver::elemType(context, innerSt);
  if (!elemType)
    return logError("Cannot resolve leaf element type");

  Value *outerLoaded = builder.CreateLoad(outerSt, it->second.allocaInst);
  Value *outerData = builder.CreateExtractValue(outerLoaded, {1});

  Value *row = codegen(*e.rowIndex);
  if (row->getType()->isIntegerTy(32))
    row = builder.CreateSExt(row, Type::getInt64Ty(context));

  Value *rowPtr = builder.CreateGEP(innerSt, outerData, row);
  Value *innerLoaded = builder.CreateLoad(innerSt, rowPtr);
  Value *innerData = builder.CreateExtractValue(innerLoaded, {1});

  Value *col = codegen(*e.colIndex);
  if (col->getType()->isIntegerTy(32))
    col = builder.CreateSExt(col, Type::getInt64Ty(context));

  Value *val = codegen(*e.value);
  if (!val)
    return nullptr;
  val = TypeResolver::coerce(builder, val, elemType);

  Value *elemPtr = builder.CreateGEP(elemType, innerData, col, "elem.ptr");
  builder.CreateStore(val, elemPtr);
  return val;
}

// ─────────────────────────────────────────────────────────────────────────────
// Property access
// ─────────────────────────────────────────────────────────────────────────────

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
  return logError((".length not applicable to this type"));
}

Value *CodeGenerator::visitStringText(const StringTextExpr &e) {
  const std::string &name = e.name.token.getWord();
  auto it = namedValues.find(name);
  if (it == namedValues.end())
    return logError(("Unknown variable: " + name).c_str());
  if (!TypeResolver::isString(it->second.type))
    return logError(".text used on non-string variable");

  StructType *st = cast<StructType>(it->second.type);
  Value *loaded = builder.CreateLoad(st, it->second.allocaInst);
  return builder.CreateExtractValue(loaded, {0}, "text");
}

// ─────────────────────────────────────────────────────────────────────────────
// Array allocation
// ─────────────────────────────────────────────────────────────────────────────

Value *CodeGenerator::visitNewArray(const NewArrayExpr &e) {
  const std::string &typeName = e.arrayType.base.token.getWord();
  int dims = e.arrayType.dimensions;

  if (dims == 2 && e.sizes.size() >= 2) {
    Type *elem = TypeResolver::fromName(context, typeName);
    if (!elem)
      return logError("Unknown 2-D array element type");
    Value *rows = codegen(*e.sizes[0]);
    Value *cols = codegen(*e.sizes[1]);
    if (!rows || !cols)
      return nullptr;
    return ArrayAlloc::make2D(builder, context, module.get(), elem, rows, cols);
  }

  // 1-D
  Type *elem = TypeResolver::fromName(context, typeName);
  if (!elem)
    return logError("Unknown array element type");
  Value *sz = codegen(*e.sizes[0]);
  if (!sz)
    return nullptr;
  return ArrayAlloc::make1D(builder, context, module.get(), elem, sz);
}

// ─────────────────────────────────────────────────────────────────────────────
// Function calls
// ─────────────────────────────────────────────────────────────────────────────

Value *CodeGenerator::visitCall(const CallExpr &e) {
  const std::string rawName = e.callee.token.getWord();
  const std::string calleeName = normalizeFunctionName(rawName);

  if ((calleeName == "printf" || rawName == "Printf") &&
      e.arguments.size() == 1)
    return handlePrintf(e);
  if (rawName == "Print" && e.arguments.size() == 1)
    return handlePrint(e);

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
      args.push_back(v);
    }
  }

  bool isVoid = callee->getReturnType()->isVoidTy();
  return builder.CreateCall(callee, args, isVoid ? "" : "call");
}

Value *CodeGenerator::handlePrintf(const CallExpr &e) {
  auto *strArg = dynamic_cast<const StrLitExpr *>(e.arguments[0].get());
  if (!strArg)
    return logError("Printf requires a string literal argument");

  std::string raw = unescapeString(strArg->lit.getWord());
  raw = replaceHexColors(raw) + "\033[0m";

  std::vector<FmtArg> fmtArgs;
  std::string fmt = buildPrintfFmt(raw, context, builder, namedValues, fmtArgs);

  llvm::Function *printfF = module->getFunction("printf");
  if (!printfF)
    return logError("printf not declared");

  Value *fmtPtr = builder.CreateGlobalString(fmt, ".fmt");
  std::vector<Value *> args = {fmtPtr};

  for (auto &fa : fmtArgs) {
    if (fa.spec == "%s") {
      if (auto *ai = dyn_cast<AllocaInst>(fa.value)) {
        Type *at = ai->getAllocatedType();
        if (TypeResolver::isString(at)) {
          Value *loaded = builder.CreateLoad(cast<StructType>(at), ai);
          args.push_back(builder.CreateExtractValue(loaded, {0}));
          continue;
        }
      }
    }
    args.push_back(fa.value);
  }
  return builder.CreateCall(printfF, args, "printf.ret");
}

Value *CodeGenerator::handlePrint(const CallExpr &e) {
  Value *val = codegen(*e.arguments[0]);
  if (!val)
    return logError("Failed to evaluate Print argument");

  llvm::Function *printfF = module->getFunction("printf");
  if (!printfF)
    return logError("printf not declared");

  Value *fmt = builder.CreateGlobalString("%s\033[0m\n", ".pfmt");
  Value *charPtr = nullptr;

  if (auto *ai = dyn_cast<AllocaInst>(val)) {
    Type *at = ai->getAllocatedType();
    if (TypeResolver::isString(at)) {
      Value *loaded = builder.CreateLoad(cast<StructType>(at), ai);
      charPtr = builder.CreateExtractValue(loaded, {0});
    }
  }
  if (!charPtr) {
    Value *sv = StringOps::fromValue(builder, context, module.get(), val);
    if (auto *ai = dyn_cast<AllocaInst>(sv)) {
      Value *loaded =
          builder.CreateLoad(cast<StructType>(ai->getAllocatedType()), ai);
      charPtr = builder.CreateExtractValue(loaded, {0});
    } else {
      charPtr = sv;
    }
  }
  return builder.CreateCall(printfF, {fmt, charPtr}, "print.ret");
}

// ─────────────────────────────────────────────────────────────────────────────
// Expression dispatch
// ─────────────────────────────────────────────────────────────────────────────
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
  if (auto *p = dynamic_cast<const Array2DIndexExpr *>(&expr))
    return visitArray2DIndex(*p);
  if (auto *p = dynamic_cast<const ArrayIndexAssignExpr *>(&expr))
    return visitArrayIndexAssign(*p);
  if (auto *p = dynamic_cast<const Array2DIndexAssignExpr *>(&expr))
    return visitArray2DIndexAssign(*p);
  if (auto *p = dynamic_cast<const LengthPropertyExpr *>(&expr))
    return visitLengthProperty(*p);
  if (auto *p = dynamic_cast<const StringTextExpr *>(&expr))
    return visitStringText(*p);
  return logError("Unknown expression node");
}

// ─────────────────────────────────────────────────────────────────────────────
// VarDecl initialisation
// ─────────────────────────────────────────────────────────────────────────────

Value *CodeGenerator::visitVarDecl(const VarDecl &d) {
  const std::string &name = d.name.token.getWord();
  Type *ty = TypeResolver::fromTypeDesc(context, d.type);
  if (!ty)
    return logError(("Unknown type for variable: " + name).c_str());

  AllocaInst *alloca = builder.CreateAlloca(ty, nullptr, name);

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

  // Deep-copy string
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

    builder.CreateStore(newMem, builder.CreateStructGEP(st, alloca, 0));
    builder.CreateStore(srcLen, builder.CreateStructGEP(st, alloca, 1));
    builder.CreateStore(srcCap, builder.CreateStructGEP(st, alloca, 2));
    namedValues[name] = VarInfo(alloca, ty, false, false, false, d.isConst);
    return alloca;
  }

  // Shallow copy for arrays (copy struct, data pointer is shared)
  if (TypeResolver::isArray(ty)) {
    Value *loaded = builder.CreateLoad(ty, init);
    builder.CreateStore(loaded, alloca);
    namedValues[name] = VarInfo(alloca, ty, false, false, false, d.isConst);
    return alloca;
  }

  // Scalar with coercion
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

  namedValues[name] = VarInfo(alloca, ty, false, false, false, d.isConst);
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

  // Store the address of the source in the borrow alloca
  builder.CreateStore(sit->second.allocaInst, alloca);
  sit->second.isBorrowed = true;

  Type *refTy = PointerType::get(context, 0);
  VarInfo vi(alloca, refTy, false, false, /*isRef=*/true, d.isConst, src);
  vi.pointeeType = sit->second.type;
  namedValues[name] = vi;
  return alloca;
}

// ─────────────────────────────────────────────────────────────────────────────
// Statement visitors
// ─────────────────────────────────────────────────────────────────────────────

Value *CodeGenerator::codegen(const Statement &stmt) {
  if (auto *p = dynamic_cast<const VarDecl *>(&stmt))
    return visitVarDecl(*p);
  if (auto *p = dynamic_cast<const ExprStmt *>(&stmt))
    return codegen(*p->expr);
  if (auto *p = dynamic_cast<const IfStmt *>(&stmt))
    return visitIfStmt(*p);
  if (auto *p = dynamic_cast<const WhileStmt *>(&stmt))
    return visitWhileStmt(*p);
  if (auto *p = dynamic_cast<const Return *>(&stmt))
    return visitReturn(*p);
  return logError("Unknown statement node");
}

void CodeGenerator::codegen(const Block &block) {
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
  if (!builder.GetInsertBlock()->getTerminator())
    builder.CreateBr(mergeBB);

  fn->insert(fn->end(), elseBB);
  builder.SetInsertPoint(elseBB);
  if (s.elseBranch)
    codegen(*s.elseBranch);
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
  if (!builder.GetInsertBlock()->getTerminator())
    builder.CreateBr(condBB);

  fn->insert(fn->end(), exitBB);
  builder.SetInsertPoint(exitBB);
  return nullptr;
}

Value *CodeGenerator::visitReturn(const Return &s) {
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

// ─────────────────────────────────────────────────────────────────────────────
// Function compilation
// ─────────────────────────────────────────────────────────────────────────────

llvm::Function *CodeGenerator::codegen(const AST_H::Function &func) {
  const std::string fname = normalizeFunctionName(func.name.token.getWord());
  Type *retTy = TypeResolver::fromTypeDesc(context, func.returnType);
  if (!retTy)
    return nullptr;

  // Build parameter types
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

  // Get-or-create function
  llvm::Function *f = module->getFunction(fname);
  if (!f) {
    auto *ft = FunctionType::get(retTy, paramTypes, false);
    f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, fname,
                               *module);
  }
  f->addFnAttr("stackrealignment");

  BasicBlock *entry = BasicBlock::Create(context, "entry", f);
  builder.SetInsertPoint(entry);

  namedValues.clear();
  size_t idx = 0;
  for (auto &arg : f->args()) {
    const auto &param = func.params[idx++];
    const std::string pname = param.name.token.getWord();

    if (param.isBorrowRef) {
      // Store the incoming pointer in an alloca
      AllocaInst *ptrAlloca = builder.CreateAlloca(PointerType::get(context, 0),
                                                   nullptr, pname + ".refptr");
      builder.CreateStore(&arg, ptrAlloca);

      Type *pointee = TypeResolver::fromTypeDesc(context, param.type);
      VarInfo vi(ptrAlloca, PointerType::get(context, 0), false, false,
                 /*ref=*/true, param.isConst);
      vi.pointeeType = pointee;
      namedValues[pname] = vi;
    } else {
      AllocaInst *a = builder.CreateAlloca(arg.getType(), nullptr, pname);
      builder.CreateStore(&arg, a);
      namedValues[pname] =
          VarInfo(a, arg.getType(), false, false, false, param.isConst);
    }
  }

  codegen(*func.body);

  // Ensure a terminator exists
  if (!builder.GetInsertBlock()->getTerminator()) {
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

// ─────────────────────────────────────────────────────────────────────────────
// Program entry point
// ─────────────────────────────────────────────────────────────────────────────

bool CodeGenerator::generate(const Program &program,
                             const std::string &outputFilename) {
  namedValues.clear();

  // Pre-declare runtime functions
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

  // Compile all functions
  for (const auto &fn : program.functions) {
    if (!codegen(*fn))
      return false;
  }

  // Write LLVM IR
  std::error_code ec;
  raw_fd_ostream out(outputFilename + ".ll", ec, sys::fs::OF_None);
  if (ec) {
    errs() << "Cannot open output file: " << ec.message() << "\n";
    return false;
  }
  module->print(out, nullptr);
  return true;
}
