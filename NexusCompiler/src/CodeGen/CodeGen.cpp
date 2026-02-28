#include "CodeGen.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include <memory>
#include <regex>
#include <unordered_map>

using namespace llvm;

//------------------------------------------------------------------------------
// Helper Functions
//------------------------------------------------------------------------------

static std::string normalizeFunctionName(const std::string &name) {
  static const std::unordered_map<std::string, std::string> mappings = {
      {"Main", "main"},
      {"Printf", "printf"},
      {"Print", "print_literal"},
      {"printf", "printf"}};

  auto it = mappings.find(name);
  return (it != mappings.end()) ? it->second : name;
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
      case '{':
        result += '{';
        ++i;
        break; // \{ → literal {
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

//------------------------------------------------------------------------------
// Type System
//------------------------------------------------------------------------------

class TypeResolver {
public:
  static Type *getLLVMType(LLVMContext &context, const Identifier &typeId) {
    const std::string &t = typeId.token.getWord();
    if (t == "i32" || t == "int" || t == "integer")
      return Type::getInt32Ty(context);
    if (t == "i64" || t == "long")
      return Type::getInt64Ty(context);
    if (t == "i16" || t == "short")
      return Type::getInt16Ty(context);
    if (t == "i8" || t == "char")
      return Type::getInt8Ty(context);
    if (t == "f32" || t == "float")
      return Type::getFloatTy(context);
    if (t == "f64" || t == "double")
      return Type::getDoubleTy(context);
    if (t == "bool")
      return Type::getInt1Ty(context);
    if (t == "void")
      return Type::getVoidTy(context);
    if (t == "str" || t == "string") {
      StructType *st = StructType::getTypeByName(context, "string");
      if (!st) {
        st = StructType::create(context, "string");
        st->setBody({PointerType::get(context, 0), Type::getInt64Ty(context),
                     Type::getInt64Ty(context)});
      }
      return st;
    }
    // Nested array element type: "array.i32", "array.array.i32", etc.
    if (t.size() > 6 && t.substr(0, 6) == "array.") {
      StructType *existing = StructType::getTypeByName(context, t);
      if (existing)
        return existing;
      std::string elemTypeName = t.substr(6);
      Identifier elemId{Token{TokenKind::TOK_IDENTIFIER, elemTypeName, 0, 0}};
      getLLVMType(context, elemId); // ensure element type registered first
      StructType *st = StructType::create(context, t);
      st->setBody({Type::getInt64Ty(context), PointerType::get(context, 0)});
      return st;
    }
    return nullptr;
  }

  static Type *getLLVMType(LLVMContext &context, const ::ArrayType &arrType) {
    Type *elemType = getLLVMType(context, arrType.elementType);
    if (!elemType)
      return nullptr;
    std::string structName = "array." + arrType.elementType.token.getWord();
    StructType *arrayStruct = StructType::getTypeByName(context, structName);
    if (!arrayStruct) {
      arrayStruct = StructType::create(context, structName);
      arrayStruct->setBody(
          {Type::getInt64Ty(context), PointerType::get(context, 0)});
    }
    return arrayStruct;
  }

  static Type *getLLVMType(LLVMContext &context,
                           const VarDecl::TypeVariant &type) {
    if (std::holds_alternative<Identifier>(type))
      return getLLVMType(context, std::get<Identifier>(type));
    else
      return getLLVMType(context, std::get<::ArrayType>(type));
  }

  static bool isString(Type *ty) {
    if (!ty || !ty->isStructTy())
      return false;
    return cast<StructType>(ty)->getName() == "string";
  }

  static bool isArrayType(Type *ty) {
    if (!ty || !ty->isStructTy())
      return false;
    return cast<StructType>(ty)->getName().starts_with("array.");
  }

  static bool isNumeric(Type *ty) {
    return ty->isIntegerTy() || ty->isFloatingPointTy();
  }
  static bool isFloat(Type *ty) { return ty->isFloatingPointTy(); }
  static bool isBool(Type *ty) { return ty->isIntegerTy(1); }

  static Type *getLargerType(Type *a, Type *b) {
    if (!a || !b)
      return a ? a : b;
    if (a == b)
      return a;
    if (isArrayType(a) || isArrayType(b) || isString(a) || isString(b))
      return a;
    if (a->isPointerTy() || b->isPointerTy())
      return a->isPointerTy() ? a : b;
    if (a->isFloatingPointTy() || b->isFloatingPointTy()) {
      if (a->isDoubleTy() || b->isDoubleTy())
        return Type::getDoubleTy(a->getContext());
      return Type::getFloatTy(a->getContext());
    }
    if (a->isIntegerTy() && b->isIntegerTy()) {
      unsigned maxBits = std::max(
          std::max(a->getIntegerBitWidth(), b->getIntegerBitWidth()), 32u);
      return Type::getIntNTy(a->getContext(), maxBits);
    }
    return a;
  }

  static Value *convertToType(IRBuilder<> &builder, Value *val,
                              Type *targetTy) {
    if (!val || !targetTy)
      return val;
    Type *srcTy = val->getType();
    if (srcTy == targetTy)
      return val;
    if (targetTy->isPointerTy() && srcTy->isPointerTy())
      return val;
    if (targetTy->isFloatingPointTy()) {
      if (srcTy->isFloatingPointTy()) {
        if (targetTy->isDoubleTy() && srcTy->isFloatTy())
          return builder.CreateFPExt(val, targetTy, "f2d");
        if (targetTy->isFloatTy() && srcTy->isDoubleTy())
          return builder.CreateFPTrunc(val, targetTy, "d2f");
      } else if (srcTy->isIntegerTy()) {
        return builder.CreateSIToFP(val, targetTy,
                                    targetTy->isDoubleTy() ? "i2d" : "i2f");
      }
    }
    if (targetTy->isIntegerTy()) {
      if (srcTy->isIntegerTy()) {
        unsigned tb = targetTy->getIntegerBitWidth();
        unsigned sb = srcTy->getIntegerBitWidth();
        if (tb > sb)
          return builder.CreateSExt(val, targetTy, "ext");
        if (tb < sb)
          return builder.CreateTrunc(val, targetTy, "trunc");
      } else if (srcTy->isFloatingPointTy()) {
        return builder.CreateFPToSI(val, targetTy, "f2i");
      }
    }
    return val;
  }

  static AllocaInst *createArrayAlloca(IRBuilder<> &builder, Type *elemType,
                                       Value *size,
                                       const std::string &name = "array") {
    LLVMContext &ctx = builder.getContext();
    llvm::Function *mallocF = getMallocFunction(builder, ctx);
    if (!mallocF)
      return nullptr;

    Type *i64Ty = Type::getInt64Ty(ctx);
    uint64_t elemSize = builder.GetInsertBlock()
                            ->getParent()
                            ->getParent()
                            ->getDataLayout()
                            .getTypeAllocSize(elemType);
    Value *elemSizeVal = ConstantInt::get(i64Ty, elemSize);

    if (size->getType()->isIntegerTy(32))
      size = builder.CreateSExt(size, i64Ty, "size_ext");

    Value *totalSize = builder.CreateMul(size, elemSizeVal, "array_bytes");
    Value *rawMem = builder.CreateCall(mallocF, {totalSize}, "array_mem");

    std::string structName = "array." + getTypeName(elemType);
    StructType *arrayStructTy = StructType::getTypeByName(ctx, structName);
    if (!arrayStructTy) {
      arrayStructTy = StructType::create(ctx, structName);
      arrayStructTy->setBody({i64Ty, PointerType::get(ctx, 0)});
    }

    AllocaInst *arrayStruct =
        builder.CreateAlloca(arrayStructTy, nullptr, name);

    Value *lengthField = builder.CreateStructGEP(arrayStructTy, arrayStruct, 0);
    builder.CreateStore(size, lengthField);

    Value *dataField = builder.CreateStructGEP(arrayStructTy, arrayStruct, 1);
    builder.CreateStore(rawMem, dataField);

    return arrayStruct;
  }

  static Value *getArrayLength(IRBuilder<> &builder, Value *arrayStruct,
                               StructType *arrayTy) {
    if (!arrayStruct || !arrayTy || !isArrayType(arrayTy))
      return nullptr;
    Value *loadedArray = builder.CreateLoad(arrayTy, arrayStruct, "array_load");
    return builder.CreateExtractValue(loadedArray, {0}, "length");
  }

  // Extract the raw char* data pointer from a string struct alloca
  static Value *getStringDataPtr(IRBuilder<> &builder, LLVMContext &ctx,
                                 Value *strAlloca) {
    StructType *stringTy = StructType::getTypeByName(ctx, "string");
    if (!stringTy)
      return strAlloca;
    Value *dataFieldPtr =
        builder.CreateStructGEP(stringTy, strAlloca, 0, "str_data_gep");
    return builder.CreateLoad(PointerType::get(ctx, 0), dataFieldPtr,
                              "str_data_ptr");
  }

private:
  static std::string getTypeName(Type *ty) {
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
    return "unknown";
  }

  static llvm::Function *getMallocFunction(IRBuilder<> &builder,
                                           LLVMContext &ctx) {
    Module *module = builder.GetInsertBlock()->getParent()->getParent();
    llvm::Function *mallocF = module->getFunction("malloc");
    if (!mallocF) {
      FunctionType *mallocTy = FunctionType::get(
          PointerType::get(ctx, 0), {Type::getInt64Ty(ctx)}, false);
      mallocF = llvm::Function::Create(
          mallocTy, llvm::Function::ExternalLinkage, "malloc", module);
    }
    return mallocF;
  }
};

//------------------------------------------------------------------------------
// String Operations
//------------------------------------------------------------------------------

class StringOperations {
public:
  static Value *createStringFromLiteral(IRBuilder<> &builder, LLVMContext &ctx,
                                        const std::string &literal) {
    Value *literalPtr = builder.CreateGlobalString(literal, "str_lit");
    Value *literalLen =
        ConstantInt::get(Type::getInt64Ty(ctx), literal.length());
    return createStringFromParts(builder, ctx, literalPtr, literalLen);
  }

  static Value *createStringFromParts(IRBuilder<> &builder, LLVMContext &ctx,
                                      Value *dataPtr, Value *length) {
    llvm::Function *mallocF = getMallocFunction(builder, ctx);
    if (!mallocF)
      return nullptr;

    Type *i64Ty = Type::getInt64Ty(ctx);
    Value *size =
        builder.CreateAdd(length, ConstantInt::get(i64Ty, 1), "size_with_null");
    Value *newMem = builder.CreateCall(mallocF, {size}, "string_alloc");

    llvm::Function *memcpyF = getMemcpyFunction(builder, ctx);
    if (!memcpyF)
      return nullptr;

    builder.CreateCall(memcpyF,
                       {newMem, dataPtr, length, ConstantInt::getFalse(ctx)});

    // Null terminator
    Value *nullPtr = builder.CreateGEP(Type::getInt8Ty(ctx), newMem, length);
    builder.CreateStore(ConstantInt::get(Type::getInt8Ty(ctx), 0), nullPtr);

    StructType *stringTy = getStringType(ctx);
    AllocaInst *stringStruct =
        builder.CreateAlloca(stringTy, nullptr, "string");

    builder.CreateStore(newMem,
                        builder.CreateStructGEP(stringTy, stringStruct, 0));
    builder.CreateStore(length,
                        builder.CreateStructGEP(stringTy, stringStruct, 1));
    builder.CreateStore(size,
                        builder.CreateStructGEP(stringTy, stringStruct, 2));

    return stringStruct;
  }

  // Concatenate two string struct allocas → new string struct alloca
  static Value *concatStrings(IRBuilder<> &builder, LLVMContext &ctx,
                              Value *leftStruct, Value *rightStruct) {
    StructType *stringTy = getStringType(ctx);
    if (!stringTy)
      return nullptr;

    Value *leftLoaded = builder.CreateLoad(stringTy, leftStruct, "left_str");
    Value *leftData = builder.CreateExtractValue(leftLoaded, {0}, "left_data");
    Value *leftLen = builder.CreateExtractValue(leftLoaded, {1}, "left_len");

    Value *rightLoaded = builder.CreateLoad(stringTy, rightStruct, "right_str");
    Value *rightData =
        builder.CreateExtractValue(rightLoaded, {0}, "right_data");
    Value *rightLen = builder.CreateExtractValue(rightLoaded, {1}, "right_len");

    Type *i64Ty = Type::getInt64Ty(ctx);
    Value *totalLen = builder.CreateAdd(leftLen, rightLen, "total_len");
    Value *totalSize =
        builder.CreateAdd(totalLen, ConstantInt::get(i64Ty, 1), "total_size");

    llvm::Function *mallocF = getMallocFunction(builder, ctx);
    llvm::Function *memcpyF = getMemcpyFunction(builder, ctx);
    if (!mallocF || !memcpyF)
      return nullptr;

    Value *newMem = builder.CreateCall(mallocF, {totalSize}, "concat_alloc");

    builder.CreateCall(memcpyF,
                       {newMem, leftData, leftLen, ConstantInt::getFalse(ctx)});
    Value *rightStart =
        builder.CreateGEP(Type::getInt8Ty(ctx), newMem, leftLen);
    builder.CreateCall(
        memcpyF, {rightStart, rightData, rightLen, ConstantInt::getFalse(ctx)});

    Value *nullPtr = builder.CreateGEP(Type::getInt8Ty(ctx), newMem, totalLen);
    builder.CreateStore(ConstantInt::get(Type::getInt8Ty(ctx), 0), nullPtr);

    AllocaInst *resultStruct =
        builder.CreateAlloca(stringTy, nullptr, "concat_result");
    builder.CreateStore(newMem,
                        builder.CreateStructGEP(stringTy, resultStruct, 0));
    builder.CreateStore(totalLen,
                        builder.CreateStructGEP(stringTy, resultStruct, 1));
    builder.CreateStore(totalSize,
                        builder.CreateStructGEP(stringTy, resultStruct, 2));

    return resultStruct;
  }

  // Convert any value to a string struct alloca
  static Value *valueToString(IRBuilder<> &builder, LLVMContext &ctx,
                              Value *val) {
    Type *ty = val->getType();

    if (TypeResolver::isString(ty))
      return val; // already a string struct alloca

    if (ty->isIntegerTy())
      return intToString(builder, ctx, val);

    if (ty->isFloatingPointTy())
      return floatToString(builder, ctx, val);

    // Opaque pointer — assume null-terminated char*
    if (ty->isPointerTy()) {
      Value *len = getCStringLength(builder, ctx, val);
      return createStringFromParts(builder, ctx, val, len);
    }

    if (TypeResolver::isArrayType(ty))
      return createStringFromLiteral(builder, ctx, "[array]");

    return createStringFromLiteral(builder, ctx, "[unprintable]");
  }

  static StructType *getStringType(LLVMContext &ctx) {
    StructType *st = StructType::getTypeByName(ctx, "string");
    if (!st) {
      st = StructType::create(ctx, "string");
      st->setBody({PointerType::get(ctx, 0), Type::getInt64Ty(ctx),
                   Type::getInt64Ty(ctx)});
    }
    return st;
  }

private:
  static llvm::Function *getMallocFunction(IRBuilder<> &builder,
                                           LLVMContext &ctx) {
    Module *module = builder.GetInsertBlock()->getParent()->getParent();
    llvm::Function *mallocF = module->getFunction("malloc");
    if (!mallocF) {
      FunctionType *mallocTy = FunctionType::get(
          PointerType::get(ctx, 0), {Type::getInt64Ty(ctx)}, false);
      mallocF = llvm::Function::Create(
          mallocTy, llvm::Function::ExternalLinkage, "malloc", module);
    }
    return mallocF;
  }

  static llvm::Function *getMemcpyFunction(IRBuilder<> &builder,
                                           LLVMContext &ctx) {
    Module *module = builder.GetInsertBlock()->getParent()->getParent();
    const std::string name = "llvm.memcpy.p0.p0.i64";
    llvm::Function *memcpyF = module->getFunction(name);
    if (!memcpyF) {
      Type *ptrTy = PointerType::get(ctx, 0);
      FunctionType *ft = FunctionType::get(
          Type::getVoidTy(ctx),
          {ptrTy, ptrTy, Type::getInt64Ty(ctx), Type::getInt1Ty(ctx)}, false);
      memcpyF = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                                       name, module);
      memcpyF->addFnAttr(Attribute::NoUnwind);
    }
    return memcpyF;
  }

  static llvm::Function *getSprintfFunction(IRBuilder<> &builder,
                                            LLVMContext &ctx) {
    Module *module = builder.GetInsertBlock()->getParent()->getParent();
    llvm::Function *f = module->getFunction("sprintf");
    if (!f) {
      Type *ptrTy = PointerType::get(ctx, 0);
      FunctionType *ft =
          FunctionType::get(Type::getInt32Ty(ctx), {ptrTy, ptrTy}, true);
      f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "sprintf",
                                 module);
    }
    return f;
  }

  static Value *getCStringLength(IRBuilder<> &builder, LLVMContext &ctx,
                                 Value *strPtr) {
    Module *module = builder.GetInsertBlock()->getParent()->getParent();
    llvm::Function *strlenF = module->getFunction("strlen");
    if (!strlenF) {
      FunctionType *ft = FunctionType::get(Type::getInt64Ty(ctx),
                                           {PointerType::get(ctx, 0)}, false);
      strlenF = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                                       "strlen", module);
    }
    return builder.CreateCall(strlenF, {strPtr}, "strlen");
  }

  static Value *intToString(IRBuilder<> &builder, LLVMContext &ctx,
                            Value *val) {
    Type *i64Ty = Type::getInt64Ty(ctx);
    if (val->getType()->getIntegerBitWidth() < 64)
      val = builder.CreateSExt(val, i64Ty, "int_ext");

    Value *fmtPtr = builder.CreateGlobalString("%lld", "fmt");
    llvm::ArrayType *bufferTy = llvm::ArrayType::get(Type::getInt8Ty(ctx), 32);
    AllocaInst *buffer = builder.CreateAlloca(bufferTy, nullptr, "int_buf");

    llvm::Function *sprintfF = getSprintfFunction(builder, ctx);
    builder.CreateCall(sprintfF, {buffer, fmtPtr, val});

    Value *len = getCStringLength(builder, ctx, buffer);
    return createStringFromParts(builder, ctx, buffer, len);
  }

  static Value *floatToString(IRBuilder<> &builder, LLVMContext &ctx,
                              Value *val) {
    if (val->getType()->isFloatTy())
      val = builder.CreateFPExt(val, Type::getDoubleTy(ctx), "f2d");

    Value *fmtPtr = builder.CreateGlobalString("%g", "fmt");
    llvm::ArrayType *bufferTy = llvm::ArrayType::get(Type::getInt8Ty(ctx), 64);
    AllocaInst *buffer = builder.CreateAlloca(bufferTy, nullptr, "float_buf");

    llvm::Function *sprintfF = getSprintfFunction(builder, ctx);
    builder.CreateCall(sprintfF, {buffer, fmtPtr, val});

    Value *len = getCStringLength(builder, ctx, buffer);
    return createStringFromParts(builder, ctx, buffer, len);
  }
};

