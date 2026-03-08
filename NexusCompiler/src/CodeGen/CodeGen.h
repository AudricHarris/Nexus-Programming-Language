#ifndef CodeGen_H
#define CodeGen_H

#include "../AST/AST.h"
#include "Emitters/ArrayEmitter.h"
#include "Emitters/PrintEmitter.h"
#include "Emitters/StringEmitter.h"
#include "Manager/ArithmeticManager.h"
#include "Manager/ScopeManager.h"
#include "VarInfo.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

// -------------- //
// Code generator //
// -------------- //
class CodeGenerator {
public:
  CodeGenerator();

  bool generate(const Program &program, const std::string &outputFilename);

  static bool isCStringPointer(llvm::Type *ty);

private:
  // LLVM state
  llvm::LLVMContext context;
  std::unique_ptr<llvm::Module> module;
  llvm::IRBuilder<> builder;

  // Symbol tables
  std::map<std::string, VarInfo> namedValues;
  std::map<std::string, std::vector<bool>> borrowRefParams; // callee → isRef[]

  // Subsystems
  ScopeManager scopeMgr;

  // Temporaries whose heap buffers must be freed after a statement
  std::vector<llvm::Value *> pendingFrees;

  // Error
  llvm::Value *logError(const char *msg);

  // Expression visitors
  llvm::Value *visitIdentifier(const IdentExpr &e);
  llvm::Value *visitIntLit(const IntLitExpr &e);
  llvm::Value *visitFloatLit(const FloatLitExpr &e);
  llvm::Value *visitStrLit(const StrLitExpr &e);
  llvm::Value *visitBoolLit(const BoolLitExpr &e);
  llvm::Value *visitBinary(const BinaryExpr &e);
  llvm::Value *visitUnary(const UnaryExpr &e);
  llvm::Value *visitAssign(const AssignExpr &e);
  llvm::Value *visitIncrement(const Increment &e);
  llvm::Value *visitDecrement(const Decrement &e);
  llvm::Value *visitCall(const CallExpr &e);
  llvm::Value *visitNewArray(const NewArrayExpr &e);
  llvm::Value *visitArrayIndex(const ArrayIndexExpr &e);
  llvm::Value *visitArrayIndexAssign(const ArrayIndexAssignExpr &e);
  llvm::Value *visitLengthProperty(const LengthPropertyExpr &e);

  // Expression dispatch
  llvm::Value *codegen(const Expression &expr);

  // Statement visitors
  llvm::Value *visitVarDecl(const VarDecl &d);
  llvm::Value *visitIfStmt(const IfStmt &s);
  llvm::Value *visitWhileStmt(const WhileStmt &s);
  llvm::Value *visitReturn(const Return &s);

  llvm::Value *codegen(const Statement &stmt);
  void codegen(const Block &block);
  llvm::Function *codegen(const AST_H::Function &func);

  // VarDecl initialisation helpers
  llvm::Value *initCopy(const VarDecl &d, const std::string &name,
                        llvm::Type *ty, llvm::AllocaInst *alloca);
  llvm::Value *initMove(const VarDecl &d, const std::string &name,
                        llvm::Type *ty, llvm::AllocaInst *alloca);
  llvm::Value *initBorrow(const VarDecl &d, const std::string &name,
                          llvm::AllocaInst *alloca);

  // Increment / decrement helper
  llvm::Value *generateIncrDecr(const std::string &varName, bool isInc);

  // Pending-free helpers
  void scheduleStringFree(llvm::Value *strAlloca);
  void flushPendingFrees();

  // free() lazy-declare
  llvm::Function *getFree();
};

#endif // CodeGen_H
