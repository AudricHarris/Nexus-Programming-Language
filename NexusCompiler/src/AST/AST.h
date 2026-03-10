#ifndef AST_H
#define AST_H

#include "../Token/TokenType.h"
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

  explicit TypeDesc(const Identifier &b, int dims = 0, bool c = false)
      : base(b), dimensions(dims), isConst(c) {}
  explicit TypeDesc(Identifier &&b, int dims = 0, bool c = false)
      : base(std::move(b)), dimensions(dims), isConst(c) {}

  const Identifier &elementType() const { return base; }

  std::string fullName() const {
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
};

// ------------------- //
// Literal expressions //
// ------------------- //
struct IntLitExpr : Expression {
  Token lit;

  explicit IntLitExpr(const Token &t) : lit(t) {}
  void toJson(std::ostream &os, int indent) const override {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"IntLitExpr\",\"value\":"
       << json_utils::escape(lit.getWord()) << "}";
  }
};

struct FloatLitExpr : Expression {
  Token lit;

  explicit FloatLitExpr(const Token &t) : lit(t) {}
  void toJson(std::ostream &os, int indent) const override {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"FloatLitExpr\",\"value\":"
       << json_utils::escape(lit.getWord()) << "}";
  }
};

struct StrLitExpr : Expression {
  Token lit;

  explicit StrLitExpr(const Token &t) : lit(t) {}
  void toJson(std::ostream &os, int indent) const override {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"StrLitExpr\",\"value\":"
       << json_utils::escape(lit.getWord()) << "}";
  }
};

struct BoolLitExpr : Expression {
  Token lit;

  explicit BoolLitExpr(const Token &t) : lit(t) {}
  void toJson(std::ostream &os, int indent) const override {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"BoolLitExpr\",\"value\":"
       << json_utils::escape(lit.getWord()) << "}";
  }
};

struct CharLitExpr : Expression {
  Token lit;

  explicit CharLitExpr(const Token &t) : lit(t) {}
  void toJson(std::ostream &os, int indent) const override {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"CharLitExpr\",\"value\":"
       << json_utils::escape(lit.getWord()) << "}";
  }
};

// --------------------- //
// Identifier expression //
// --------------------- //
struct IdentExpr : Expression {
  Identifier name;

  explicit IdentExpr(const Identifier &n) : name(n) {}
  void toJson(std::ostream &os, int indent) const override {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"IdentExpr\",\"name\":"
       << json_utils::escape(name.token.getWord()) << "}";
  }
};

// ----------------------- //
// Arithmetic / comparison //
// ----------------------- //
struct BinaryExpr : Expression {
  BinaryOp op;
  ExprPtr left, right;

  BinaryExpr(BinaryOp o, ExprPtr l, ExprPtr r)
      : op(o), left(std::move(l)), right(std::move(r)) {}
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
  void toJson(std::ostream &os, int indent) const override {
    std::string p(indent, ' ');
    os << p
       << "{\"kind\":\"UnaryExpr\",\"op\":" << json_utils::escape(toString(op))
       << ",\"operand\":";
    operand->toJson(os, indent + 2);
    os << "}";
  }
};

// --------------- //
// Call expression //
// --------------- //
struct CallExpr : Expression {
  Identifier callee;

  std::vector<ExprPtr> arguments;
  CallExpr(const Identifier &c, std::vector<ExprPtr> args)
      : callee(c), arguments(std::move(args)) {}
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

// ------------------- //
// Assignment (scalar) //
// ------------------- //
struct AssignExpr : Expression {
  Identifier target;
  ExprPtr value;
  AssignKind kind;

  AssignExpr(Identifier tgt, ExprPtr val, AssignKind k = AssignKind::Copy)
      : target(std::move(tgt)), value(std::move(val)), kind(k) {}
  void toJson(std::ostream &os, int indent) const override {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"AssignExpr\",\"target\":"
       << json_utils::escape(target.token.getWord()) << ",\"value\":";
    value->toJson(os, indent + 2);
    os << "}";
  }
};

// --------------------- //
// Increment / Decrement //
// --------------------- //
struct Increment : Expression {
  Identifier target;

  explicit Increment(const Identifier &t) : target(t) {}
  void toJson(std::ostream &os, int indent) const override {
    os << std::string(indent, ' ') << "{\"kind\":\"Increment\",\"target\":"
       << json_utils::escape(target.token.getWord()) << "}";
  }
};

struct Decrement : Expression {
  Identifier target;