struct FmtArg {
  std::string spec; // e.g. "%s", "%lld", "%g"
  Value *value;     // the actual LLVM value to pass to printf
};

static Value *evalInterpolation(const std::string &inner, LLVMContext &ctx,
                                IRBuilder<> &builder,
                                const std::map<std::string, VarInfo> &vars) {
  bool isSimpleIdent = !inner.empty();
  for (char c : inner)
    if (!std::isalnum((unsigned char)c) && c != '_') {
      isSimpleIdent = false;
      break;
    }

  if (isSimpleIdent) {
    auto it = vars.find(inner);
    if (it != vars.end() && !it->second.isMoved) {
      if (TypeResolver::isString(it->second.type) ||
          TypeResolver::isArrayType(it->second.type))
        return it->second.alloca;
      return builder.CreateLoad(it->second.type, it->second.alloca,
                                inner + "_load");
    }
    return nullptr;
  }

  try {
    size_t pos = 0;
    long long v = std::stoll(inner, &pos);
    if (pos == inner.size())
      return ConstantInt::get(Type::getInt32Ty(ctx), v);
  } catch (...) {
  }

  // Try float literal
  try {
    size_t pos = 0;
    double v = std::stod(inner, &pos);
    if (pos == inner.size())
      return ConstantFP::get(Type::getDoubleTy(ctx), v);
  } catch (...) {
  }

  return nullptr;
}

