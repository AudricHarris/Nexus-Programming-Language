
#ifndef ARITHMETIC_MANAGER_H
#define ARITHMETIC_MANAGER_H

#include "../../AST/AST.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Value.h"

// --------------------------------------------------------- //
// ArithmeticManager — arithmetic operations and comparisons //
// --------------------------------------------------------- //
class ArithmeticManager {
public:
  static llvm::Value *promoteToDouble(llvm::IRBuilder<> &B, llvm::Value *v);
  static llvm::Value *promoteToInt(llvm::IRBuilder<> &B, llvm::LLVMContext &ctx,
                                   llvm::Value *v);

  static llvm::Value *generateComparison(llvm::IRBuilder<> &B, BinaryOp op,
                                         llvm::Value *l, llvm::Value *r,
                                         bool isFloat);

  static llvm::Value *emitBinaryOp(llvm::IRBuilder<> &B, llvm::LLVMContext &ctx,
                                   BinaryOp op, llvm::Value *l, llvm::Value *r);
};

#endif // ARITHMETIC_MANAGER_H
