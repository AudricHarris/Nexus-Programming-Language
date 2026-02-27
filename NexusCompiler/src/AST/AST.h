#ifndef AST_H
#define AST_H

#include "../Dictionary/TokenType.h"
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

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

struct Identifier;
struct IntegerLiteral;
struct FloatLiteral;
struct StringLiteral;
struct BoolLiteral;
struct Parameter;
struct Block;
struct Function;
struct Expression;
struct Statement;

using ExprPtr = std::unique_ptr<Expression>;

struct Identifier {
  Token token;
  explicit Identifier(const Token &t) : token(t) {}
};

struct IntegerLiteral {
  Token token;
  explicit IntegerLiteral(const Token &t) : token(t) {}
};

struct FloatLiteral {
  Token token;
  explicit FloatLiteral(const Token &t) : token(t) {}
};

struct StringLiteral {
  Token token;
  explicit StringLiteral(const Token &t) : token(t) {}
};

struct BoolLiteral {
  Token token;
  explicit BoolLiteral(const Token &t) : token(t) {}
};

struct Parameter {
  Identifier type;
  Identifier name;
  Parameter(const Identifier &t, const Identifier &n) : type(t), name(n) {}
  Parameter(Identifier &&t, Identifier &&n)
      : type(std::move(t)), name(std::move(n)) {}
};

struct Expression {
  virtual ~Expression() = default;
  virtual void toJson(std::ostream &os, int indent = 0) const = 0;
};

// ---------------------------
//     Binary / Unary Ops
// ---------------------------

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
  Ge
};

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
  }
  return "Unknown";
}

enum class UnaryOp { Negate };

inline std::string toString(UnaryOp op) {
  switch (op) {
  case UnaryOp::Negate:
    return "Negate";
  }
  return "Unknown";
}

// ---------------------------
//     Assignment kinds
// ---------------------------

// Copy  : T x = value   (or x = value for reassignment)
// Move  : T x <- value  (transfers ownership; source invalidated)
// Borrow: T x &= value  (reference borrow; no ownership transfer)
enum class AssignKind { Copy, Move, Borrow };

// ---------------------------
//     Expression nodes
// ---------------------------

struct IdentExpr : Expression {
  Identifier name;
  explicit IdentExpr(const Identifier &n) : name(n) {}

  void toJson(std::ostream &os, int indent) const override {
    std::string pad(indent, ' ');
    os << pad << "{\n";
    os << pad << "  \"kind\": \"IdentExpr\",\n";
    os << pad << "  \"name\": " << json_utils::escape(name.token.getWord())
       << "\n";
    os << pad << "}";
  }
};

struct IntLitExpr : Expression {
  IntegerLiteral lit;
  explicit IntLitExpr(const IntegerLiteral &l) : lit(l) {}

  void toJson(std::ostream &os, int indent) const override {
    std::string pad(indent, ' ');
    os << pad << "{\n";
    os << pad << "  \"kind\": \"IntLitExpr\",\n";
    os << pad << "  \"value\": " << json_utils::escape(lit.token.getWord())
       << "\n";
    os << pad << "}";
  }
};

struct FloatLitExpr : Expression {
  FloatLiteral lit;
  explicit FloatLitExpr(const FloatLiteral &l) : lit(l) {}

  void toJson(std::ostream &os, int indent) const override {
    std::string pad(indent, ' ');
    os << pad << "{\n";
    os << pad << "  \"kind\": \"FloatLitExpr\",\n";
    os << pad << "  \"value\": " << json_utils::escape(lit.token.getWord())
       << "\n";
    os << pad << "}";
  }
};

struct StrLitExpr : Expression {
  StringLiteral lit;
  explicit StrLitExpr(const StringLiteral &l) : lit(l) {}

  void toJson(std::ostream &os, int indent) const override {
    std::string pad(indent, ' ');
    os << pad << "{\n";
    os << pad << "  \"kind\": \"StrLitExpr\",\n";
    os << pad << "  \"value\": " << json_utils::escape(lit.token.getWord())
       << "\n";
    os << pad << "}";
  }
};

struct BoolLitExpr : Expression {
  BoolLiteral lit;
  explicit BoolLitExpr(const BoolLiteral &l) : lit(l) {}

  void toJson(std::ostream &os, int indent) const override {
    std::string pad(indent, ' ');
    os << pad << "{\n";
    os << pad << "  \"kind\": \"BoolLitExpr\",\n";
    os << pad << "  \"value\": " << json_utils::escape(lit.token.getWord())
       << "\n";
    os << pad << "}";
  }
};

struct BinaryExpr : Expression {
  BinaryOp op;
  ExprPtr left;
  ExprPtr right;

  BinaryExpr(BinaryOp o, ExprPtr l, ExprPtr r)
      : op(o), left(std::move(l)), right(std::move(r)) {}

  void toJson(std::ostream &os, int indent) const override {
    std::string pad(indent, ' ');
    os << pad << "{\n";
    os << pad << "  \"kind\": \"BinaryExpr\",\n";
    os << pad << "  \"operator\": " << json_utils::escape(toString(op))
       << ",\n";
    os << pad << "  \"left\": ";
    left->toJson(os, indent + 2);
    os << ",\n";
    os << pad << "  \"right\": ";
    right->toJson(os, indent + 2);
    os << "\n" << pad << "}";
  }
};