// Process the raw string, returning a printf-style format string and populating
// outArgs with (format-specifier, value) pairs.
static std::string processPrintfString(
    const std::string &raw, LLVMContext &ctx, IRBuilder<> &builder,
    const std::map<std::string, VarInfo> &vars, std::vector<FmtArg> &outArgs) {
  std::string result;
  size_t i = 0;

  while (i < raw.size()) {
    // Escaped {: \{ → literal {
    if (raw[i] == '\\' && i + 1 < raw.size() && raw[i + 1] == '{') {
      result += '{';
      i += 2;
      continue;
    }

    if (raw[i] != '{') {
      // Escape any literal % for printf
      if (raw[i] == '%')
        result += '%';
      result += raw[i++];
      continue;
    }

    if (i + 1 < raw.size() && raw[i + 1] == '{') {
      result += '{';
      i += 2;
      continue;
    }

    // Find matching }
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
      else
        break;
    }
    if (j >= raw.size()) {
      result += raw[i++];
      continue;
    }

    std::string inner = raw.substr(start, j - start);
    Value *val = evalInterpolation(inner, ctx, builder, vars);

    if (val) {
      Type *ty = val->getType();

      bool valIsString = false;
      // if (isSimpleIdent) {
      //  auto vit = vars.find(inner);
      // if (vit != vars.end())
      // valIsString = TypeResolver::isString(vit->second.type);
      //}

      if (valIsString) {
        Value *dataPtr = TypeResolver::getStringDataPtr(builder, ctx, val);
        outArgs.push_back({"%s", dataPtr});
        result += "%s";
      } else if (ty->isIntegerTy(1)) {
        Value *trueStr = builder.CreateGlobalString("true", "bool_t");
        Value *falseStr = builder.CreateGlobalString("false", "bool_f");
        Value *selected =
            builder.CreateSelect(val, trueStr, falseStr, "bool_str");
        outArgs.push_back({"%s", selected});
        result += "%s";
      } else if (ty->isIntegerTy()) {
        if (ty->getIntegerBitWidth() < 64)
          val = builder.CreateSExt(val, Type::getInt64Ty(ctx), "iext");
        outArgs.push_back({"%lld", val});
        result += "%lld";
      } else if (ty->isFloatingPointTy()) {
        if (!ty->isDoubleTy())
          val = builder.CreateFPExt(val, Type::getDoubleTy(ctx), "f2d");
        outArgs.push_back({"%g", val});
        result += "%g";
      } else if (ty->isPointerTy()) {
        outArgs.push_back({"%s", val});
        result += "%s";
      } else {
        Value *strStruct = StringOperations::valueToString(builder, ctx, val);
        Value *dataPtr =
            TypeResolver::getStringDataPtr(builder, ctx, strStruct);
        outArgs.push_back({"%s", dataPtr});
        result += "%s";
      }
    } else {
      result += '{' + inner + '}';
    }

    i = j + 1;
  }

  return result;
}

