#ifndef BUILTIN_EMITTER_H
#define BUILTIN_EMITTER_H

#include "../../AST/AST.h"
#include "../VarInfo.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include <map>
#include <string>

// ------------------------------------------------------------------------ //
// BuiltInEmitter : Read/Random/Etc... All the built in commands in nexus   //
// ------------------------------------------------------------------------ //
class BuiltinEmitter {
public:
  static llvm::Value *handleRandom(llvm::IRBuilder<> &B, llvm::LLVMContext &ctx,
                                   llvm::Module *M);

  static void emitRuntimeInit(llvm::IRBuilder<> &B, llvm::LLVMContext &ctx,
                              llvm::Module *M);

  static llvm::Value *handleRead(llvm::IRBuilder<> &B, llvm::LLVMContext &ctx,
                                 llvm::Module *M);
};

#endif // PRINT_EMITTER_H