struct UnaryExpr : Expression {
  UnaryOp op;
  ExprPtr operand;

  UnaryExpr(UnaryOp o, ExprPtr expr) : op(o), operand(std::move(expr)) {}

  void toJson(std::ostream &os, int indent) const override {
    std::string pad(indent, ' ');
    os << pad << "{\n";
    os << pad << "  \"kind\": \"UnaryExpr\",\n";
    os << pad << "  \"operator\": " << json_utils::escape(toString(op))
       << ",\n";
    os << pad << "  \"operand\": ";
    operand->toJson(os, indent + 2);
    os << "\n" << pad << "}";
  }
};

struct CallExpr : Expression {
  Identifier callee;
  std::vector<ExprPtr> arguments;
  CallExpr(const Identifier &c, std::vector<ExprPtr> args)
      : callee(c), arguments(std::move(args)) {}

  void toJson(std::ostream &os, int indent) const override {
    std::string pad(indent, ' ');
    os << pad << "{\n";
    os << pad << "  \"kind\": \"CallExpr\",\n";
    os << pad << "  \"callee\": " << json_utils::escape(callee.token.getWord())
       << ",\n";
    os << pad << "  \"arguments\": [\n";
    for (size_t i = 0; i < arguments.size(); ++i) {
      if (i > 0)
        os << ",\n";
      arguments[i]->toJson(os, indent + 4);
    }
    os << "\n" << pad << "  ]\n";
    os << pad << "}";
  }
};

// Copy assignment expression (reassignment, not declaration): x = expr
struct AssignExpr : Expression {
  Identifier target;
  ExprPtr value;
  AssignKind kind; // Copy | Move | Borrow

  AssignExpr(Identifier tgt, ExprPtr val, AssignKind k = AssignKind::Copy)
      : target(std::move(tgt)), value(std::move(val)), kind(k) {}

  void toJson(std::ostream &os, int indent) const override {
    std::string pad(indent, ' ');
    os << pad << "{\n";
    os << pad << "  \"kind\": \"AssignExpr\",\n";
    os << pad << "  \"assignKind\": \""
       << (kind == AssignKind::Copy   ? "Copy"
           : kind == AssignKind::Move ? "Move"
                                      : "Borrow")
       << "\",\n";
    os << pad << "  \"target\": " << json_utils::escape(target.token.getWord())
       << ",\n";
    os << pad << "  \"value\": ";
    value->toJson(os, indent + 2);
    os << "\n" << pad << "}";
  }
};

struct Increment : Expression {
  Identifier target;
  explicit Increment(const Identifier &tgt) : target(tgt) {}

  void toJson(std::ostream &os, int indent) const override {
    std::string pad(indent, ' ');
    os << pad << "{\n";
    os << pad << "  \"kind\": \"Increment\",\n";
    os << pad << "  \"target\": " << json_utils::escape(target.token.getWord())
       << "\n";
    os << pad << "}";
  }
};

struct Decrement : Expression {
  Identifier target;
  explicit Decrement(const Identifier &tgt) : target(tgt) {}

  void toJson(std::ostream &os, int indent) const override {
    std::string pad(indent, ' ');
    os << pad << "{\n";
    os << pad << "  \"kind\": \"Decrement\",\n";
    os << pad << "  \"target\": " << json_utils::escape(target.token.getWord())
       << "\n";
    os << pad << "}";
  }
};

// ---------------------------
//     Statement nodes
// ---------------------------

struct Statement {
  virtual ~Statement() = default;
  virtual void toJson(std::ostream &os, int indent = 0) const = 0;
};

// Variable declaration: T name = expr  /  T name <- expr  /  T name &= expr
struct VarDecl : Statement {
  Identifier type;
  Identifier name;
  ExprPtr initializer;
  AssignKind kind; // Copy | Move | Borrow
  bool isMove;

  VarDecl(const Identifier &t, const Identifier &n, ExprPtr init,
          AssignKind k = AssignKind::Copy, bool moved = false)
      : type(t), name(n), initializer(std::move(init)), kind(k), isMove(moved) {
  }

  void toJson(std::ostream &os, int indent) const override {
    std::string pad(indent, ' ');
    os << pad << "{\n";
    os << pad << "  \"kind\": \"VarDecl\",\n";
    os << pad << "  \"assignKind\": \""
       << (kind == AssignKind::Copy   ? "Copy"
           : kind == AssignKind::Move ? "Move"
                                      : "Borrow")
       << "\",\n";
    os << pad << "  \"type\": " << json_utils::escape(type.token.getWord())
       << ",\n";
    os << pad << "  \"name\": " << json_utils::escape(name.token.getWord())
       << ",\n";
    os << pad << "  \"initializer\": ";
    if (initializer)
      initializer->toJson(os, indent + 2);
    else
      os << "null";
    os << "\n" << pad << "}";
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

  void toJson(std::ostream &os, int indent = 0) const override {
    std::string pad(indent, ' ');
    os << pad << "{\n";
    os << pad << "  \"kind\": \"IfStmt\",\n";
    os << pad << "  \"condition\": ";
    condition->toJson(os, indent + 4);
    os << ",\n";
    os << pad << "  \"then\": ";
    // thenBranch->toJson(...) — omitted for brevity like original
    os << ",\n";
    os << pad << "  \"else\": null\n";
    os << pad << "}";
  }
};

struct WhileStmt : Statement {
  ExprPtr condition;
  std::unique_ptr<Block> doBranch;