static Value *
evaluateInterpolatedStringLiteral(const std::string &raw, LLVMContext &ctx,
                                  IRBuilder<> &builder,
                                  const std::map<std::string, VarInfo> &vars) {

  Value *result = StringOperations::createStringFromLiteral(builder, ctx, "");

  size_t i = 0;
  std::string literal_chunk;

  auto flushLiteral = [&]() {
    if (!literal_chunk.empty()) {
      Value *chunk = StringOperations::createStringFromLiteral(builder, ctx,
                                                               literal_chunk);
      result = StringOperations::concatStrings(builder, ctx, result, chunk);
      literal_chunk.clear();
    }
  };

  while (i < raw.size()) {
    // \{ → literal {
    if (raw[i] == '\\' && i + 1 < raw.size() && raw[i + 1] == '{') {
      literal_chunk += '{';
      i += 2;
      continue;
    }

    if (raw[i] != '{') {
      literal_chunk += raw[i++];
      continue;
    }

    // {{ → literal {
    if (i + 1 < raw.size() && raw[i + 1] == '{') {
      literal_chunk += '{';
      i += 2;
      continue;
    }

    // Find }
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
      else
        break;
    }
    if (j >= raw.size()) {
      literal_chunk += raw[i++];
      continue;
    }

    std::string inner = raw.substr(start, j - start);
    Value *val = evalInterpolation(inner, ctx, builder, vars);

    if (val) {
      flushLiteral();
      // Convert val to string struct
      Value *strVal = StringOperations::valueToString(builder, ctx, val);
      result = StringOperations::concatStrings(builder, ctx, result, strVal);
    } else {
      literal_chunk += '{' + inner + '}';
    }

    i = j + 1;
  }

  flushLiteral();
  return result;
}

//------------------------------------------------------------------------------
// CodeGenerator Implementation
//------------------------------------------------------------------------------

void CodeGenerator::declareExternalFunctions() {
  Type *ptrTy = PointerType::get(context, 0);
  Type *i64Ty = Type::getInt64Ty(context);
  Type *i32Ty = Type::getInt32Ty(context);
  Type *i1Ty = Type::getInt1Ty(context);
  Type *voidTy = Type::getVoidTy(context);

  auto declare = [&](const std::string &name, FunctionType *ft,
                     bool noUnwind = false) {
    if (!module->getFunction(name)) {
      auto *f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                                       name, *module);
      if (noUnwind)
        f->addFnAttr(Attribute::NoUnwind);
    }
  };

  declare("sprintf", FunctionType::get(i32Ty, {ptrTy, ptrTy}, true), true);
  declare("printf", FunctionType::get(i32Ty, {ptrTy}, true), true);
  declare("strlen", FunctionType::get(i64Ty, {ptrTy}, false), true);
  declare("strcpy", FunctionType::get(ptrTy, {ptrTy, ptrTy}, false), true);
  declare("strcat", FunctionType::get(ptrTy, {ptrTy, ptrTy}, false), true);
  declare("malloc", FunctionType::get(ptrTy, {i64Ty}, false), true);
  declare("free", FunctionType::get(voidTy, {ptrTy}, false), true);
  declare("llvm.memcpy.p0.p0.i64",
          FunctionType::get(voidTy, {ptrTy, ptrTy, i64Ty, i1Ty}, false), true);
}

CodeGenerator::CodeGenerator()
    : module(std::make_unique<Module>("nexus", context)), builder(context) {
  module->setTargetTriple(Triple(LLVM_HOST_TRIPLE));
}

Value *CodeGenerator::logError(const char *msg) {
  errs() << "\033[31mCode compiling error: " << msg << "\033[0m\n";
  return nullptr;
}

Value *CodeGenerator::visitNewArray(const NewArrayExpr &expr) {
  Type *elemType =
      TypeResolver::getLLVMType(context, expr.arrayType.elementType);
  if (!elemType)
    return logError("Unknown array element type");

  Value *size = codegen(*expr.size);
  if (!size)
    return logError("Failed to codegen array size");

  return TypeResolver::createArrayAlloca(builder, elemType, size, "arr");
}

Value *CodeGenerator::visitArrayIndex(const ArrayIndexExpr &expr) {
  std::string arrName = expr.array.token.getWord();
  auto it = namedValues.find(arrName);
  if (it == namedValues.end())
    return logError(("Unknown array: " + arrName).c_str());

  StructType *arrStructTy = dyn_cast<StructType>(it->second.type);
  if (!arrStructTy || !TypeResolver::isArrayType(arrStructTy))
    return logError(("Not an array: " + arrName).c_str());

  Value *loadedStruct =
      builder.CreateLoad(arrStructTy, it->second.alloca, "arr_load");
  Value *dataPtr = builder.CreateExtractValue(loadedStruct, {1}, "data_ptr");

  Value *idx = codegen(*expr.index);
  if (!idx)
    return logError("Failed to codegen array index");
  if (idx->getType()->isIntegerTy(32))
    idx = builder.CreateSExt(idx, Type::getInt64Ty(context), "idx_ext");

  StringRef structName = arrStructTy->getName();
  std::string elemTypeName = structName.substr(6).str();
  Identifier elemId{Token{TokenKind::TOK_IDENTIFIER, elemTypeName, 0, 0}};
  Type *elemType = TypeResolver::getLLVMType(context, elemId);
  if (!elemType)
    return logError("Cannot resolve array element type for indexing");

  Value *elemPtr = builder.CreateGEP(elemType, dataPtr, idx, "elem_ptr");
  // Composite element types (nested arrays, strings) are reference types
  if (TypeResolver::isArrayType(elemType) || TypeResolver::isString(elemType))
    return elemPtr;
  return builder.CreateLoad(elemType, elemPtr, "elem");
}

Value *CodeGenerator::visitArrayLength(const ArrayLengthExpr &expr) {
  std::string arrName = expr.array.token.getWord();
  auto it = namedValues.find(arrName);
  if (it == namedValues.end())
    return logError(("Unknown array: " + arrName).c_str());

  StructType *arrStructTy = dyn_cast<StructType>(it->second.type);
  if (!arrStructTy || !TypeResolver::isArrayType(arrStructTy))
    return logError(("Not an array: " + arrName).c_str());

  Value *loadedStruct =
      builder.CreateLoad(arrStructTy, it->second.alloca, "arr_load");
  return builder.CreateExtractValue(loadedStruct, {0}, "length");
}

