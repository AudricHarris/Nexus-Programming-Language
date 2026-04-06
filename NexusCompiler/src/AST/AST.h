#ifndef AST_H
#define AST_H

#include "../Token/TokenType.h"
#include "ExprVisitor.h"
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

// ------------- //
// JSON helpers  //
// ------------- //
namespace json_utils {
inline std::string escape(const std::string &s) {
  std::ostringstream oss;
  oss << '"';
  for (char c : s) {
    switch (c) {
    case '"':
      oss << "\\\"";
      break;
    case '\\':
      oss << "\\\\";
      break;
    case '\b':
      oss << "\\b";
      break;
    case '\f':
      oss << "\\f";
      break;
    case '\n':
      oss << "\\n";
      break;
    case '\r':
      oss << "\\r";
      break;
    case '\t':
      oss << "\\t";
      break;
    default:
      if (static_cast<unsigned char>(c) < 32) {
        char buf[7];
        snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
        oss << buf;
      } else {
        oss << c;
      }
    }
  }
  oss << '"';
  return oss.str();
}
} // namespace json_utils

// -------------------- //
// Forward declarations //
// -------------------- //
struct Expression;
struct Statement;
struct Block;
struct Function;

using ExprPtr = std::unique_ptr<Expression>;

// ------------------ //
// Core token wrapper //
// ------------------ //
struct Identifier {
  Token token;
  explicit Identifier(const Token &t) : token(t) {}
};

// ---------------- //
// Type descriptors //
// ---------------- //
struct TypeDesc {
  Identifier base;
  int dimensions = 0;
  bool isConst = false;
  bool isPtr = false;

  explicit TypeDesc(const Identifier &b, int dims = 0, bool c = false,
                    bool ptr = false)
      : base(b), dimensions(dims), isConst(c),
        isPtr(ptr || b.token.getWord() == "ptr") {}

  explicit TypeDesc(Identifier &&b, int dims = 0, bool c = false,
                    bool ptr = false)
      : base(std::move(b)), dimensions(dims), isConst(c),
        isPtr(ptr || base.token.getWord() == "ptr") {}

  const Identifier &elementType() const { return base; }

  std::string fullName() const {
    if (isPtr)
      return "ptr";
    std::string name = base.token.getWord();
    for (int i = 0; i < dimensions; ++i)
      name = "array." + name;
    return name;
  }
};

using ArrayType = TypeDesc;

// --------- //
// Parameter //
// --------- //
struct Parameter {
  TypeDesc type;
  Identifier name;
  bool isBorrowRef = false;
  bool isConst = false;

  Parameter(TypeDesc t, Identifier n, bool ref = false, bool c = false)
      : type(std::move(t)), name(std::move(n)), isBorrowRef(ref), isConst(c) {}
};

// --------- //
// Operators //
// --------- //
enum class BinaryOp {
  Add,
  Sub,
  Mul,
  Div,
  DivFloor,
  Mod,
  Eq,
  Ne,
  Lt,
  Gt,
  Le,
  Ge,
  And,
  BitAnd,
  Or
};

enum class UnaryOp { Negate, Not };
enum class AssignKind { Copy, Move, Borrow };

inline std::string toString(BinaryOp op) {
  switch (op) {
  case BinaryOp::Add:
    return "Add";
  case BinaryOp::Sub:
    return "Sub";
  case BinaryOp::Mul:
    return "Mul";
  case BinaryOp::Div:
    return "Div";
  case BinaryOp::DivFloor:
    return "DivFloor";
  case BinaryOp::Mod:
    return "Mod";
  case BinaryOp::Eq:
    return "Eq";
  case BinaryOp::Ne:
    return "Ne";
  case BinaryOp::Lt:
    return "Lt";
  case BinaryOp::Gt:
    return "Gt";
  case BinaryOp::Le:
    return "Le";
  case BinaryOp::Ge:
    return "Ge";
  case BinaryOp::And:
    return "And";
  case BinaryOp::BitAnd:
    return "BitAnd";
  case BinaryOp::Or:
    return "Or";
  }
  return "Unknown";
}

inline std::string toString(UnaryOp op) {
  switch (op) {
  case UnaryOp::Negate:
    return "Negate";
  case UnaryOp::Not:
    return "Not";
  }
  return "Unknown";
}

