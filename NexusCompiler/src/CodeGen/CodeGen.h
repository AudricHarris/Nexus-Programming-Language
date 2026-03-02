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

// ─────────────────────────────────────────────────────────────────────────────
// Per-variable metadata tracked during code generation
// ─────────────────────────────────────────────────────────────────────────────
struct VarInfo {
  llvm::AllocaInst *allocaInst = nullptr;
  llvm::Type *type = nullptr;
  bool isBorrowed = false;
  bool isMoved = false;
  bool isReference = false;          // alloca stores ptr-to-T
  bool isConst = false;              // immutable after initialisation
  llvm::Type *pointeeType = nullptr; // T when isReference == true
  std::string sourceName;            // source variable for borrows

  VarInfo() = default;

  VarInfo(llvm::AllocaInst *a, llvm::Type *t, bool borrowed, bool moved,
          bool ref = false, bool c = false, std::string src = "")
      : allocaInst(a), type(t), isBorrowed(borrowed), isMoved(moved),
        isReference(ref), isConst(c), sourceName(std::move(src)) {}
};

// ─────────────────────────────────────────────────────────────────────────────
// Code generator
// ─────────────────────────────────────────────────────────────────────────────
class CodeGenerator {
public:
  CodeGenerator();

  /// Compile `program` and emit LLVM IR to `<outputFilename>.ll`.
  bool generate(const Program &program, const std::string &outputFilename);

  static bool isCStringPointer(llvm::Type *ty);

private:
  // ── LLVM state ──────────────────────────────────────────────────────────
  llvm::LLVMContext context;
  std::unique_ptr<llvm::Module> module;
  llvm::IRBuilder<> builder;

  // ── Symbol tables ────────────────────────────────────────────────────────
  std::map<std::string, VarInfo> namedValues;
  std::map<std::string, std::vector<bool>> borrowRefParams; // callee → isRef[]

  // ── Error ────────────────────────────────────────────────────────────────
  llvm::Value *logError(const char *msg);

  // ── Expression visitors ──────────────────────────────────────────────────
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

  // Array / string indexing
  llvm::Value *visitNewArray(const NewArrayExpr &e);
  llvm::Value *visitArrayIndex(const ArrayIndexExpr &e);     // arr[i] or str[i]
  llvm::Value *visitArray2DIndex(const Array2DIndexExpr &e); // arr[i][j]
  llvm::Value *visitArrayIndexAssign(const ArrayIndexAssignExpr &e);
  llvm::Value *visitArray2DIndexAssign(const Array2DIndexAssignExpr &e);

  // Property access
  llvm::Value *visitLengthProperty(const LengthPropertyExpr &e);
  llvm::Value *visitStringText(const StringTextExpr &e);

  // ── Expression dispatch ──────────────────────────────────────────────────
  llvm::Value *codegen(const Expression &expr);

  // ── Statement visitors ───────────────────────────────────────────────────
  llvm::Value *visitVarDecl(const VarDecl &d);
  llvm::Value *visitIfStmt(const IfStmt &s);
  llvm::Value *visitWhileStmt(const WhileStmt &s);
  llvm::Value *visitReturn(const Return &s);

  llvm::Value *codegen(const Statement &stmt);
  void codegen(const Block &block);
  llvm::Function *codegen(const Function &func);

  // ── VarDecl initialisation helpers ──────────────────────────────────────
  llvm::Value *initCopy(const VarDecl &d, const std::string &name,
                        llvm::Type *ty, llvm::AllocaInst *alloca);
  llvm::Value *initMove(const VarDecl &d, const std::string &name,
                        llvm::Type *ty, llvm::AllocaInst *alloca);
  llvm::Value *initBorrow(const VarDecl &d, const std::string &name,
                          llvm::AllocaInst *alloca);

  // ── Arithmetic helpers ───────────────────────────────────────────────────
  llvm::Value *promoteToDouble(llvm::Value *v);
  llvm::Value *promoteToInt(llvm::Value *v);
  llvm::Value *generateComparison(BinaryOp op, llvm::Value *l, llvm::Value *r,
                                  bool isFloat);
  llvm::Value *generateIncrDecr(const std::string &varName, bool isInc);

  // ── Print helpers ────────────────────────────────────────────────────────
  llvm::Value *handlePrintf(const CallExpr &e);
  llvm::Value *handlePrint(const CallExpr &e);

  // ── Colour helpers ───────────────────────────────────────────────────────
  std::string hexToAnsi(const std::string &hex);
  std::string replaceHexColors(const std::string &input);
};

#endif // CodeGen_H
