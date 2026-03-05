#include "CodeGen.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include <iostream>
#include <memory>
#include <regex>
#include <string>
#include <unordered_map>

using namespace llvm;

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

class TypeResolver {
public:
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

    if (t.size() > 6 && t.substr(0, 6) == "array.") {
      std::string innerName = t.substr(6);

      Type *innerTy = fromName(ctx, innerName);
      if (!innerTy)
        return nullptr;

      return getOrCreateArrayStruct(ctx, innerTy);
    }

    return nullptr;

    return nullptr;
  }

  static Type *fromTypeDesc(LLVMContext &ctx, const TypeDesc &td) {
    return fromName(ctx, td.fullName());
  }

  static StructType *getStringType(LLVMContext &ctx) {
    StructType *st = StructType::getTypeByName(ctx, "string");
    if (!st) {
      st = StructType::create(ctx, "string");
      st->setBody({PointerType::get(Type::getInt8Ty(ctx)->getContext(), 0),
                   Type::getInt64Ty(ctx), Type::getInt64Ty(ctx)});
    }
    return st;
  }

  static StructType *getOrCreateArrayStruct(LLVMContext &ctx,
                                            Type *elementType) {
    if (!elementType)
      return nullptr;

    std::string name = "array." + typeName(elementType);

    if (StructType *existing = StructType::getTypeByName(ctx, name))
      return existing;

    auto *dataPtrTy = PointerType::getUnqual(elementType->getContext());

    StructType *st =
        StructType::create(ctx, {Type::getInt64Ty(ctx), dataPtrTy}, name);

    return st;
  }
  static bool isString(Type *ty) {
    return ty && ty->isStructTy() &&
           cast<StructType>(ty)->getName() == "string";
  }

  static bool isArray(Type *ty) {
    return ty && ty->isStructTy() &&
           cast<StructType>(ty)->getName().starts_with("array.");
  }

  static bool isNumeric(Type *ty) {
    return ty && (ty->isIntegerTy() || ty->isFloatingPointTy());
  }

  static Type *elemType(LLVMContext &ctx, StructType *arrTy) {
    if (!arrTy || !isArray(arrTy))
      return nullptr;

    StringRef name = arrTy->getName();
    std::string innerName = name.substr(6).str();
    return fromName(ctx, innerName);
  }

  static Type *largerType(Type *a, Type *b) {
    if (!a || !b)
      return a ? a : b;
    if (a == b)
      return a;

    if (isArray(a) || isArray(b) || isString(a) || isString(b))
      return a ? a : b;

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
    if (!val || !target || val->getType() == target)
      return val;

    Type *src = val->getType();

    if (target->isPointerTy() && src->isPointerTy())
      return val;

    if (target->isFloatingPointTy()) {
      if (src->isFloatingPointTy()) {
        if (target->isDoubleTy() && src->isFloatTy())
          return B.CreateFPExt(val, target, "fpext");
        if (target->isFloatTy() && src->isDoubleTy())
          return B.CreateFPTrunc(val, target, "fptrunc");
      }
      if (src->isIntegerTy())
        return B.CreateSIToFP(val, target,
                              target->isDoubleTy() ? "sitofp.d" : "sitofp.f");
    }

    if (target->isIntegerTy()) {
      if (src->isIntegerTy()) {
        unsigned tb = target->getIntegerBitWidth();
        unsigned sb = src->getIntegerBitWidth();
        if (tb > sb)
          return B.CreateSExt(val, target, "sext");
        if (tb < sb)
          return B.CreateTrunc(val, target, "trunc");
      }
      if (src->isFloatingPointTy())
        return B.CreateFPToSI(val, target, "fptosi");
    }

    return val;
  }