  WhileStmt(ExprPtr cond, std::unique_ptr<Block> doB)
      : condition(std::move(cond)), doBranch(std::move(doB)) {}

  void toJson(std::ostream &os, int indent = 0) const override {
    std::string pad(indent, ' ');
    os << pad << "{\n";
    os << pad << "  \"kind\": \"WhileStmt\",\n";
    os << pad << "  \"condition\": ";
    condition->toJson(os, indent + 4);
    os << "\n" << pad << "}";
  }
};

struct Return : Statement {
  std::optional<ExprPtr> value;
  Return() = default;
  explicit Return(ExprPtr v) : value(std::move(v)) {}

  void toJson(std::ostream &os, int indent) const override {
    std::string pad(indent, ' ');
    os << pad << "{\n";
    os << pad << "  \"kind\": \"Return\",\n";
    os << pad << "  \"value\": ";
    if (value)
      (*value)->toJson(os, indent + 2);
    else
      os << "null";
    os << "\n" << pad << "}";
  }
};

struct ExprStmt : Statement {
  ExprPtr expr;
  ExprStmt() = default;
  explicit ExprStmt(ExprPtr e) : expr(std::move(e)) {}

  void toJson(std::ostream &os, int indent) const override {
    std::string pad(indent, ' ');
    os << pad << "{\n";
    os << pad << "  \"kind\": \"ExprStmt\",\n";
    os << pad << "  \"expr\": ";
    if (expr)
      expr->toJson(os, indent + 2);
    else
      os << "null";
    os << "\n" << pad << "}";
  }
};

// ---------------------------
//     Block / Function / Program
// ---------------------------

struct Block {
  std::vector<std::unique_ptr<Statement>> statements;
  Block() = default;
  explicit Block(std::vector<std::unique_ptr<Statement>> stmts)
      : statements(std::move(stmts)) {}

  void toJson(std::ostream &os, int indent = 0) const {
    std::string pad(indent, ' ');
    os << pad << "{\n";
    os << pad << "  \"kind\": \"Block\",\n";
    os << pad << "  \"statements\": [\n";
    for (size_t i = 0; i < statements.size(); ++i) {
      if (i > 0)
        os << ",\n";
      statements[i]->toJson(os, indent + 4);
    }
    os << "\n" << pad << "  ]\n";
    os << pad << "}";
  }
};

struct Function {
  Identifier name;
  std::vector<Parameter> params;
  std::unique_ptr<Block> body;
  Identifier returnType;

  Function(const Identifier &n, std::vector<Parameter> p,
           std::unique_ptr<Block> b, Identifier type)
      : name(n), params(std::move(p)), body(std::move(b)), returnType(type) {}

  Function(Identifier &&n, std::vector<Parameter> &&p,
           std::unique_ptr<Block> &&b, Identifier type)
      : name(std::move(n)), params(std::move(p)), body(std::move(b)),
        returnType(type) {}

  void toJson(std::ostream &os, int indent = 0) const {
    std::string pad(indent, ' ');
    os << pad << "{\n";
    os << pad << "  \"kind\": \"Function\",\n";
    os << pad << "  \"name\": " << json_utils::escape(name.token.getWord())
       << ",\n";
    os << pad << "  \"Return\": " << json_utils::escape(name.token.getWord())
       << ",\n";
    os << pad << "  \"params\": [\n";
    for (size_t i = 0; i < params.size(); ++i) {
      if (i > 0)
        os << ",\n";
      os << pad << "    {\n";
      os << pad << "      \"type\": "
         << json_utils::escape(params[i].type.token.getWord()) << ",\n";
      os << pad << "      \"name\": "
         << json_utils::escape(params[i].name.token.getWord()) << "\n";
      os << pad << "    }";
    }
    os << "\n" << pad << "  ],\n";
    os << pad << "  \"body\": ";
    if (body)
      body->toJson(os, indent + 2);
    else
      os << "null";
    os << "\n" << pad << "}";
  }
};

struct Program {
  std::vector<std::unique_ptr<Function>> functions;
  Program() = default;

  void toJson(std::ostream &os = std::cout, int indent = 0) const {
    std::string pad(indent, ' ');
    os << pad << "{\n";
    os << pad << "  \"kind\": \"Program\",\n";
    os << pad << "  \"functions\": [\n";
    for (size_t i = 0; i < functions.size(); ++i) {
      if (i > 0)
        os << ",\n";
      functions[i]->toJson(os, indent + 4);
    }
    os << "\n" << pad << "  ]\n";
    os << pad << "}\n";
  }
};

#endif