  explicit Decrement(const Identifier &t) : target(t) {}
  void toJson(std::ostream &os, int indent) const override {
    os << std::string(indent, ' ') << "{\"kind\":\"Decrement\",\"target\":"
       << json_utils::escape(target.token.getWord()) << "}";
  }
};

// ---------------- //
// Array allocation //
// ---------------- //
struct NewArrayExpr : Expression {
  TypeDesc arrayType;
  std::vector<ExprPtr> sizes;

  NewArrayExpr(TypeDesc t, std::vector<ExprPtr> sz)
      : arrayType(std::move(t)), sizes(std::move(sz)) {}
  void toJson(std::ostream &os, int indent) const override {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"NewArrayExpr\",\"type\":"
       << json_utils::escape(arrayType.fullName())
       << ",\"dims\":" << sizes.size() << "}";
  }
};

// -------------- //
// Array indexing //
// -------------- //
struct ArrayIndexExpr : Expression {
  Identifier array;
  std::vector<ExprPtr> indices;

  ArrayIndexExpr(Identifier arr, std::vector<ExprPtr> idxs)
      : array(std::move(arr)), indices(std::move(idxs)) {}

  void toJson(std::ostream &os, int indent) const override {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"ArrayIndexExpr\",\"array\":"
       << json_utils::escape(array.token.getWord()) << ",\"indices\":[";
    for (size_t i = 0; i < indices.size(); ++i) {
      if (i > 0)
        os << ",";
      indices[i]->toJson(os, indent + 2);
    }
    os << "]}";
  }
};

// ------------------------ //
// Array element assignment //
// ------------------------ //
struct ArrayIndexAssignExpr : Expression {
  Identifier array;
  std::vector<ExprPtr> indices;
  ExprPtr value;

  ArrayIndexAssignExpr(Identifier arr, std::vector<ExprPtr> idxs, ExprPtr val)
      : array(std::move(arr)), indices(std::move(idxs)), value(std::move(val)) {
  }

  void toJson(std::ostream &os, int indent) const override {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"ArrayIndexAssignExpr\",\"array\":"
       << json_utils::escape(array.token.getWord()) << ",\"indices\":[";
    for (size_t i = 0; i < indices.size(); ++i) {
      if (i > 0)
        os << ",";
      indices[i]->toJson(os, indent + 2);
    }
    os << "]}";
  }
};

// ---------------- //
// Property access: //
// ---------------- //
struct LengthPropertyExpr : Expression {
  Identifier name;

  explicit LengthPropertyExpr(const Identifier &n) : name(n) {}
  void toJson(std::ostream &os, int indent) const override {
    os << std::string(indent, ' ')
       << "{\"kind\":\"LengthPropertyExpr\",\"name\":"
       << json_utils::escape(name.token.getWord()) << "}";
  }
};

// -------------- //
// Statement base //
// -------------- //
struct Statement {
  virtual ~Statement() = default;
  virtual void toJson(std::ostream &os, int indent = 0) const = 0;
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
  void toJson(std::ostream &os, int indent) const override {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"WhileStmt\",\"cond\":";
    condition->toJson(os, indent + 2);
    os << "}";
  }
};

struct Return : Statement {
  std::optional<ExprPtr> value;

  Return() = default;
  explicit Return(ExprPtr v) : value(std::move(v)) {}
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

struct ExprStmt : Statement {
  ExprPtr expr;

  ExprStmt() = default;
  explicit ExprStmt(ExprPtr e) : expr(std::move(e)) {}
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

  Function(Identifier n, std::vector<Parameter> p, std::unique_ptr<Block> b,
           TypeDesc ret)
      : name(std::move(n)), params(std::move(p)), body(std::move(b)),
        returnType(std::move(ret)) {}

  void toJson(std::ostream &os, int indent = 0) const {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"Function\","
       << "\"name\":" << json_utils::escape(name.token.getWord()) << ","
       << "\"return\":" << json_utils::escape(returnType.fullName()) << "}";
  }
};

struct Program {
  std::vector<std::unique_ptr<Function>> functions;

  Program() = default;
  void toJson(std::ostream &os = std::cout, int indent = 0) const {
    std::string p(indent, ' ');
    os << p << "{\"kind\":\"Program\",\"functions\":[";
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
}

#endif