private:
  static std::string typeName(Type *ty) {
    if (!ty)
      return "unknown";

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

class StringOps {
public:
  static Value *fromLiteral(IRBuilder<> &B, LLVMContext &ctx, Module *M,
                            const std::string &literal) {
    Value *ptr = B.CreateGlobalString(literal, "strl");
    Value *len = ConstantInt::get(Type::getInt64Ty(ctx), literal.size());
    return fromParts(B, ctx, M, ptr, len);
  }

  static Value *fromParts(IRBuilder<> &B, LLVMContext &ctx, Module *M,
                          Value *dataPtr, Value *length) {
    Type *i64 = Type::getInt64Ty(ctx);
    Value *cap = B.CreateAdd(length, ConstantInt::get(i64, 1), "cap");
    Value *mem = B.CreateCall(RTDecl::malloc_(M, ctx), {cap}, "str.alloc");

    B.CreateCall(RTDecl::memcpy_(M, ctx),
                 {mem, dataPtr, length, ConstantInt::getFalse(ctx)});
    Value *nullPos = B.CreateGEP(Type::getInt8Ty(ctx), mem, length);
    B.CreateStore(ConstantInt::get(Type::getInt8Ty(ctx), 0), nullPos);

    StructType *st = TypeResolver::getStringType(ctx);
    AllocaInst *s = B.CreateAlloca(st, nullptr, "string");
    B.CreateStore(mem, B.CreateStructGEP(st, s, 0));
    B.CreateStore(length, B.CreateStructGEP(st, s, 1));
    B.CreateStore(cap, B.CreateStructGEP(st, s, 2));
    return s;
  }

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

struct FmtArg {
  std::string spec;
  Value *value;
};

static Value *evalInterp(const std::string &inner, LLVMContext &ctx,
                         IRBuilder<> &B,
                         const std::map<std::string, VarInfo> &vars) {
  // Simple identifier
  bool isIdent = !inner.empty();
  for (char c : inner) {
    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
      isIdent = false;
      break;
    }
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
  try {
    size_t pos;
    double v = std::stod(inner, &pos);
    if (pos == inner.size())
      return ConstantFP::get(Type::getDoubleTy(ctx), v);
  } catch (...) {
  }
  return nullptr;
}
void CodeGenerator::scheduleStringFree(Value *strAlloca) {
  // strAlloca is an AllocaInst* holding a %string struct
  StructType *st = TypeResolver::getStringType(context);
  Value *loaded = builder.CreateLoad(st, strAlloca);
  Value *dataPtr = builder.CreateExtractValue(loaded, {0}, "tmp.data");
  pendingFrees.push_back(dataPtr);
}
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

namespace ArrayAlloc {

using namespace llvm;

static Value *emitMalloc(IRBuilder<> &B, LLVMContext &C, Module &M,
                         Type *allocTy, Value *count) {
  const DataLayout &DL = M.getDataLayout();
  uint64_t size = DL.getTypeAllocSize(allocTy);

  Type *i64Ty = Type::getInt64Ty(C);

  Value *elemSize = ConstantInt::get(i64Ty, size);
  Value *count64 = B.CreateZExt(count, i64Ty);
  Value *total = B.CreateMul(elemSize, count64);

  // malloc must return ptr
  FunctionCallee mallocFn = M.getOrInsertFunction(
      "malloc", FunctionType::get(PointerType::get(C, 0), {i64Ty}, false));

  return B.CreateCall(mallocFn, {total}); // already ptr
}

static StructType *getArrayStructTy(LLVMContext &C) {
  // { i64, ptr }
  return StructType::get(Type::getInt64Ty(C), PointerType::get(C, 0));
}

static Value *buildLevel(IRBuilder<> &B, LLVMContext &C, Module &M,
                         Type *elementType, ArrayRef<Value *> dims,
                         unsigned depth) {
  Type *i64Ty = Type::getInt64Ty(C);
  StructType *arrTy = getArrayStructTy(C);

  Value *len = dims[depth];

  // allocate descriptor on stack
  Value *descriptor = B.CreateAlloca(arrTy);

  // store length
  Value *lenPtr = B.CreateStructGEP(arrTy, descriptor, 0);
  B.CreateStore(B.CreateZExt(len, i64Ty), lenPtr);

  Value *dataPtr = B.CreateStructGEP(arrTy, descriptor, 1);

  bool isLeaf = (depth == dims.size() - 1);

  if (isLeaf) {
    // allocate element buffer
    Value *buffer = emitMalloc(B, C, M, elementType, len);
    B.CreateStore(buffer, dataPtr); // store ptr directly
  } else {
    StructType *childTy = getArrayStructTy(C);

    // allocate array of child descriptors
    Value *childArray = emitMalloc(B, C, M, childTy, len);

    // store pointer to children
    B.CreateStore(childArray, dataPtr);

    llvm::Function *fn = B.GetInsertBlock()->getParent();

    BasicBlock *loopBB = BasicBlock::Create(C, "nd.loop", fn);
    BasicBlock *bodyBB = BasicBlock::Create(C, "nd.body", fn);
    BasicBlock *afterBB = BasicBlock::Create(C, "nd.after", fn);

    Value *index = B.CreateAlloca(i64Ty);
    B.CreateStore(ConstantInt::get(i64Ty, 0), index);

    B.CreateBr(loopBB);
    B.SetInsertPoint(loopBB);

    Value *iVal = B.CreateLoad(i64Ty, index);
    Value *cond = B.CreateICmpULT(iVal, B.CreateZExt(len, i64Ty));

    B.CreateCondBr(cond, bodyBB, afterBB);

    // ---- BODY ----
    B.SetInsertPoint(bodyBB);

    Value *slot = B.CreateGEP(childTy, childArray, iVal);

    Value *childDesc = buildLevel(B, C, M, elementType, dims, depth + 1);

    Value *childVal = B.CreateLoad(childTy, childDesc);
    B.CreateStore(childVal, slot);

    Value *next = B.CreateAdd(iVal, ConstantInt::get(i64Ty, 1));

    B.CreateStore(next, index);
    B.CreateBr(loopBB);

    // ---- AFTER ----
    B.SetInsertPoint(afterBB);
  }

  return descriptor;
}

static Value *makeND(IRBuilder<> &B, LLVMContext &C, Module &M,
                     Type *elementType, ArrayRef<Value *> dims) {
  return buildLevel(B, C, M, elementType, dims, 0);
}

} // namespace ArrayAlloc
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

CodeGenerator::CodeGenerator()
    : module(std::make_unique<Module>("nexus", context)), builder(context) {
  module->setTargetTriple(Triple(LLVM_HOST_TRIPLE));
}

bool CodeGenerator::isCStringPointer(Type *ty) { return ty->isPointerTy(); }

Value *CodeGenerator::logError(const char *msg) {
  errs() << "\033[31mCodeGen error: " << msg << "\033[0m\n";
  return nullptr;
}

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
  TypeResolver::getStringType(l->getContext());
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
static llvm::Function *free_(Module *M, LLVMContext &ctx) {
  llvm::Function *f = M->getFunction("free");
  if (!f) {
    auto *ft = FunctionType::get(Type::getVoidTy(ctx),
                                 {PointerType::get(ctx, 0)}, false);
    f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "free", M);
  }
  return f;
}
Value *CodeGenerator::visitBinary(const BinaryExpr &e) {
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

    Value *concatRes =
        StringOps::concat(builder, context, module.get(), lstr, rstr);

    // Free RHS temporary if it wasn't a named variable
    bool rhsIsNamed = dynamic_cast<const IdentExpr *>(e.right.get()) != nullptr;
    if (!rhsIsNamed) {
      StructType *st = TypeResolver::getStringType(context);
      Value *loaded = builder.CreateLoad(st, rstr);
      Value *dataPtr = builder.CreateExtractValue(loaded, {0}, "rhs.tmp.data");
      builder.CreateCall(free_(module.get(), context), {dataPtr});
    }

    // Free LHS temporary if it wasn't a named variable either
    bool lhsIsNamed = dynamic_cast<const IdentExpr *>(e.left.get()) != nullptr;
    if (!lhsIsNamed && ls) {
      StructType *st = TypeResolver::getStringType(context);
      Value *loaded = builder.CreateLoad(st, lstr);
      Value *dataPtr = builder.CreateExtractValue(loaded, {0}, "lhs.tmp.data");
      builder.CreateCall(free_(module.get(), context), {dataPtr});
    }

    return concatRes;
  }
  if ((e.op == BinaryOp::Eq || e.op == BinaryOp::Ne) && (ls || rs)) {
    Value *l = codegen(*e.left), *r = codegen(*e.right);
    if (!l || !r)
      return nullptr;
    auto charPtr = [&](Value *v) -> Value * {
      Type *ty = v->getType();

      // Case 1: pointer to a string struct (AllocaInst or any ptr)
      if (ty->isPointerTy()) {
        if (auto *ai = dyn_cast<AllocaInst>(v)) {
          if (TypeResolver::isString(ai->getAllocatedType())) {
            Value *loaded = builder.CreateLoad(
                cast<StructType>(ai->getAllocatedType()), v, "str.load");
            return builder.CreateExtractValue(loaded, {0}, "str.data");
          }
        }
        // Already a char* (e.g. from a prior ExtractValue)
        return v;
      }

      // Case 2: string struct value (e.g. returned from a call or load)
      if (TypeResolver::isString(ty)) {
        return builder.CreateExtractValue(v, {0}, "str.data");
      }

      // Case 3: already a raw pointer (i8* / opaque ptr)
      return v;
    };
    llvm::Function *strcmpF = module->getFunction("strcmp");
    if (!strcmpF)
      return logError("strcmp not declared");
    Value *cmp = builder.CreateCall(strcmpF, {charPtr(l), charPtr(r)}, "scmp");
    Value *eq =
        builder.CreateICmpEQ(cmp, ConstantInt::get(cmp->getType(), 0), "seq");
    bool lIsNamed = dynamic_cast<const IdentExpr *>(e.left.get()) != nullptr;
    bool rIsNamed = dynamic_cast<const IdentExpr *>(e.right.get()) != nullptr;

    auto freeStringStruct = [&](Value *v) {
      // v is a ptr to a %string alloca — load it and free the data ptr
      if (auto *ai = dyn_cast<AllocaInst>(v)) {
        if (TypeResolver::isString(ai->getAllocatedType())) {
          StructType *st = cast<StructType>(ai->getAllocatedType());
          Value *loaded = builder.CreateLoad(st, ai);
          Value *data = builder.CreateExtractValue(loaded, {0}, "cmp.tmp.data");
          builder.CreateCall(free_(module.get(), context), {data});
        }
      }
    };

    if (!lIsNamed && ls)
      freeStringStruct(l);
    if (!rIsNamed && rs)
      freeStringStruct(r);

    return e.op == BinaryOp::Eq ? eq : builder.CreateNot(eq, "sne");
  }

  Value *l = codegen(*e.left), *r = codegen(*e.right);
  if (!l || !r)
    return nullptr;

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
    // Free old buffer before overwriting
    Value *oldVal =
        builder.CreateLoad(targetTy, it->second.allocaInst, tgt + ".old");
    Value *oldData = builder.CreateExtractValue(oldVal, {0}, "old.data");
    builder.CreateCall(free_(module.get(), context), {oldData});

    // Now store the new value
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
      if (TypeResolver::isString(ty)) {
        return builder.CreateLoad(Type::getInt8Ty(context), elemPtr, "char");
      }
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
      if (TypeResolver::isArray(elemTy) || TypeResolver::isString(elemTy)) {
        return logError("Cannot assign scalar to array slot");
      }

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

Value *CodeGenerator::visitNewArray(const NewArrayExpr &e) {
  std::string typeName = e.arrayType.base.token.getWord();
  Type *elemType = TypeResolver::fromName(context, typeName);
  if (!elemType) {
    return nullptr;
  }

  std::vector<Value *> dimValues;
  for (auto &sizeExpr : e.sizes) {
    Value *v = codegen(*sizeExpr);
    if (!v)
      return nullptr;
    dimValues.push_back(v);
  }
  return ArrayAlloc::makeND(builder, context, *module, elemType, dimValues);
}

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
  auto *strArg = dynamic_cast<const StrLitExpr *>(e.arguments[0].get());
  if (!strArg)
    return logError("Printf requires a string literal argument");

  llvm::Function *printfF = module->getFunction("printf");
  if (!printfF)
    return logError("printf not declared");

  std::string fmt = strArg->lit.getWord() + "\n";

  Value *fmtPtr = builder.CreateGlobalString(fmt, ".fmt");
  std::vector<Value *> args = {fmtPtr};
  return builder.CreateCall(printfF, args, "printf.ret");
}

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
  if (auto *p = dynamic_cast<const ArrayIndexAssignExpr *>(&expr))
    return visitArrayIndexAssign(*p);
  if (auto *p = dynamic_cast<const LengthPropertyExpr *>(&expr))
    return visitLengthProperty(*p);
  return logError("Unknown expression node");
}

