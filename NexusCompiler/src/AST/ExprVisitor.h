#ifndef EXPR_VISITOR_H
#define EXPR_VISITOR_H

// Forward-declare every concrete Expression node so this header stays lean.
struct IntLitExpr;
struct FloatLitExpr;
struct StrLitExpr;
struct BoolLitExpr;
struct CharLitExpr;
struct NullLitExpr;
struct IdentExpr;
struct BinaryExpr;
struct UnaryExpr;
struct CastExpr;
struct CallExpr;
struct AssignExpr;
struct Increment;
struct Decrement;
struct NewArrayExpr;
struct ArrayIndexExpr;
struct ArrayIndexAssignExpr;
struct LengthPropertyExpr;
struct IndexedLengthExpr;
struct FieldAccessExpr;
struct FieldAssignExpr;
struct StructLitExpr;
struct CompoundAssignExpr;

namespace llvm {
class Value;
}

// Visitor for Expression nodes.
// Currently returns llvm::Value* since it's only used by the code generator.
struct ExprVisitor {
  virtual ~ExprVisitor() = default;

  virtual llvm::Value *visitIntLit(const IntLitExpr &) = 0;
  virtual llvm::Value *visitFloatLit(const FloatLitExpr &) = 0;
  virtual llvm::Value *visitStrLit(const StrLitExpr &) = 0;
  virtual llvm::Value *visitBoolLit(const BoolLitExpr &) = 0;
  virtual llvm::Value *visitCharLit(const CharLitExpr &) = 0;
  virtual llvm::Value *visitNullLit(const NullLitExpr &) = 0;
  virtual llvm::Value *visitIdentifier(const IdentExpr &) = 0;
  virtual llvm::Value *visitBinary(const BinaryExpr &) = 0;
  virtual llvm::Value *visitUnary(const UnaryExpr &) = 0;
  virtual llvm::Value *visitCast(const CastExpr &) = 0;
  virtual llvm::Value *visitCall(const CallExpr &) = 0;
  virtual llvm::Value *visitAssign(const AssignExpr &) = 0;
  virtual llvm::Value *visitIncrement(const Increment &) = 0;
  virtual llvm::Value *visitDecrement(const Decrement &) = 0;
  virtual llvm::Value *visitNewArray(const NewArrayExpr &) = 0;
  virtual llvm::Value *visitArrayIndex(const ArrayIndexExpr &) = 0;
  virtual llvm::Value *visitArrayIndexAssign(const ArrayIndexAssignExpr &) = 0;
  virtual llvm::Value *visitLengthProperty(const LengthPropertyExpr &) = 0;
  virtual llvm::Value *visitIndexedLength(const IndexedLengthExpr &) = 0;
  virtual llvm::Value *visitFieldAccess(const FieldAccessExpr &) = 0;
  virtual llvm::Value *visitFieldAssign(const FieldAssignExpr &) = 0;
  virtual llvm::Value *visitStructLit(const StructLitExpr &) = 0;
  virtual llvm::Value *visitCompoundAssign(const CompoundAssignExpr &) = 0;
};

// Pure-virtual visitor for Statement nodes.
struct VarDecl;
struct ExprStmt;
struct IfStmt;
struct WhileStmt;
struct ForRangeStmt;
struct Return;
struct Break;
struct Continue;

struct StmtVisitor {
  virtual ~StmtVisitor() = default;

  virtual llvm::Value *visitVarDecl(const VarDecl &) = 0;
  virtual llvm::Value *visitExprStmt(const ExprStmt &) = 0;
  virtual llvm::Value *visitIfStmt(const IfStmt &) = 0;
  virtual llvm::Value *visitWhileStmt(const WhileStmt &) = 0;
  virtual llvm::Value *visitForRange(const ForRangeStmt &) = 0;
  virtual llvm::Value *visitReturn(const Return &) = 0;
  virtual llvm::Value *visitBreak(const Break &) = 0;
  virtual llvm::Value *visitContinue(const Continue &) = 0;
};

#endif // EXPR_VISITOR_H