// --------------- //
// Expression base //
// --------------- //
struct Expression {
  virtual ~Expression() = default;
  virtual void toJson(std::ostream &os, int indent = 0) const = 0;
  // Dispatch to a visitor — every concrete node overrides this.
  virtual llvm::Value *accept(ExprVisitor &v) const = 0;
};

// ------------------- //
// Literal expressions //
// ------------------- //
struct IntLitExpr : Expression {
  Token lit;
  explicit IntLitExpr(const Token &t) : lit(t) {}
  llvm::Value *accept(ExprVisitor &v) const override {
    return v.visitIntLit(*this);
  }
  void toJson(std::ostream &os, int indent) const override {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"IntLitExpr\",\"value\":"
       << json_utils::escape(lit.getWord()) << "}";
  }
};

struct FloatLitExpr : Expression {
  Token lit;
  explicit FloatLitExpr(const Token &t) : lit(t) {}
  llvm::Value *accept(ExprVisitor &v) const override {
    return v.visitFloatLit(*this);
  }
  void toJson(std::ostream &os, int indent) const override {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"FloatLitExpr\",\"value\":"
       << json_utils::escape(lit.getWord()) << "}";
  }
};

struct StrLitExpr : Expression {
  Token lit;
  explicit StrLitExpr(const Token &t) : lit(t) {}
  llvm::Value *accept(ExprVisitor &v) const override {
    return v.visitStrLit(*this);
  }
  void toJson(std::ostream &os, int indent) const override {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"StrLitExpr\",\"value\":"
       << json_utils::escape(lit.getWord()) << "}";
  }
};

struct BoolLitExpr : Expression {
  Token lit;
  explicit BoolLitExpr(const Token &t) : lit(t) {}
  llvm::Value *accept(ExprVisitor &v) const override {
    return v.visitBoolLit(*this);
  }
  void toJson(std::ostream &os, int indent) const override {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"BoolLitExpr\",\"value\":"
       << json_utils::escape(lit.getWord()) << "}";
  }
};

struct CharLitExpr : Expression {
  Token lit;
  explicit CharLitExpr(const Token &t) : lit(t) {}
  llvm::Value *accept(ExprVisitor &v) const override {
    return v.visitCharLit(*this);
  }
  void toJson(std::ostream &os, int indent) const override {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"CharLitExpr\",\"value\":"
       << json_utils::escape(lit.getWord()) << "}";
  }
};

struct NullLitExpr : Expression {
  NullLitExpr() = default;
  llvm::Value *accept(ExprVisitor &v) const override {
    return v.visitNullLit(*this);
  }
  void toJson(std::ostream &os, int indent) const override {
    os << std::string(indent, ' ') << "{\"kind\":\"NullLitExpr\"}";
  }
};

struct IdentExpr : Expression {
  Identifier name;
  explicit IdentExpr(const Identifier &n) : name(n) {}
  llvm::Value *accept(ExprVisitor &v) const override {
    return v.visitIdentifier(*this);
  }
  void toJson(std::ostream &os, int indent) const override {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"IdentExpr\",\"name\":"
       << json_utils::escape(name.token.getWord()) << "}";
  }
};

struct BinaryExpr : Expression {
  BinaryOp op;
  ExprPtr left, right;
  BinaryExpr(BinaryOp o, ExprPtr l, ExprPtr r)
      : op(o), left(std::move(l)), right(std::move(r)) {}
  llvm::Value *accept(ExprVisitor &v) const override {
    return v.visitBinary(*this);
  }
  void toJson(std::ostream &os, int indent) const override {
    std::string p(indent, ' ');
    os << p << "{\n"
       << p << "  \"kind\":\"BinaryExpr\",\n"
       << p << "  \"op\":" << json_utils::escape(toString(op)) << ",\n"
       << p << "  \"left\":";
    left->toJson(os, indent + 2);
    os << ",\n" << p << "  \"right\":";
    right->toJson(os, indent + 2);
    os << "\n" << p << "}";
  }
};

