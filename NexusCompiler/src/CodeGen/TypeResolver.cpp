#include "TypeResolver.h"
#include "../AST/AST.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/Casting.h"
#include <string>

std::string TypeResolver::typeName(llvm::Type *ty) {
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
  if (auto *st = llvm::dyn_cast<llvm::StructType>(ty))
    return st->getName().str();
  return "unknown";
}

llvm::Type *TypeResolver::fromName(llvm::LLVMContext &ctx,
                                   const std::string &t) {
  if (t == "i32" || t == "int" || t == "integer")
    return llvm::Type::getInt32Ty(ctx);
  if (t == "i64" || t == "long")
    return llvm::Type::getInt64Ty(ctx);
  if (t == "i16" || t == "short")
    return llvm::Type::getInt16Ty(ctx);
  if (t == "i8" || t == "char")
    return llvm::Type::getInt8Ty(ctx);
  if (t == "f32" || t == "float")
    return llvm::Type::getFloatTy(ctx);
  if (t == "f64" || t == "double")
    return llvm::Type::getDoubleTy(ctx);
  if (t == "bool")
    return llvm::Type::getInt1Ty(ctx);
  if (t == "void")
    return llvm::Type::getVoidTy(ctx);
  if (t == "str" || t == "string")
    return getStringType(ctx);
  if (t == "ptr")
    return llvm::PointerType::get(ctx, 0);

  if (t.size() > 6 && t.substr(0, 6) == "array.") {
    std::string innerName = t.substr(6);
    llvm::Type *innerTy = fromName(ctx, innerName);
    if (!innerTy)
      return nullptr;
    return getOrCreateArrayStruct(ctx, innerTy);
  }

  return nullptr;
}

llvm::Type *TypeResolver::fromTypeDesc(llvm::LLVMContext &ctx,
                                       const TypeDesc &td) {
  llvm::Type *base = fromName(ctx, td.base.token.getWord());
  if (!base)
    return nullptr;

  llvm::Type *result = base;
  for (int i = 0; i < td.dimensions; ++i) {
    result = getOrCreateArrayStruct(ctx, result);
    if (!result)
      return nullptr;
  }
  return result;
}

llvm::StructType *TypeResolver::getStringType(llvm::LLVMContext &ctx) {
  llvm::StructType *st = llvm::StructType::getTypeByName(ctx, "string");
  if (!st) {
    st = llvm::StructType::create(ctx, "string");
    st->setBody(
        {llvm::PointerType::get(llvm::Type::getInt8Ty(ctx)->getContext(), 0),
         llvm::Type::getInt64Ty(ctx), llvm::Type::getInt64Ty(ctx)});
  }
  return st;
}

llvm::StructType *
TypeResolver::getOrCreateArrayStruct(llvm::LLVMContext &ctx,
                                     llvm::Type *elementType) {
  if (!elementType)
    return nullptr;
  std::string name = "array." + typeName(elementType);
  if (llvm::StructType *existing = llvm::StructType::getTypeByName(ctx, name))
    return existing;

  auto *dataPtrTy = llvm::PointerType::getUnqual(elementType->getContext());
  llvm::StructType *st = llvm::StructType::create(
      ctx, {llvm::Type::getInt64Ty(ctx), dataPtrTy}, name);
  return st;
}

bool TypeResolver::isString(llvm::Type *ty) {
  return ty && ty->isStructTy() &&
         llvm::cast<llvm::StructType>(ty)->getName() == "string";
}

bool TypeResolver::isArray(llvm::Type *ty) {
  return ty && ty->isStructTy() &&
         llvm::cast<llvm::StructType>(ty)->getName().starts_with("array.");
}

bool TypeResolver::isNumeric(llvm::Type *ty) {
  return ty && (ty->isIntegerTy() || ty->isFloatingPointTy());
}

bool TypeResolver::isPtr(llvm::Type *ty) { return ty && ty->isPointerTy(); }

llvm::Type *TypeResolver::elemType(llvm::LLVMContext &ctx,
                                   llvm::StructType *arrTy) {
  if (!arrTy || !isArray(arrTy))
    return nullptr;
  llvm::StringRef name = arrTy->getName();
  std::string innerName = name.substr(6).str();
  return fromName(ctx, innerName);
}

llvm::Type *TypeResolver::largerType(llvm::Type *a, llvm::Type *b) {
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
      return llvm::Type::getDoubleTy(a->getContext());
    return llvm::Type::getFloatTy(a->getContext());
  }

  if (a->isIntegerTy() && b->isIntegerTy()) {
    unsigned bits =
        std::max({a->getIntegerBitWidth(), b->getIntegerBitWidth(), 32u});
    return llvm::Type::getIntNTy(a->getContext(), bits);
  }

  return a;
}

llvm::Value *TypeResolver::coerce(llvm::IRBuilder<> &B, llvm::Value *val,
                                  llvm::Type *target) {
  if (!val || !target || val->getType() == target)
    return val;

  llvm::Type *src = val->getType();

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
  }

  return val;
}
