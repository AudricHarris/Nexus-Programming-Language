#pragma once
#include "../AST/AST.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"

class TypeResolver {
public:
  static llvm::Type *fromName(llvm::LLVMContext &ctx, const std::string &t);
  static llvm::Type *fromTypeDesc(llvm::LLVMContext &ctx, const TypeDesc &td);
  static llvm::StructType *getStringType(llvm::LLVMContext &ctx);
  static llvm::StructType *getOrCreateArrayStruct(llvm::LLVMContext &ctx,
                                                  llvm::Type *elementType);
  static bool isString(llvm::Type *ty);
  static bool isArray(llvm::Type *ty);
  static bool isNumeric(llvm::Type *ty);
  static bool isPtr(llvm::Type *ty);
  static llvm::Type *elemType(llvm::LLVMContext &ctx, llvm::StructType *arrTy);
  static llvm::Type *largerType(llvm::Type *a, llvm::Type *b);
  static llvm::Value *coerce(llvm::IRBuilder<> &B, llvm::Value *val,
                             llvm::Type *target, bool srcUnsigned = false);

private:
  static std::string typeName(llvm::Type *ty);
};