struct UnaryExpr : Expression {
  UnaryOp op;
  ExprPtr operand;
  UnaryExpr(UnaryOp o, ExprPtr e) : op(o), operand(std::move(e)) {}
  llvm::Value *accept(ExprVisitor &v) const override {
    return v.visitUnary(*this);
  }
  void toJson(std::ostream &os, int indent) const override {
    std::string p(indent, ' ');
    os << p
       << "{\"kind\":\"UnaryExpr\",\"op\":" << json_utils::escape(toString(op))
       << ",\"operand\":";
    operand->toJson(os, indent + 2);
    os << "}";
  }
};

struct CallExpr : Expression {
  Identifier callee;
  std::vector<ExprPtr> arguments;
  CallExpr(const Identifier &c, std::vector<ExprPtr> args)
      : callee(c), arguments(std::move(args)) {}
  llvm::Value *accept(ExprVisitor &v) const override {
    return v.visitCall(*this);
  }
  void toJson(std::ostream &os, int indent) const override {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"CallExpr\",\"callee\":"
       << json_utils::escape(callee.token.getWord()) << ",\"args\":[";
    for (size_t i = 0; i < arguments.size(); ++i) {
      if (i)
        os << ",";
      arguments[i]->toJson(os, indent + 2);
    }
    os << "]}";
  }
};

struct AssignExpr : Expression {
  Identifier target;
  ExprPtr value;
  AssignKind kind;
  AssignExpr(Identifier tgt, ExprPtr val, AssignKind k = AssignKind::Copy)
      : target(std::move(tgt)), value(std::move(val)), kind(k) {}
  llvm::Value *accept(ExprVisitor &v) const override {
    return v.visitAssign(*this);
  }
  void toJson(std::ostream &os, int indent) const override {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"AssignExpr\",\"target\":"
       << json_utils::escape(target.token.getWord()) << ",\"value\":";
    value->toJson(os, indent + 2);
    os << "}";
  }
};

struct Increment : Expression {
  Identifier target;
  explicit Increment(const Identifier &t) : target(t) {}
  llvm::Value *accept(ExprVisitor &v) const override {
    return v.visitIncrement(*this);
  }
  void toJson(std::ostream &os, int indent) const override {
    os << std::string(indent, ' ') << "{\"kind\":\"Increment\",\"target\":"
       << json_utils::escape(target.token.getWord()) << "}";
  }
};

struct Decrement : Expression {
  Identifier target;
  explicit Decrement(const Identifier &t) : target(t) {}
  llvm::Value *accept(ExprVisitor &v) const override {
    return v.visitDecrement(*this);
  }
  void toJson(std::ostream &os, int indent) const override {
    os << std::string(indent, ' ') << "{\"kind\":\"Decrement\",\"target\":"
       << json_utils::escape(target.token.getWord()) << "}";
  }
};

struct CompoundAssignExpr : Expression {
  Identifier target;
  BinaryOp op;
  ExprPtr value;

  CompoundAssignExpr(Identifier t, BinaryOp o, ExprPtr v)
      : target(std::move(t)), op(o), value(std::move(v)) {}

  llvm::Value *accept(ExprVisitor &v) const override {
    return v.visitCompoundAssign(*this);
  }
  void toJson(std::ostream &os, int indent) const override {
    os << std::string(indent, ' ') << "{\"kind\":\"CompoundAssignExpr\","
       << "\"target\":" << json_utils::escape(target.token.getWord()) << ","
       << "\"op\":\"" << toString(op) << "\","
       << "\"value\":";
    value->toJson(os, indent + 2);
    os << "}";
  }
};

// ARRAYS AST Structures
struct NewArrayExpr : Expression {
  TypeDesc arrayType;
  std::vector<ExprPtr> sizes;
  NewArrayExpr(TypeDesc t, std::vector<ExprPtr> sz)
      : arrayType(std::move(t)), sizes(std::move(sz)) {}
  llvm::Value *accept(ExprVisitor &v) const override {
    return v.visitNewArray(*this);
  }
  void toJson(std::ostream &os, int indent) const override {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"NewArrayExpr\",\"type\":"
       << json_utils::escape(arrayType.fullName())
       << ",\"dims\":" << sizes.size() << "}";
  }
};

