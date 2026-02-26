#ifndef CodeGen_H
#define CodeGen_H

#include "../AST/AST.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

struct VarInfo {
  llvm::AllocaInst *alloca;
  llvm::Type *type;
  bool isBorrowed = false;
  bool isMoved = false;
};

class CodeGenerator {
public:
  CodeGenerator();
  bool generate(const Program &program, const std::string &outputFilename);

private:
  // Core LLVM components
  llvm::LLVMContext context;
  std::unique_ptr<llvm::Module> module;
  llvm::IRBuilder<> builder;
  std::map<std::string, VarInfo> namedValues;

  // Error handling
  llvm::Value *logError(const char *msg);

  // Expression visitors
  llvm::Value *visitIdentifier(const IdentExpr &expr);
  llvm::Value *visitLiteral(const IntLitExpr &expr);
  llvm::Value *visitLiteral(const FloatLitExpr &expr);
  llvm::Value *visitLiteral(const StrLitExpr &expr);
  llvm::Value *visitLiteral(const BoolLitExpr &expr);
  llvm::Value *visitBinary(const BinaryExpr &expr);
  llvm::Value *visitUnary(const UnaryExpr &expr);
  llvm::Value *visitAssignment(const AssignExpr &expr);
  llvm::Value *visitIncrement(const Increment &expr);
  llvm::Value *visitDecrement(const Decrement &expr);
  llvm::Value *visitCall(const CallExpr &expr);

  // Expression dispatch
  llvm::Value *codegen(const Expression &expr);

  // Statement visitors
  llvm::Value *visitVarDecl(const VarDecl &decl);
  llvm::Value *visitIfStmt(const IfStmt &stmt);
  llvm::Value *visitWhileStmt(const WhileStmt &stmt);
  llvm::Value *visitReturn(const Return &stmt);

  // Statement dispatch
  llvm::Value *codegen(const Statement &stmt);
  void codegen(const Block &block);
  llvm::Function *codegen(const Function &func);

  // Helper methods
  llvm::Value *promoteToDouble(llvm::Value *val);
  llvm::Value *promoteToInt(llvm::Value *val);
  llvm::Value *generateComparison(BinaryOp op, llvm::Value *left,
                                  llvm::Value *right, bool isFloat);
  llvm::Value *generateIncrementDecrement(const std::string &varName,
                                          bool isInc);

  // Initialization helpers
  llvm::Value *handleMoveInitialization(const VarDecl &decl,
                                        const std::string &varName,
                                        llvm::Type *ty,
                                        llvm::AllocaInst *alloc);
  llvm::Value *handleCopyInitialization(const VarDecl &decl,
                                        const std::string &varName,
                                        llvm::Type *ty,
                                        llvm::AllocaInst *alloc);

  // Print function helpers
  llvm::Value *handlePrintf(const CallExpr &expr);
  llvm::Value *handlePrint(const CallExpr &expr);

  // Color formatting
  std::string hexToAnsi(const std::string &hex);
  std::string replaceHexColors(const std::string &input);
};

#endif