void CodeGenerator::emitScopeDestructors() {
  for (auto it = scopeStack.back().rbegin(); it != scopeStack.back().rend();
       ++it) {

    auto nv = namedValues.find(*it);
    if (nv == namedValues.end())
      continue;

    VarInfo &vi = nv->second;

    if (!TypeResolver::isArray(vi.type) && !TypeResolver::isString(vi.type) &&
        !vi.ownsHeap)
      continue;
    if (vi.isMoved || vi.isBorrowed || vi.isReference)
      continue;

    emitDestructor(vi);
  }
}

void CodeGenerator::emitArrayFree(Value *arrPtr, StructType *arrSt, int depth) {
  Type *i64 = Type::getInt64Ty(context);
  llvm::Function *freeF = free_(module.get(), context);
  llvm::Function *fn = builder.GetInsertBlock()->getParent();

  Value *loaded = builder.CreateLoad(arrSt, arrPtr);
  Value *dataPtr = builder.CreateExtractValue(loaded, {1}, "data");
  Value *len = builder.CreateExtractValue(loaded, {0}, "len");

  Type *elemTy = TypeResolver::elemType(context, arrSt);

  // If elements are sub-arrays, recurse into each slot first.
  if (elemTy && TypeResolver::isArray(elemTy)) {
    StructType *elemSt = cast<StructType>(elemTy);
    std::string sfx = std::to_string(depth);

    BasicBlock *loopBB = BasicBlock::Create(context, "free.loop" + sfx, fn);
    BasicBlock *bodyBB = BasicBlock::Create(context, "free.body" + sfx, fn);
    BasicBlock *afterBB = BasicBlock::Create(context, "free.after" + sfx, fn);

    AllocaInst *idx = builder.CreateAlloca(i64, nullptr, "fi" + sfx);
    builder.CreateStore(ConstantInt::get(i64, 0), idx);
    builder.CreateBr(loopBB);

    builder.SetInsertPoint(loopBB);
    Value *cur = builder.CreateLoad(i64, idx);
    Value *cond = builder.CreateICmpULT(cur, len);
    builder.CreateCondBr(cond, bodyBB, afterBB);

    builder.SetInsertPoint(bodyBB);
    Value *slotPtr = builder.CreateGEP(elemSt, dataPtr, cur, "slot" + sfx);
    emitArrayFree(slotPtr, elemSt, depth + 1); // recurse
    Value *next = builder.CreateAdd(cur, ConstantInt::get(i64, 1));
    builder.CreateStore(next, idx);
    builder.CreateBr(loopBB);

    builder.SetInsertPoint(afterBB);
  }

  builder.CreateCall(freeF, {dataPtr});
}