struct ArrayIndexExpr : Expression {
  Identifier array;
  std::vector<ExprPtr> indices;
  ArrayIndexExpr(Identifier arr, std::vector<ExprPtr> idxs)
      : array(std::move(arr)), indices(std::move(idxs)) {}
  llvm::Value *accept(ExprVisitor &v) const override {
    return v.visitArrayIndex(*this);
  }
  void toJson(std::ostream &os, int indent) const override {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"ArrayIndexExpr\",\"array\":"
       << json_utils::escape(array.token.getWord()) << ",\"indices\":[";
    for (size_t i = 0; i < indices.size(); ++i) {
      if (i)
        os << ",";
      indices[i]->toJson(os, indent + 2);
    }
    os << "]}";
  }
};

struct ArrayIndexAssignExpr : Expression {
  Identifier array;
  std::vector<ExprPtr> indices;
  ExprPtr value;
  ArrayIndexAssignExpr(Identifier arr, std::vector<ExprPtr> idxs, ExprPtr val)
      : array(std::move(arr)), indices(std::move(idxs)), value(std::move(val)) {
  }
  llvm::Value *accept(ExprVisitor &v) const override {
    return v.visitArrayIndexAssign(*this);
  }
  void toJson(std::ostream &os, int indent) const override {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"ArrayIndexAssignExpr\",\"array\":"
       << json_utils::escape(array.token.getWord()) << ",\"indices\":[";
    for (size_t i = 0; i < indices.size(); ++i) {
      if (i)
        os << ",";
      indices[i]->toJson(os, indent + 2);
    }
    os << "]}";
  }
};

struct LengthPropertyExpr : Expression {
  Identifier name;
  explicit LengthPropertyExpr(const Identifier &n) : name(n) {}
  llvm::Value *accept(ExprVisitor &v) const override {
    return v.visitLengthProperty(*this);
  }
  void toJson(std::ostream &os, int indent) const override {
    os << std::string(indent, ' ')
       << "{\"kind\":\"LengthPropertyExpr\",\"name\":"
       << json_utils::escape(name.token.getWord()) << "}";
  }
};

struct IndexedLengthExpr : Expression {
  Identifier arrayName;
  std::vector<ExprPtr> indices;
  IndexedLengthExpr(Identifier name, std::vector<ExprPtr> idxs)
      : arrayName(std::move(name)), indices(std::move(idxs)) {}
  llvm::Value *accept(ExprVisitor &v) const override {
    return v.visitIndexedLength(*this);
  }
  void toJson(std::ostream &os, int indent) const override {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"IndexedLengthExpr\",\"name\":"
       << json_utils::escape(arrayName.token.getWord())
       << ",\"depth\":" << indices.size() << "}";
  }
};

struct FieldAccessExpr : Expression {
  ExprPtr object;
  std::string field;
  FieldAccessExpr(ExprPtr obj, std::string f)
      : object(std::move(obj)), field(std::move(f)) {}
  llvm::Value *accept(ExprVisitor &v) const override {
    return v.visitFieldAccess(*this);
  }
  void toJson(std::ostream &os, int indent) const override {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"FieldAccessExpr\","
       << "\"field\":" << json_utils::escape(field) << ",\"object\":";
    object->toJson(os, indent + 2);
    os << "}";
  }
};

struct FieldAssignExpr : Expression {
  ExprPtr object;
  std::string field;
  ExprPtr value;
  FieldAssignExpr(ExprPtr obj, std::string f, ExprPtr val)
      : object(std::move(obj)), field(std::move(f)), value(std::move(val)) {}
  llvm::Value *accept(ExprVisitor &v) const override {
    return v.visitFieldAssign(*this);
  }
  void toJson(std::ostream &os, int indent) const override {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"FieldAssignExpr\","
       << "\"field\":" << json_utils::escape(field) << ",\"value\":";
    value->toJson(os, indent + 2);
    os << "}";
  }
};

struct StructLitExpr : Expression {
  std::string typeName;
  std::vector<ExprPtr> values;
  StructLitExpr(std::string tn, std::vector<ExprPtr> vals)
      : typeName(std::move(tn)), values(std::move(vals)) {}
  llvm::Value *accept(ExprVisitor &v) const override {
    return v.visitStructLit(*this);
  }
  void toJson(std::ostream &os, int indent) const override {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"StructLitExpr\",\"type\":"
       << json_utils::escape(typeName) << ",\"values\":[";
    for (size_t i = 0; i < values.size(); ++i) {
      if (i)
        os << ",";
      values[i]->toJson(os, indent + 2);
    }
    os << "]}";
  }
};