//------------------------------------------------------------------------------
// Expression Code Generation
//------------------------------------------------------------------------------

Value *CodeGenerator::visitIdentifier(const IdentExpr &expr) {
  std::string name = expr.name.token.getWord();
  auto it = namedValues.find(name);
  if (it == namedValues.end())
    return logError(("Unknown variable: " + name).c_str());
  if (it->second.isMoved)
    return logError(("Use of moved value: " + name).c_str());
  if (TypeResolver::isString(it->second.type) ||
      TypeResolver::isArrayType(it->second.type))
    return it->second.alloca;
  return builder.CreateLoad(it->second.type, it->second.alloca, name + "_load");
}

Value *CodeGenerator::visitLiteral(const IntLitExpr &expr) {
  return ConstantInt::get(Type::getInt32Ty(context),
                          std::stoll(expr.lit.token.getWord()));
}

Value *CodeGenerator::visitLiteral(const FloatLitExpr &expr) {
  return ConstantFP::get(Type::getDoubleTy(context),
                         std::stod(expr.lit.token.getWord()));
}

Value *CodeGenerator::visitLiteral(const StrLitExpr &expr) {
  std::string raw = unescapeString(expr.lit.token.getWord());
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
    return evaluateInterpolatedStringLiteral(raw, context, builder,
                                             namedValues);
  }

  std::string processed;
  for (size_t i = 0; i < raw.size(); ++i) {
    if (raw[i] == '{' && i + 1 < raw.size() && raw[i + 1] == '{') {
      processed += '{';
      ++i;
    } else
      processed += raw[i];
  }
  return StringOperations::createStringFromLiteral(builder, context, processed);
}

Value *CodeGenerator::visitLiteral(const BoolLitExpr &expr) {
  return ConstantInt::get(Type::getInt1Ty(context),
                          expr.lit.token.getWord() == "true" ? 1 : 0);
}

Value *CodeGenerator::promoteToDouble(Value *val) {
  if (val->getType()->isDoubleTy())
    return val;
  if (val->getType()->isFloatTy())
    return builder.CreateFPExt(val, Type::getDoubleTy(context), "f2d");
  return builder.CreateSIToFP(val, Type::getDoubleTy(context), "i2d");
}

Value *CodeGenerator::promoteToInt(Value *val) {
  if (val->getType()->isIntegerTy())
    return val;
  return builder.CreateFPToSI(val, Type::getInt32Ty(context), "f2i");
}

Value *CodeGenerator::visitBinary(const BinaryExpr &expr) {
  Value *left = codegen(*expr.left);
  Value *right = codegen(*expr.right);
  if (!left || !right)
    return nullptr;

  if (expr.op == BinaryOp::Add) {
    std::function<bool(const Expression &)> isStrExpr =
        [&](const Expression &e) -> bool {
      if (dynamic_cast<const StrLitExpr *>(&e))
        return true;
      if (auto *id = dynamic_cast<const IdentExpr *>(&e)) {
        auto it = namedValues.find(id->name.token.getWord());
        if (it != namedValues.end())
          return TypeResolver::isString(it->second.type);
      }
      if (auto *bin = dynamic_cast<const BinaryExpr *>(&e)) {
        if (bin->op == BinaryOp::Add)
          return isStrExpr(*bin->left) || isStrExpr(*bin->right);
      }
      return false;
    };

    bool leftIsStr = isStrExpr(*expr.left);
    bool rightIsStr = isStrExpr(*expr.right);

    if (leftIsStr || rightIsStr) {
      Value *leftStr =
          leftIsStr ? left
                    : StringOperations::valueToString(builder, context, left);
      Value *rightStr =
          rightIsStr ? right
                     : StringOperations::valueToString(builder, context, right);
      return StringOperations::concatStrings(builder, context, leftStr,
                                             rightStr);
    }
  }

  if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy()) {
    unsigned lb = left->getType()->getIntegerBitWidth();
    unsigned rb = right->getType()->getIntegerBitWidth();
    if (lb < rb)
      left = builder.CreateSExt(left, right->getType(), "lext");
    else if (rb < lb)
      right = builder.CreateSExt(right, left->getType(), "rext");
  }

  bool leftFloat = left->getType()->isFloatingPointTy();
  bool rightFloat = right->getType()->isFloatingPointTy();

  if (leftFloat && right->getType()->isIntegerTy())
    right = builder.CreateSIToFP(right, Type::getDoubleTy(context), "i2d");
  else if (rightFloat && left->getType()->isIntegerTy())
    left = builder.CreateSIToFP(left, Type::getDoubleTy(context), "i2d");

  switch (expr.op) {
  case BinaryOp::Lt:
  case BinaryOp::Le:
  case BinaryOp::Gt:
  case BinaryOp::Ge:
  case BinaryOp::Eq:
  case BinaryOp::Ne:
    return generateComparison(expr.op, left, right, leftFloat || rightFloat);
  default:
    break;
  }

  switch (expr.op) {
  case BinaryOp::Add:
    if (leftFloat || rightFloat) {
      left = promoteToDouble(left);
      right = promoteToDouble(right);
      return builder.CreateFAdd(left, right, "fadd");
    }
    return builder.CreateAdd(left, right, "add");

  case BinaryOp::Sub:
    if (leftFloat || rightFloat) {
      left = promoteToDouble(left);
      right = promoteToDouble(right);
      return builder.CreateFSub(left, right, "fsub");
    }
    return builder.CreateSub(left, right, "sub");

  case BinaryOp::Mul:
    if (leftFloat || rightFloat) {
      left = promoteToDouble(left);
      right = promoteToDouble(right);
      return builder.CreateFMul(left, right, "fmul");
    }
    return builder.CreateMul(left, right, "mul");

  case BinaryOp::Div:
    left = promoteToDouble(left);
    right = promoteToDouble(right);
    return builder.CreateFDiv(left, right, "fdiv");

  case BinaryOp::DivFloor:
    left = promoteToInt(left);
    right = promoteToInt(right);
    return builder.CreateSDiv(left, right, "divfloor");

  case BinaryOp::Mod:
    left = promoteToInt(left);
    right = promoteToInt(right);
    return builder.CreateSRem(left, right, "mod");

  default:
    return logError("Unknown binary operator");
  }
}

Value *CodeGenerator::generateComparison(BinaryOp op, Value *left, Value *right,
                                         bool isFloat) {
  if (isFloat) {
    left = promoteToDouble(left);
    right = promoteToDouble(right);
    switch (op) {
    case BinaryOp::Lt:
      return builder.CreateFCmpOLT(left, right, "flt");
    case BinaryOp::Le:
      return builder.CreateFCmpOLE(left, right, "fle");
    case BinaryOp::Gt:
      return builder.CreateFCmpOGT(left, right, "fgt");
    case BinaryOp::Ge:
      return builder.CreateFCmpOGE(left, right, "fge");
    case BinaryOp::Eq:
      return builder.CreateFCmpOEQ(left, right, "feq");
    case BinaryOp::Ne:
      return builder.CreateFCmpONE(left, right, "fne");
    default:
      return nullptr;
    }
  } else {
    switch (op) {
    case BinaryOp::Lt:
      return builder.CreateICmpSLT(left, right, "ilt");
    case BinaryOp::Le:
      return builder.CreateICmpSLE(left, right, "ile");
    case BinaryOp::Gt:
      return builder.CreateICmpSGT(left, right, "igt");
    case BinaryOp::Ge:
      return builder.CreateICmpSGE(left, right, "ige");
    case BinaryOp::Eq:
      return builder.CreateICmpEQ(left, right, "ieq");
    case BinaryOp::Ne:
      return builder.CreateICmpNE(left, right, "ine");
    default:
      return nullptr;
    }
  }
}

