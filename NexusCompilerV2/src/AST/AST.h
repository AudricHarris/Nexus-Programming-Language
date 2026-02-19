#ifndef AST_H
#define AST_H
#include "../Dictionary/TokenType.h"
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

struct Identifier {
  Token token;

  explicit Identifier(const Token &t) // most common case
      : token(t) {}
};

struct IntegerLiteral {
  Token token;

  explicit IntegerLiteral(const Token &t) : token(t) {}
};

struct StringLiteral {
  Token token;

  explicit StringLiteral(const Token &t) : token(t) {}
};

struct Expression;
using ExprPtr = std::unique_ptr<Expression>;

struct Block {
  std::vector<std::unique_ptr<struct Statement>> statements;

  Block() = default;
  explicit Block(std::vector<std::unique_ptr<Statement>> stmts)
      : statements(std::move(stmts)) {}
};

struct Parameter {
  Identifier type;
  Identifier name;

  Parameter(const Identifier &t, const Identifier &n) : type(t), name(n) {}
  Parameter(Identifier &&t, Identifier &&n)
      : type(std::move(t)), name(std::move(n)) {}
};

struct Function {
  Identifier name;
  std::vector<Parameter> params;
  std::unique_ptr<Block> body;

  Function(const Identifier &n, std::vector<Parameter> p,
           std::unique_ptr<Block> b)
      : name(n), params(std::move(p)), body(std::move(b)) {}

  // move version — useful when constructing with temporaries
  Function(Identifier &&n, std::vector<Parameter> &&p,
           std::unique_ptr<Block> &&b)
      : name(std::move(n)), params(std::move(p)), body(std::move(b)) {}
};

// -------------------------- //
// Statements //
// -------------------------- //

struct Statement {
  virtual ~Statement() = default;
};

struct VarDecl : Statement {
  Identifier type;
  Identifier name;
  ExprPtr initializer; // fixed typo: intializer → initializer

  VarDecl(const Identifier &t, const Identifier &n, ExprPtr init)
      : type(t), name(n), initializer(std::move(init)) {}
};

struct Assignment : Statement {
  Identifier target;
  ExprPtr value;

  Assignment(const Identifier &tgt, ExprPtr val)
      : target(tgt), value(std::move(val)) {}
};

struct Increment : Statement {
  Identifier target;

  explicit Increment(const Identifier &tgt) : target(tgt) {}
};

struct Return : Statement {
  std::optional<ExprPtr> value;

  Return() = default;
  explicit Return(ExprPtr v) : value(std::move(v)) {}
};

struct ExprStmt : Statement {
  ExprPtr expr;

  ExprStmt() = default;
  explicit ExprStmt(ExprPtr e) : expr(std::move(e)) {}
};

// -------------------------- //
// Expressions //
// -------------------------- //

struct Expression {
  virtual ~Expression() = default;
};

struct IdentExpr : Expression {
  Identifier name;

  explicit IdentExpr(const Identifier &n) : name(n) {}
};

struct IntLitExpr : Expression {
  IntegerLiteral lit;

  explicit IntLitExpr(const IntegerLiteral &l) : lit(l) {}
};

struct StrLitExpr : Expression {
  StringLiteral lit;

  explicit StrLitExpr(const StringLiteral &l) : lit(l) {}
};

struct CallExpr : Expression {
  Identifier callee;
  std::vector<ExprPtr> arguments;

  CallExpr(const Identifier &c, std::vector<ExprPtr> args)
      : callee(c), arguments(std::move(args)) {}
};

struct Program {
  std::vector<std::unique_ptr<Function>> functions;

  Program() = default;
};

#endif