// -------------- //
// Statement base //
// -------------- //
struct Statement {
  virtual ~Statement() = default;
  virtual void toJson(std::ostream &os, int indent = 0) const = 0;
  virtual llvm::Value *accept(StmtVisitor &v) const = 0;
};

// -------------------- //
// Variable declaration //
// -------------------- //
struct VarDecl : Statement {
  TypeDesc type;
  Identifier name;
  ExprPtr initializer;
  AssignKind kind;
  bool isConst = false;

  VarDecl(TypeDesc t, Identifier n, ExprPtr init,
          AssignKind k = AssignKind::Copy, bool c = false)
      : type(std::move(t)), name(std::move(n)), initializer(std::move(init)),
        kind(k), isConst(c) {}

  VarDecl(TypeDesc t, Identifier n)
      : type(std::move(t)), name(std::move(n)), initializer(nullptr),
        kind(AssignKind::Copy), isConst(false) {}

  llvm::Value *accept(StmtVisitor &v) const override {
    return v.visitVarDecl(*this);
  }

  void toJson(std::ostream &os, int indent) const override {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"VarDecl\","
       << "\"const\":" << (isConst ? "true" : "false") << ","
       << "\"type\":" << json_utils::escape(type.fullName()) << ","
       << "\"name\":" << json_utils::escape(name.token.getWord()) << ","
       << "\"assignKind\":\""
       << (kind == AssignKind::Copy   ? "Copy"
           : kind == AssignKind::Move ? "Move"
                                      : "Borrow")
       << "\","
       << "\"init\":";
    if (initializer)
      initializer->toJson(os, indent + 2);
    else
      os << "null";
    os << "}";
  }
};

// ------------ //
// Control flow //
// ------------ //
struct Block {
  std::vector<std::unique_ptr<Statement>> statements;

  Block() = default;
  explicit Block(std::vector<std::unique_ptr<Statement>> s)
      : statements(std::move(s)) {}
  void toJson(std::ostream &os, int indent = 0) const {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"Block\",\"stmts\":[";
    for (size_t i = 0; i < statements.size(); ++i) {
      if (i)
        os << ",";
      statements[i]->toJson(os, indent + 2);
    }
    os << "]}";
  }
};

struct IfStmt : Statement {
  ExprPtr condition;
  std::unique_ptr<Block> thenBranch;
  std::unique_ptr<Block> elseBranch;

  IfStmt(ExprPtr cond, std::unique_ptr<Block> thenB,
         std::unique_ptr<Block> elseB = nullptr)
      : condition(std::move(cond)), thenBranch(std::move(thenB)),
        elseBranch(std::move(elseB)) {}
  llvm::Value *accept(StmtVisitor &v) const override {
    return v.visitIfStmt(*this);
  }
  void toJson(std::ostream &os, int indent) const override {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"IfStmt\",\"cond\":";
    condition->toJson(os, indent + 2);
    os << "}";
  }
};

struct WhileStmt : Statement {
  ExprPtr condition;
  std::unique_ptr<Block> doBranch;
  WhileStmt(ExprPtr cond, std::unique_ptr<Block> body)
      : condition(std::move(cond)), doBranch(std::move(body)) {}
  llvm::Value *accept(StmtVisitor &v) const override {
    return v.visitWhileStmt(*this);
  }
  void toJson(std::ostream &os, int indent) const override {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"WhileStmt\",\"cond\":";
    condition->toJson(os, indent + 2);
    os << "}";
  }
};

struct ForRangeStmt : Statement {
  TypeDesc varType;
  Identifier varName;
  ExprPtr start;
  ExprPtr end;
  ExprPtr step;
  std::unique_ptr<Block> body;

