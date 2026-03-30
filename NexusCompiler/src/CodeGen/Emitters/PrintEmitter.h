#ifndef PRINT_EMITTER_H
#define PRINT_EMITTER_H

#include "../../AST/AST.h"
#include "../VarInfo.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include <map>
#include <string>

// ------------------------------------------------------------------------ //
// PrintEmitter — Printf/Print wrappers for both Nexus and C calling styles //
// ------------------------------------------------------------------------ //
class PrintEmitter {
public:
  static llvm::Value *handlePrintf(const CallExpr &e, llvm::IRBuilder<> &B,
                                   llvm::LLVMContext &ctx, llvm::Module *M,
                                   const std::map<std::string, VarInfo> &vars);

  static llvm::Value *handlePrint(const CallExpr &e, llvm::IRBuilder<> &B,
                                  llvm::LLVMContext &ctx, llvm::Module *M);

  // ------------------------------------- //
  // Colour helpers (used by handlePrintf) //
  // ------------------------------------- //
  static std::string hexToAnsi(const std::string &hex);
  static std::string replaceHexColors(const std::string &input);
};

#endif // PRINT_EMITTER_H
