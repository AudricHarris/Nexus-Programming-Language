#ifndef CodeGen_H
#define CodeGen_H

#include "../AST/AST.h"
#include "../AST/ExprVisitor.h"
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
#include <set>
#include <string>
#include <vector>

struct LoopContext {
  llvm::BasicBlock *condBB;
  llvm::BasicBlock *exitBB;
};

class CodeGenerator : public ExprVisitor, public StmtVisitor {
public:
  CodeGenerator();
  ~CodeGenerator() = default;

  bool generate(const Program &program, const std::string &outputFilename);

  static bool isCStringPointer(llvm::Type *ty);

  llvm::Value *visitIntLit(const IntLitExpr &e) override;
  llvm::Value *visitFloatLit(const FloatLitExpr &e) override;
  llvm::Value *visitStrLit(const StrLitExpr &e) override;
  llvm::Value *visitBoolLit(const BoolLitExpr &e) override;
  llvm::Value *visitCharLit(const CharLitExpr &e) override;
  llvm::Value *visitNullLit(const NullLitExpr &) override;
  llvm::Value *visitIdentifier(const IdentExpr &e) override;
  llvm::Value *visitBinary(const BinaryExpr &e) override;
  llvm::Value *visitUnary(const UnaryExpr &e) override;
  llvm::Value *visitCall(const CallExpr &e) override;
  llvm::Value *visitAssign(const AssignExpr &e) override;
  llvm::Value *visitIncrement(const Increment &e) override;
  llvm::Value *visitDecrement(const Decrement &e) override;
  llvm::Value *visitCompoundAssign(const CompoundAssignExpr &e) override;
  llvm::Value *visitNewArray(const NewArrayExpr &e) override;
  llvm::Value *visitArrayIndex(const ArrayIndexExpr &e) override;
  llvm::Value *visitArrayIndexAssign(const ArrayIndexAssignExpr &e) override;
  llvm::Value *visitLengthProperty(const LengthPropertyExpr &e) override;
  llvm::Value *visitIndexedLength(const IndexedLengthExpr &e) override;
  llvm::Value *visitFieldAccess(const FieldAccessExpr &e) override;
  llvm::Value *visitFieldAssign(const FieldAssignExpr &e) override;
  llvm::Value *visitStructLit(const StructLitExpr &e) override;

  llvm::Value *visitVarDecl(const VarDecl &d) override;
  llvm::Value *visitExprStmt(const ExprStmt &s) override;
  llvm::Value *visitIfStmt(const IfStmt &s) override;
  llvm::Value *visitWhileStmt(const WhileStmt &s) override;
  llvm::Value *visitForRange(const ForRangeStmt &s) override;
  llvm::Value *visitReturn(const Return &s) override;
  llvm::Value *visitBreak(const Break &) override;
  llvm::Value *visitContinue(const Continue &) override;

private:
  // LLVM state
  llvm::LLVMContext context;
  std::unique_ptr<llvm::Module> module;
  llvm::IRBuilder<> builder;
  std::vector<LoopContext> loopStack;

  // Symbol tables
  std::map<std::string, VarInfo> namedValues;
  std::map<std::string, VarInfo> globalValues;
  std::map<std::string, std::vector<bool>> borrowRefParams;

  // Struct definitions (populated at start of generate())
  std::vector<StructDecl *> structDefs;

  // Subsystems
  ScopeManager scopeMgr;

  // Error
  llvm::Value *logError(const char *msg);

  // Thin wrappers
  llvm::Value *codegen(const Expression &expr);
  llvm::Value *codegen(const Statement &stmt);
  void codegen(const Block &block);
  llvm::Function *codegen(const AST_H::Function &func);

  // Helpers
  llvm::AllocaInst *createEntryAlloca(llvm::Type *ty, const std::string &name);
  llvm::Value *generateIncrDecr(const std::string &varName, bool isInc);
  llvm::Function *getFree();

  std::pair<llvm::Value *, llvm::StructType *>
  resolveStructPtr(const Expression &expr);
};

#endif // CodeGen_H