  ForRangeStmt(TypeDesc t, Identifier n, ExprPtr s, ExprPtr e, ExprPtr st,
               std::unique_ptr<Block> b)
      : varType(std::move(t)), varName(std::move(n)), start(std::move(s)),
        end(std::move(e)), step(std::move(st)), body(std::move(b)) {}
  llvm::Value *accept(StmtVisitor &v) const override {
    return v.visitForRange(*this);
  }
  void toJson(std::ostream &os, int indent) const override {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"ForRangeStmt\","
       << "\"var\":" << json_utils::escape(varName.token.getWord()) << "}";
  }
};

struct Return : Statement {
  std::optional<ExprPtr> value;
  Return() = default;
  explicit Return(ExprPtr v) : value(std::move(v)) {}
  llvm::Value *accept(StmtVisitor &v) const override {
    return v.visitReturn(*this);
  }
  void toJson(std::ostream &os, int indent) const override {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"Return\",\"value\":";
    if (value)
      (*value)->toJson(os, indent + 2);
    else
      os << "null";
    os << "}";
  }
};

struct Break : Statement {
  Break() = default;
  llvm::Value *accept(StmtVisitor &v) const override {
    return v.visitBreak(*this);
  }
  void toJson(std::ostream &os, int indent) const override {
    os << std::string(indent, ' ') << "{\"kind\":\"Break\"}";
  }
};

struct Continue : Statement {
  Continue() = default;
  llvm::Value *accept(StmtVisitor &v) const override {
    return v.visitContinue(*this);
  }
  void toJson(std::ostream &os, int indent) const override {
    os << std::string(indent, ' ') << "{\"kind\":\"Continue\"}";
  }
};

struct ExprStmt : Statement {
  ExprPtr expr;
  ExprStmt() = default;
  explicit ExprStmt(ExprPtr e) : expr(std::move(e)) {}
  llvm::Value *accept(StmtVisitor &v) const override {
    return v.visitExprStmt(*this);
  }
  void toJson(std::ostream &os, int indent) const override {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"ExprStmt\",\"expr\":";
    if (expr)
      expr->toJson(os, indent + 2);
    else
      os << "null";
    os << "}";
  }
};

// ------------------ //
// Function & Program //
// ------------------ //
struct Function {
  Identifier name;
  std::vector<Parameter> params;
  std::unique_ptr<Block> body;
  TypeDesc returnType;
  bool isPublic = false;

  Function(Identifier n, std::vector<Parameter> p, std::unique_ptr<Block> b,
           TypeDesc ret, bool pub = false)
      : name(std::move(n)), params(std::move(p)), body(std::move(b)),
        returnType(std::move(ret)), isPublic(pub) {}
  void toJson(std::ostream &os, int indent = 0) const {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"Function\","
       << "\"name\":" << json_utils::escape(name.token.getWord()) << ","
       << "\"public\":" << (isPublic ? "true" : "false") << ","
       << "\"return\":" << json_utils::escape(returnType.fullName()) << "}";
  }
};

// --------------------------------- //
// Extern "C" function declarations  //
// --------------------------------- //
struct ExternFuncDecl {
  std::string name;
  std::vector<TypeDesc> paramTypes;
  TypeDesc returnType;
  bool isPrivate = false;

  ExternFuncDecl(std::string n, std::vector<TypeDesc> pts, TypeDesc ret,
                 bool priv = false)
      : name(std::move(n)), paramTypes(std::move(pts)),
        returnType(std::move(ret)), isPrivate(priv) {}
  void toJson(std::ostream &os, int indent = 0) const {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"ExternFuncDecl\","
       << "\"name\":" << json_utils::escape(name) << ","
       << "\"private\":" << (isPrivate ? "true" : "false") << ","
       << "\"return\":" << json_utils::escape(returnType.fullName()) << "}";
  }
};

struct ExternBlock {
  std::vector<ExternFuncDecl> decls;
  ExternBlock() = default;
  explicit ExternBlock(std::vector<ExternFuncDecl> d) : decls(std::move(d)) {}
  void toJson(std::ostream &os, int indent = 0) const {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"ExternBlock\",\"decls\":[";
    for (size_t i = 0; i < decls.size(); ++i) {
      if (i)
        os << ",";
      decls[i].toJson(os, indent + 2);
    }
    os << "]}";
  }
};