Value *CodeGenerator::visitArrayIndexAssign(const ArrayIndexAssignExpr &expr) {
  std::string arrName = expr.array.token.getWord();
  auto it = namedValues.find(arrName);
  if (it == namedValues.end())
    return logError(("Unknown array: " + arrName).c_str());

  StructType *arrStructTy = dyn_cast<StructType>(it->second.type);
  if (!arrStructTy || !TypeResolver::isArrayType(arrStructTy))
    return logError(("Not an array: " + arrName).c_str());

  Value *loadedStruct =
      builder.CreateLoad(arrStructTy, it->second.alloca, "arr_load");
  Value *dataPtr = builder.CreateExtractValue(loadedStruct, {1}, "data_ptr");

  StringRef structName = arrStructTy->getName();
  std::string elemTypeName = structName.substr(6).str();
  Identifier elemId{Token{TokenKind::TOK_IDENTIFIER, elemTypeName, 0, 0}};
  Type *elemType = TypeResolver::getLLVMType(context, elemId);
  if (!elemType)
    return logError("Cannot resolve array element type for index assignment");

  Value *idx = codegen(*expr.index);
  if (!idx)
    return logError("Failed to codegen index");
  if (idx->getType()->isIntegerTy(32))
    idx = builder.CreateSExt(idx, Type::getInt64Ty(context), "idx_ext");

  Value *val = codegen(*expr.value);
  if (!val)
    return logError("Failed to codegen assigned value");

  // Composite element types (arrays/strings) come as alloca ptrs; load struct
  // value
  if (TypeResolver::isArrayType(elemType) || TypeResolver::isString(elemType))
    val = builder.CreateLoad(elemType, val, "elem_val");
  else
    val = TypeResolver::convertToType(builder, val, elemType);

  Value *elemPtr = builder.CreateGEP(elemType, dataPtr, idx, "elem_ptr");
  builder.CreateStore(val, elemPtr);
  return val;
}

Value *CodeGenerator::visitUnary(const UnaryExpr &expr) {
  Value *operand = codegen(*expr.operand);
  if (!operand)
    return nullptr;
  switch (expr.op) {
  case UnaryOp::Negate:
    return operand->getType()->isFloatingPointTy()
               ? builder.CreateFNeg(operand, "fneg")
               : builder.CreateNeg(operand, "neg");
  }
  return logError("Unknown unary operator");
}

Value *CodeGenerator::visitAssignment(const AssignExpr &expr) {
  std::string targetName = expr.target.token.getWord();
  Value *val = codegen(*expr.value);
  if (!val)
    return nullptr;

  auto it = namedValues.find(targetName);
  if (it == namedValues.end())
    return logError(("Undeclared variable: " + targetName).c_str());
  if (it->second.isBorrowed)
    return logError(("Cannot modify borrowed variable: " + targetName).c_str());

  Type *targetType = it->second.type;

  // String/array: val is a ptr to struct; load the struct and store into target
  // alloca
  if (TypeResolver::isString(targetType) ||
      TypeResolver::isArrayType(targetType)) {
    Value *loaded = builder.CreateLoad(targetType, val, "ref_load");
    builder.CreateStore(loaded, it->second.alloca);
    return it->second.alloca;
  }

  // Auto-widen/convert scalars to target type
  if (targetType->isIntegerTy() && val->getType()->isIntegerTy()) {
    unsigned tb = targetType->getIntegerBitWidth();
    unsigned sb = val->getType()->getIntegerBitWidth();
    if (tb > sb)
      val = builder.CreateSExt(val, targetType, "iext");
    else if (tb < sb)
      val = builder.CreateTrunc(val, targetType, "itrunc");
  } else if (targetType->isFloatingPointTy() && val->getType()->isIntegerTy()) {
    val = builder.CreateSIToFP(val, targetType, "i2f");
  } else if (targetType->isDoubleTy() && val->getType()->isFloatTy()) {
    val = builder.CreateFPExt(val, targetType, "f2d");
  } else if (targetType->isFloatTy() && val->getType()->isDoubleTy()) {
    val = builder.CreateFPTrunc(val, targetType, "d2f");
  }

  switch (expr.kind) {
  case AssignKind::Copy:
    builder.CreateStore(val, it->second.alloca);
    break;

  case AssignKind::Move: {
    auto *sourceIdent = dynamic_cast<const IdentExpr *>(expr.value.get());
    if (!sourceIdent)
      return logError("Move requires variable on right-hand side");
    std::string sourceName = sourceIdent->name.token.getWord();
    auto srcIt = namedValues.find(sourceName);
    if (srcIt == namedValues.end())
      return logError(("Unknown variable: " + sourceName).c_str());
    if (srcIt->second.isMoved)
      return logError(("Already moved: " + sourceName).c_str());
    if (srcIt->second.isBorrowed)
      return logError(("Cannot move borrowed value: " + sourceName).c_str());
    builder.CreateStore(val, it->second.alloca);
    srcIt->second.isMoved = true;
    break;
  }

  case AssignKind::Borrow: {
    auto *sourceIdent = dynamic_cast<const IdentExpr *>(expr.value.get());
    if (!sourceIdent)
      return logError("Borrow requires variable on right-hand side");
    std::string sourceName = sourceIdent->name.token.getWord();
    auto srcIt = namedValues.find(sourceName);
    if (srcIt == namedValues.end())
      return logError(("Unknown variable: " + sourceName).c_str());
    if (srcIt->second.isMoved)
      return logError(("Cannot borrow moved value: " + sourceName).c_str());
    namedValues[targetName] = {srcIt->second.alloca, srcIt->second.type, true,
                               false};
    break;
  }
  }

  return val;
}

Value *CodeGenerator::visitIncrement(const Increment &expr) {
  return generateIncrementDecrement(expr.target.token.getWord(), true);
}

Value *CodeGenerator::visitDecrement(const Decrement &expr) {
  return generateIncrementDecrement(expr.target.token.getWord(), false);
}

Value *CodeGenerator::generateIncrementDecrement(const std::string &varName,
                                                 bool isInc) {
  auto it = namedValues.find(varName);
  if (it == namedValues.end())
    return logError(("Unknown variable: " + varName).c_str());
  if (it->second.isBorrowed || it->second.isMoved)
    return logError(("Cannot modify " + varName).c_str());

  Type *ty = it->second.type;
  Value *cur = builder.CreateLoad(ty, it->second.alloca, "load_" + varName);
  Value *result;
  if (ty->isFloatingPointTy()) {
    Value *one = ConstantFP::get(ty, 1.0);
    result = isInc ? builder.CreateFAdd(cur, one, "finc")
                   : builder.CreateFSub(cur, one, "fdec");
  } else {
    Value *one = ConstantInt::get(ty, 1);
    result = isInc ? builder.CreateAdd(cur, one, "inc")
                   : builder.CreateSub(cur, one, "dec");
  }
  builder.CreateStore(result, it->second.alloca);
  return result;
}

