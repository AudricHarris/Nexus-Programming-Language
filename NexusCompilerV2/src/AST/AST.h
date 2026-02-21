#ifndef AST_H
#define AST_H

#include "../Dictionary/TokenType.h"
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

// ─────────────────────────────────────────────
// JSON string escaping helper
// ─────────────────────────────────────────────
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

// ─────────────────────────────────────────────
// Forward declarations (minimal)
// ─────────────────────────────────────────────
struct Identifier;
struct IntegerLiteral;
struct StringLiteral;
struct Parameter;
struct Block;
struct Function;
struct Expression;
struct Statement;

using ExprPtr = std::unique_ptr<Expression>;

// ─────────────────────────────────────────────
// Basic value types
// ─────────────────────────────────────────────

struct Identifier {
  Token token;
  explicit Identifier(const Token &t) : token(t) {}
};

struct IntegerLiteral {
  Token token;
  explicit IntegerLiteral(const Token &t) : token(t) {}
};

struct StringLiteral {
  Token token;
  explicit StringLiteral(const Token &t) : token(t) {}
};

struct Parameter {
  Identifier type;
  Identifier name;
  Parameter(const Identifier &t, const Identifier &n) : type(t), name(n) {}
  Parameter(Identifier &&t, Identifier &&n)
      : type(std::move(t)), name(std::move(n)) {}
};

// ─────────────────────────────────────────────
// EXPRESSIONS — base + all derived classes
// ─────────────────────────────────────────────

struct Expression {
  virtual ~Expression() = default;
  virtual void toJson(std::ostream &os, int indent = 0) const = 0;
};

struct Assignment : Expression {
  Identifier target;
  ExprPtr value;
  Assignment(const Identifier &tgt, ExprPtr val)
      : target(tgt), value(std::move(val)) {}

  void toJson(std::ostream &os, int indent) const override {
    std::string pad(indent, ' ');
    os << pad << "{\n";
    os << pad << "  \"kind\": \"Assignment\",\n";
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

struct AssignExpr : Expression {
  Identifier target;
  ExprPtr value;
  AssignExpr(Identifier tgt, ExprPtr val)
      : target(std::move(tgt)), value(std::move(val)) {}

  void toJson(std::ostream &os, int indent) const override {
    std::string pad(indent, ' ');
    os << pad << "{\n";
    os << pad << "  \"kind\": \"AssignExpr\",\n";
    os << pad << "  \"target\": " << json_utils::escape(target.token.getWord())
       << ",\n";
    os << pad << "  \"value\": ";
    value->toJson(os, indent + 2);
    os << "\n" << pad << "}";
  }
};

// ─────────────────────────────────────────────
// STATEMENTS — base + derived classes
// ─────────────────────────────────────────────

struct Statement {
  virtual ~Statement() = default;
  virtual void toJson(std::ostream &os, int indent = 0) const = 0;
};

struct VarDecl : Statement {
  Identifier type;
  Identifier name;
  ExprPtr initializer;
  VarDecl(const Identifier &t, const Identifier &n, ExprPtr init)
      : type(t), name(n), initializer(std::move(init)) {}

  void toJson(std::ostream &os, int indent) const override {
    std::string pad(indent, ' ');
    os << pad << "{\n";
    os << pad << "  \"kind\": \"VarDecl\",\n";
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

// ─────────────────────────────────────────────
// Block — comes after Statement
// ─────────────────────────────────────────────

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

// ─────────────────────────────────────────────
// Function — comes after Block
// ─────────────────────────────────────────────

struct Function {
  Identifier name;
  std::vector<Parameter> params;
  std::unique_ptr<Block> body;

  Function(const Identifier &n, std::vector<Parameter> p,
           std::unique_ptr<Block> b)
      : name(n), params(std::move(p)), body(std::move(b)) {}

  Function(Identifier &&n, std::vector<Parameter> &&p,
           std::unique_ptr<Block> &&b)
      : name(std::move(n)), params(std::move(p)), body(std::move(b)) {}

  void toJson(std::ostream &os, int indent = 0) const {
    std::string pad(indent, ' ');
    os << pad << "{\n";
    os << pad << "  \"kind\": \"Function\",\n";
    os << pad << "  \"name\": " << json_utils::escape(name.token.getWord())
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
    if (body) {
      body->toJson(os, indent + 2);
    } else {
      os << "null";
    }
    os << "\n" << pad << "}";
  }
};

// ─────────────────────────────────────────────
// Program — at the end
// ─────────────────────────────────────────────

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
