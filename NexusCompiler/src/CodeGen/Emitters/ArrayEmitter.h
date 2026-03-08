#ifndef ARRAY_EMITTER_H
#define ARRAY_EMITTER_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"

// ---------------------------------------------------------------- //
// ArrayEmitter — creation, indexing and length of primitive arrays //
// ---------------------------------------------------------------- //
namespace ArrayEmitter {

llvm::StructType *getArrayStructTy(llvm::LLVMContext &C);

llvm::Value *emitMalloc(llvm::IRBuilder<> &B, llvm::LLVMContext &C,
                        llvm::Module &M, llvm::Type *allocTy,
                        llvm::Value *count);

llvm::Value *makeND(llvm::IRBuilder<> &B, llvm::LLVMContext &C, llvm::Module &M,
                    llvm::Type *elementType,
                    llvm::ArrayRef<llvm::Value *> dims);

void emitArrayFree(llvm::IRBuilder<> &B, llvm::LLVMContext &C, llvm::Module &M,
                   llvm::Value *arrPtr, llvm::StructType *arrSt, int depth);

} // namespace ArrayEmitter

#endif // ARRAY_EMITTER_H