Value *CodeGenerator::visitCall(const CallExpr &expr) {
  std::string rawName = expr.callee.token.getWord();
  std::string calleeName = normalizeFunctionName(rawName);

  if ((calleeName == "printf" || rawName == "Printf") &&
      expr.arguments.size() == 1)
    return handlePrintf(expr);

  if (rawName == "Print" && expr.arguments.size() == 1)
    return handlePrint(expr);

  llvm::Function *callee = module->getFunction(calleeName);
  if (!callee)
    return logError(("Unknown function: " + calleeName).c_str());
  if (!callee->isVarArg() && callee->arg_size() != expr.arguments.size())
    return logError("Incorrect number of arguments");

  std::vector<Value *> args;
  for (auto &arg : expr.arguments) {
    Value *v = codegen(*arg);
    if (!v)
      return nullptr;
    args.push_back(v);
  }
  return builder.CreateCall(callee, args, "calltmp");
}

Value *CodeGenerator::handlePrintf(const CallExpr &expr) {
  auto *strArg = dynamic_cast<const StrLitExpr *>(expr.arguments[0].get());
  if (!strArg)
    return logError("Printf requires string literal");

  std::string raw = unescapeString(strArg->lit.token.getWord());
  raw = replaceHexColors(raw) + "\033[0m";

  std::vector<FmtArg> fmtArgs;
  std::string fmt =
      processPrintfString(raw, context, builder, namedValues, fmtArgs);

  Value *fmtPtr = builder.CreateGlobalString(fmt, ".fmt");
  llvm::Function *printfF = module->getFunction("printf");
  if (!printfF)
    return logError("printf not declared");

  std::vector<Value *> args = {fmtPtr};
  for (auto &fa : fmtArgs)
    args.push_back(fa.value);

  return builder.CreateCall(printfF, args, "printf_ret");
}

Value *CodeGenerator::handlePrint(const CallExpr &expr) {
  auto *strArg = dynamic_cast<const StrLitExpr *>(expr.arguments[0].get());
  if (!strArg)
    return logError("Print requires string literal");

  // Print also supports interpolation; append newline
  std::string raw = unescapeString(strArg->lit.token.getWord());
  raw = replaceHexColors(raw) + "\033[0m\n";

  std::vector<FmtArg> fmtArgs;
  std::string fmt =
      processPrintfString(raw, context, builder, namedValues, fmtArgs);

  Value *fmtPtr = builder.CreateGlobalString(fmt, ".pfmt");
  llvm::Function *printfF = module->getFunction("printf");
  if (!printfF)
    return logError("printf not declared");

  std::vector<Value *> args = {fmtPtr};
  for (auto &fa : fmtArgs)
    args.push_back(fa.value);

  return builder.CreateCall(printfF, args, "print_ret");
}

Value *CodeGenerator::codegen(const Expression &expr) {
  if (auto *ie = dynamic_cast<const IdentExpr *>(&expr))
    return visitIdentifier(*ie);
  if (auto *ile = dynamic_cast<const IntLitExpr *>(&expr))
    return visitLiteral(*ile);
  if (auto *fle = dynamic_cast<const FloatLitExpr *>(&expr))
    return visitLiteral(*fle);
  if (auto *sle = dynamic_cast<const StrLitExpr *>(&expr))
    return visitLiteral(*sle);
  if (auto *ble = dynamic_cast<const BoolLitExpr *>(&expr))
    return visitLiteral(*ble);
  if (auto *bin = dynamic_cast<const BinaryExpr *>(&expr))
    return visitBinary(*bin);
  if (auto *un = dynamic_cast<const UnaryExpr *>(&expr))
    return visitUnary(*un);
  if (auto *ae = dynamic_cast<const AssignExpr *>(&expr))
    return visitAssignment(*ae);
  if (auto *inc = dynamic_cast<const Increment *>(&expr))
    return visitIncrement(*inc);
  if (auto *dec = dynamic_cast<const Decrement *>(&expr))
    return visitDecrement(*dec);
  if (auto *ce = dynamic_cast<const CallExpr *>(&expr))
    return visitCall(*ce);
  if (auto *na = dynamic_cast<const NewArrayExpr *>(&expr))
    return visitNewArray(*na);
  if (auto *ai = dynamic_cast<const ArrayIndexExpr *>(&expr))
    return visitArrayIndex(*ai);
  if (auto *al = dynamic_cast<const ArrayLengthExpr *>(&expr))
    return visitArrayLength(*al);
  if (auto *aia = dynamic_cast<const ArrayIndexAssignExpr *>(&expr))
    return visitArrayIndexAssign(*aia);
  return logError("Unknown expression type");
}

//------------------------------------------------------------------------------
// Statement Code Generation
//------------------------------------------------------------------------------

Value *CodeGenerator::codegen(const Statement &stmt) {
  if (auto *vd = dynamic_cast<const VarDecl *>(&stmt))
    return visitVarDecl(*vd);
  if (auto *es = dynamic_cast<const ExprStmt *>(&stmt))
    return codegen(*es->expr);
  if (auto *ifs = dynamic_cast<const IfStmt *>(&stmt))
    return visitIfStmt(*ifs);
  if (auto *loop = dynamic_cast<const WhileStmt *>(&stmt))
    return visitWhileStmt(*loop);
  if (auto *ret = dynamic_cast<const Return *>(&stmt))
    return visitReturn(*ret);
  return logError("Unknown statement type");
}

Value *CodeGenerator::visitVarDecl(const VarDecl &decl) {
  std::string varName = decl.name.token.getWord();
  Type *ty = TypeResolver::getLLVMType(context, decl.type);
  if (!ty)
    return logError(("Unknown type: " + varName).c_str());

  AllocaInst *alloc = builder.CreateAlloca(ty, nullptr, varName);

  if (decl.initializer) {
    if (decl.isMove)
      return handleMoveInitialization(decl, varName, ty, alloc);
    else
      return handleCopyInitialization(decl, varName, ty, alloc);
  }

  namedValues[varName] = {alloc, ty, false, false};
  return alloc;
}

Value *CodeGenerator::handleMoveInitialization(const VarDecl &decl,
                                               const std::string &varName,
                                               Type *ty, AllocaInst *alloc) {
  auto *identExpr = dynamic_cast<const IdentExpr *>(decl.initializer.get());
  if (!identExpr)
    return logError("Move requires variable on right-hand side");

  std::string sourceName = identExpr->name.token.getWord();
  auto srcIt = namedValues.find(sourceName);
  if (srcIt == namedValues.end())
    return logError(("Unknown variable: " + sourceName).c_str());
  if (srcIt->second.isMoved)
    return logError(("Already moved: " + sourceName).c_str());
  if (srcIt->second.isBorrowed)
    return logError(("Cannot move borrowed value: " + sourceName).c_str());

  Value *srcVal = builder.CreateLoad(srcIt->second.type, srcIt->second.alloca,
                                     sourceName + "_load");
  builder.CreateStore(srcVal, alloc);
  srcIt->second.isMoved = true;

  namedValues[varName] = {alloc, ty, false, false};
  return alloc;
}

