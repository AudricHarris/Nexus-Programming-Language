#ifndef STRING_EMITTER_H
#define STRING_EMITTER_H

#include "llvm/IR/Verifier.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Value.h>
#include <memory>

class StringOps {
public:
  static llvm::Value *fromLiteral(llvm::IRBuilder<> &B, llvm::LLVMContext &ctx,
                                  llvm::Module *M, const std::string &literal);

  static llvm::Value *fromParts(llvm::IRBuilder<> &B, llvm::LLVMContext &ctx,
                                llvm::Module *M, llvm::Value *dataPtr,
                                llvm::Value *length);

  static llvm::Value *concat(llvm::IRBuilder<> &B, llvm::LLVMContext &ctx,
                             llvm::Module *M, llvm::Value *leftStruct,
                             llvm::Value *rightStruct);

  static llvm::Value *fromValue(llvm::IRBuilder<> &B, llvm::LLVMContext &ctx,
                                llvm::Module *M, llvm::Value *val);

private:
  static llvm::Value *intToStr(llvm::IRBuilder<> &B, llvm::LLVMContext &ctx,
                               llvm::Module *M, llvm::Value *v);

  static llvm::Value *floatToStr(llvm::IRBuilder<> &B, llvm::LLVMContext &ctx,
                                 llvm::Module *M, llvm::Value *v);
};

#endif // !STRING_EMITTER_H