void CodeGenerator::emitDestructor(VarInfo &vi) {
  Type *ty = vi.type;

  if (TypeResolver::isString(ty)) {
    StructType *st = cast<StructType>(ty);
    Value *loaded = builder.CreateLoad(st, vi.allocaInst);
    Value *data = builder.CreateExtractValue(loaded, {0});
    builder.CreateCall(free_(module.get(), context), {data});
    return;
  }

  if (TypeResolver::isArray(ty)) {
    StructType *st = cast<StructType>(ty);
    emitArrayFree(vi.allocaInst, st, 0);
    return;
  }
}

Value *CodeGenerator::visitVarDecl(const VarDecl &d) {
  const std::string &name = d.name.token.getWord();
  Type *ty = TypeResolver::fromTypeDesc(context, d.type);
  if (!ty)
    return logError(("Unknown type for variable: " + name).c_str());

  AllocaInst *alloca = builder.CreateAlloca(ty, nullptr, name);

  scopeStack.back().push_back(name);
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

    // Copy FIRST...
    builder.CreateCall(
        memcpyF, {newMem, srcData, srcLen, ConstantInt::getFalse(context)});

    Value *nullPos =
        builder.CreateGEP(Type::getInt8Ty(context), newMem, srcLen);
    builder.CreateStore(ConstantInt::get(Type::getInt8Ty(context), 0), nullPos);

    builder.CreateStore(newMem, builder.CreateStructGEP(st, alloca, 0));
    builder.CreateStore(srcLen, builder.CreateStructGEP(st, alloca, 1));
    builder.CreateStore(srcCap, builder.CreateStructGEP(st, alloca, 2));

    // ...THEN free the temporary source buffer
    bool isTempSource =
        namedValues.find(init->getName().str()) == namedValues.end();
    if (isTempSource)
      builder.CreateCall(free_(module.get(), context), {srcData});

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

  // Store the address of the source in the borrow alloca
  builder.CreateStore(sit->second.allocaInst, alloca);
  sit->second.isBorrowed = true;

  Type *refTy = PointerType::get(context, 0);
  VarInfo vi(alloca, refTy, false, false, /*isRef=*/true, d.isConst, src);
  vi.pointeeType = sit->second.type;
  namedValues[name] = vi;
  return alloca;
}
void CodeGenerator::flushPendingFrees() {
  llvm::Function *freeF = free_(module.get(), context);
  for (Value *ptr : pendingFrees)
    builder.CreateCall(freeF, {ptr});
  pendingFrees.clear();
}
Value *CodeGenerator::codegen(const Statement &stmt) {
  if (auto *p = dynamic_cast<const VarDecl *>(&stmt))
    return visitVarDecl(*p);
  if (auto *p = dynamic_cast<const ExprStmt *>(&stmt)) {
    Value *v = codegen(*p->expr);
    flushPendingFrees(); // emit free() for all temporaries
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

void CodeGenerator::codegen(const Block &block) {
  scopeStack.emplace_back(); // push scope

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
  scopeStack.emplace_back();
  codegen(*s.doBranch);

  emitScopeDestructors();
  scopeStack.pop_back();
  if (!builder.GetInsertBlock()->getTerminator())
    builder.CreateBr(condBB);

  fn->insert(fn->end(), exitBB);
  builder.SetInsertPoint(exitBB);
  return nullptr;
}

Value *CodeGenerator::visitReturn(const Return &s) {
  while (!scopeStack.empty()) {
    emitScopeDestructors();
    scopeStack.pop_back();
  }
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

  if (!builder.GetInsertBlock()->getTerminator()) {
    emitScopeDestructors(); // ← ADD THIS
    scopeStack.pop_back();
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

  // Compile all functions
  for (const auto &fn : program.functions) {
    if (!codegen(*fn))
      return false;
  }

  // module->print(llvm::outs(), nullptr);

  std::error_code ec;
  raw_fd_ostream out(outputFilename + ".ll", ec, sys::fs::OF_None);
  if (ec) {
    errs() << "Cannot open output file: " << ec.message() << "\n";
    return false;
  }
  module->print(out, nullptr);
  return true;
}