Value *CodeGenerator::handleCopyInitialization(const VarDecl &decl,
                                               const std::string &varName,
                                               Type *ty, AllocaInst *alloc) {
  Value *initVal = codegen(*decl.initializer);
  if (!initVal)
    return logError(("Failed to initialize: " + varName).c_str());

  if (TypeResolver::isString(ty)) {
    StructType *stringTy = cast<StructType>(ty);
    Value *loadedStr = builder.CreateLoad(stringTy, initVal, "src_str");
    Value *srcData = builder.CreateExtractValue(loadedStr, {0}, "src_data");
    Value *srcLen = builder.CreateExtractValue(loadedStr, {1}, "src_len");
    Value *srcCap = builder.CreateExtractValue(loadedStr, {2}, "src_cap");

    llvm::Function *mallocF = module->getFunction("malloc");
    llvm::Function *memcpyF = module->getFunction("llvm.memcpy.p0.p0.i64");
    if (!mallocF || !memcpyF)
      return logError("malloc/memcpy not declared");

    Value *newMem = builder.CreateCall(mallocF, {srcCap}, "string_clone");
    builder.CreateCall(
        memcpyF, {newMem, srcData, srcLen, ConstantInt::getFalse(context)});

    builder.CreateStore(newMem, builder.CreateStructGEP(stringTy, alloc, 0));
    builder.CreateStore(srcLen, builder.CreateStructGEP(stringTy, alloc, 1));
    builder.CreateStore(srcCap, builder.CreateStructGEP(stringTy, alloc, 2));

    namedValues[varName] = {alloc, ty, false, false};
    return alloc;
  }

  if (TypeResolver::isArrayType(ty)) {
    StructType *arrStructTy = cast<StructType>(ty);
    Value *loadedArr = builder.CreateLoad(arrStructTy, initVal, "arr_copy");
    builder.CreateStore(loadedArr, alloc);
    namedValues[varName] = {alloc, ty, false, false};
    return alloc;
  }

  // Type coercions for scalar initializers
  if (ty->isIntegerTy() && initVal->getType()->isIntegerTy()) {
    unsigned tb = ty->getIntegerBitWidth(),
             sb = initVal->getType()->getIntegerBitWidth();
    if (tb > sb)
      initVal = builder.CreateSExt(initVal, ty, "iext");
    else if (tb < sb)
      initVal = builder.CreateTrunc(initVal, ty, "itrunc");
  } else if (ty->isFloatingPointTy() && initVal->getType()->isIntegerTy()) {
    initVal = builder.CreateSIToFP(initVal, ty, "i2f");
  } else if (ty->isDoubleTy() && initVal->getType()->isFloatTy()) {
    initVal = builder.CreateFPExt(initVal, ty, "f2d");
  } else if (ty->isFloatTy() && initVal->getType()->isDoubleTy()) {
    initVal = builder.CreateFPTrunc(initVal, ty, "d2f");
  } else if (ty->isIntegerTy() && initVal->getType()->isFloatingPointTy()) {
    initVal = builder.CreateFPToSI(initVal, ty, "f2i");
  }

  builder.CreateStore(initVal, alloc);
  namedValues[varName] = {alloc, ty, false, false};
  return alloc;
}

Value *CodeGenerator::visitIfStmt(const IfStmt &stmt) {
  Value *cond = codegen(*stmt.condition);
  if (!cond)
    return nullptr;
  if (!cond->getType()->isIntegerTy(1))
    cond = builder.CreateICmpNE(cond, ConstantInt::get(cond->getType(), 0),
                                "ifcond");

  llvm::Function *func = builder.GetInsertBlock()->getParent();
  BasicBlock *thenBB = BasicBlock::Create(context, "then", func);
  BasicBlock *elseBB = BasicBlock::Create(context, "else");
  BasicBlock *mergeBB = BasicBlock::Create(context, "ifcont");

  builder.CreateCondBr(cond, thenBB, elseBB);

  builder.SetInsertPoint(thenBB);
  codegen(*stmt.thenBranch);
  if (!builder.GetInsertBlock()->getTerminator())
    builder.CreateBr(mergeBB);

  func->insert(func->end(), elseBB);
  builder.SetInsertPoint(elseBB);
  if (stmt.elseBranch)
    codegen(*stmt.elseBranch);
  if (!builder.GetInsertBlock()->getTerminator())
    builder.CreateBr(mergeBB);

  func->insert(func->end(), mergeBB);
  builder.SetInsertPoint(mergeBB);
  return nullptr;
}

Value *CodeGenerator::visitWhileStmt(const WhileStmt &stmt) {
  llvm::Function *func = builder.GetInsertBlock()->getParent();

  BasicBlock *condBB = BasicBlock::Create(context, "while.cond", func);
  BasicBlock *bodyBB = BasicBlock::Create(context, "while.body");
  BasicBlock *exitBB = BasicBlock::Create(context, "while.end");

  builder.CreateBr(condBB);

  builder.SetInsertPoint(condBB);
  Value *cond = codegen(*stmt.condition);
  if (!cond)
    return nullptr;
  if (!cond->getType()->isIntegerTy(1))
    cond = builder.CreateICmpNE(cond, ConstantInt::get(cond->getType(), 0),
                                "cond");
  builder.CreateCondBr(cond, bodyBB, exitBB);

  func->insert(func->end(), bodyBB);
  builder.SetInsertPoint(bodyBB);
  codegen(*stmt.doBranch);
  if (!builder.GetInsertBlock()->getTerminator())
    builder.CreateBr(condBB);

  func->insert(func->end(), exitBB);
  builder.SetInsertPoint(exitBB);
  return nullptr;
}

Value *CodeGenerator::visitReturn(const Return &stmt) {
  if (stmt.value) {
    Value *v = codegen(**stmt.value);
    if (!v)
      return nullptr;
    builder.CreateRet(v);
  } else {
    builder.CreateRetVoid();
  }
  return nullptr;
}

void CodeGenerator::codegen(const Block &block) {
  for (const auto &stmt : block.statements) {
    if (builder.GetInsertBlock()->getTerminator())
      break;
    codegen(*stmt);
  }
}

//------------------------------------------------------------------------------
// Function Code Generation
//------------------------------------------------------------------------------

llvm::Function *CodeGenerator::codegen(const AST_H::Function &func) {
  std::string fname = normalizeFunctionName(func.name.token.getWord());
  Type *retTy = TypeResolver::getLLVMType(context, func.returnType);

  namedValues.clear();

  std::vector<Type *> paramTypes;
  for (const auto &p : func.params) {
    Type *t = TypeResolver::getLLVMType(context, p.type);
    if (!t)
      return nullptr;
    paramTypes.push_back(t);
  }

  llvm::Function *f = module->getFunction(fname);
  if (!f) {
    auto *ft = FunctionType::get(retTy, paramTypes, false);
    f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, fname,
                               *module);
  }
  f->addFnAttr("stackrealignment");

  BasicBlock *bb = BasicBlock::Create(context, "entry", f);
  builder.SetInsertPoint(bb);

  namedValues.clear();
  size_t idx = 0;
  for (auto &arg : f->args()) {
    const auto &param = func.params[idx++];
    std::string name = param.name.token.getWord();
    AllocaInst *alloc = builder.CreateAlloca(arg.getType(), nullptr, name);
    builder.CreateStore(&arg, alloc);
    namedValues[name] = {alloc, arg.getType(), false, false};
  }

  codegen(*func.body);

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

//------------------------------------------------------------------------------
// Program Generation
//------------------------------------------------------------------------------

bool CodeGenerator::generate(const Program &program,
                             const std::string &outputFilename) {
  namedValues.clear();

  Type *ptrTy = PointerType::get(context, 0);
  if (!module->getFunction("printf")) {
    auto *printfTy =
        FunctionType::get(Type::getInt32Ty(context), {ptrTy}, true);
    llvm::Function::Create(printfTy, llvm::Function::ExternalLinkage, "printf",
                           *module);
  }

  declareExternalFunctions();

  for (const auto &f : program.functions) {
    if (!codegen(*f))
      return false;
  }

  module->print(llvm::outs(), nullptr); // debug dump

  std::error_code ec;
  raw_fd_ostream out(outputFilename + ".ll", ec, sys::fs::OF_None);
  if (ec) {
    errs() << "Could not open output: " << ec.message() << "\n";
    return false;
  }

  module->print(out, nullptr);
  return true;
}