// -------------------- //
// Struct declarations  //
// -------------------- //
struct StructField {
  TypeDesc type;
  std::string name;
  StructField(TypeDesc t, std::string n)
      : type(std::move(t)), name(std::move(n)) {}
  void toJson(std::ostream &os, int indent = 0) const {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"StructField\","
       << "\"type\":" << json_utils::escape(type.fullName()) << ","
       << "\"name\":" << json_utils::escape(name) << "}";
  }
};

struct StructDecl {
  std::string name;
  std::vector<StructField> fields;
  bool isPublic = false;

  StructDecl() = default;
  StructDecl(std::string n, std::vector<StructField> f, bool pub = false)
      : name(std::move(n)), fields(std::move(f)), isPublic(pub) {}
  void toJson(std::ostream &os, int indent = 0) const {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"StructDecl\","
       << "\"name\":" << json_utils::escape(name) << ","
       << "\"public\":" << (isPublic ? "true" : "false") << ","
       << "\"fields\":[";
    for (size_t i = 0; i < fields.size(); ++i) {
      if (i)
        os << ",";
      fields[i].toJson(os, indent + 2);
    }
    os << "]}";
  }
};

// ------------------- //
// Import declarations //
// ------------------- //
struct ImportPath {
  std::vector<std::string> segments;
  bool isStdLib = false;
  ImportPath() = default;
  ImportPath(std::vector<std::string> segs, bool stdlib)
      : segments(std::move(segs)), isStdLib(stdlib) {}
};

struct ImportDecl {
  ImportPath path;
  std::vector<std::string> symbols;
  bool selective = false;
  ImportDecl() = default;
  ImportDecl(ImportPath p, std::vector<std::string> syms, bool sel)
      : path(std::move(p)), symbols(std::move(syms)), selective(sel) {}
};

// ----------------------- //
// Global variable decl    //
// ----------------------- //
struct GlobalVarDecl {
  TypeDesc type;
  std::string name;
  std::unique_ptr<Expression> init;
  bool isConst = false;
  bool isPublic = false;

  GlobalVarDecl(TypeDesc t, std::string n, std::unique_ptr<Expression> i,
                bool c = false, bool pub = false)
      : type(std::move(t)), name(std::move(n)), init(std::move(i)), isConst(c),
        isPublic(pub) {}
  void toJson(std::ostream &os, int indent = 0) const {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"GlobalVarDecl\","
       << "\"type\":" << json_utils::escape(type.fullName()) << ","
       << "\"name\":" << json_utils::escape(name) << ","
       << "\"const\":" << (isConst ? "true" : "false") << ","
       << "\"public\":" << (isPublic ? "true" : "false") << ","
       << "\"init\":";
    if (init)
      init->toJson(os, indent + 2);
    else
      os << "null";
    os << "}";
  }
};

// ------- //
// Program //
// ------- //
struct Program {
  std::vector<std::unique_ptr<ImportDecl>> imports;
  std::vector<std::unique_ptr<GlobalVarDecl>> globals;
  std::vector<std::unique_ptr<Function>> functions;
  std::vector<std::unique_ptr<StructDecl>> structs;
  std::vector<ExternBlock> externBlocks;

  Program() = default;
  void toJson(std::ostream &os = std::cout, int indent = 0) const {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"Program\",";

    os << "\"structs\":[";
    for (size_t i = 0; i < structs.size(); ++i) {
      if (i)
        os << ",";
      structs[i]->toJson(os, indent + 2);
    }
    os << "],";

    os << "\"externBlocks\":[";
    for (size_t i = 0; i < externBlocks.size(); ++i) {
      if (i)
        os << ",";
      externBlocks[i].toJson(os, indent + 2);
    }
    os << "],";

    os << "\"globals\":[";
    for (size_t i = 0; i < globals.size(); ++i) {
      if (i)
        os << ",";
      globals[i]->toJson(os, indent + 2);
    }
    os << "],";

    os << "\"functions\":[";
    for (size_t i = 0; i < functions.size(); ++i) {
      if (i)
        os << ",";
      functions[i]->toJson(os, indent + 2);
    }
    os << "]}\n";
  }
};

// --------------------- //
// Compatibility helpers //
// --------------------- //
namespace AST_H {
using Function = ::Function;
} // namespace AST_H

#endif // AST_H
