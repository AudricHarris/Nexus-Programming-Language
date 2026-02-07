#ifndef AST_H
#define AST_H

// Includes
#include "../Tokenizing/Token.h"
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

// Forward declarations
class ASTVisitor;

struct Type {
  std::string name;
  std::vector<Type> generics;
  bool isArray = false;
};

struct Param {
  std::string name;
  Type type;
};

using ParamList = std::vector<Param>;

class Expr;
class Stmt;

using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;

class ASTNode {
public:
  virtual ~ASTNode() = default;
  virtual void accept(ASTVisitor &v) const = 0;
  virtual void print(int indent = 0) const = 0;
};

class Expr : public ASTNode {
public:
  Type type;
};

class LiteralExpr : public Expr {
public:
  Token token;
  std::variant<int64_t, double, std::string, bool> value;

  LiteralExpr(Token t);
  void accept(ASTVisitor &v) const override { /* TODO */ }
  void print(int indent) const override {
    std::cout << std::string(indent, ' ')
              << "LiteralExpr: " << token.getLexeme() << "\n";
  }
};

class IdentifierExpr : public Expr {
public:
  std::string name;
  IdentifierExpr(std::string n, const Token &tok) : name(std::move(n)) {}
  void accept(ASTVisitor &v) const override { /* TODO */ }
  void print(int indent) const override {
    std::cout << std::string(indent, ' ') << "IdentifierExpr: " << name << "\n";
  }
};

class BinaryExpr : public Expr {
public:
  ExprPtr left;
  Token op;
  ExprPtr right;
  BinaryExpr(ExprPtr l, Token o, ExprPtr r)
      : left(std::move(l)), op(std::move(o)), right(std::move(r)) {}
  void accept(ASTVisitor &v) const override { /* TODO */ }
  void print(int indent) const override {
    std::cout << std::string(indent, ' ') << "BinaryExpr: " << op.getLexeme()
              << "\n";
    if (left)
      left->print(indent + 2);
    if (right)
      right->print(indent + 2);
  }
};

class MemberAccessExpr : public Expr {
public:
  ExprPtr object;
  std::string member;
  MemberAccessExpr(ExprPtr obj, std::string m)
      : object(std::move(obj)), member(std::move(m)) {}
  void accept(ASTVisitor &v) const override { /* TODO */ }
  void print(int indent) const override {
    std::cout << std::string(indent, ' ') << "MemberAccessExpr: " << member
              << "\n";
    if (object)
      object->print(indent + 2);
  }
};

class CallExpr : public Expr {
public:
  ExprPtr callee;
  std::vector<ExprPtr> args;
  CallExpr(ExprPtr c, std::vector<ExprPtr> a)
      : callee(std::move(c)), args(std::move(a)) {}
  void accept(ASTVisitor &v) const override { /* TODO */ }
  void print(int indent) const override {
    std::cout << std::string(indent, ' ') << "CallExpr\n";
    if (callee)
      callee->print(indent + 2);
    for (const auto &arg : args) {
      arg->print(indent + 2);
    }
  }
};

class NewExpr : public Expr {
public:
  Type type;
  std::vector<ExprPtr> args;
  NewExpr(Type t, std::vector<ExprPtr> a)
      : type(std::move(t)), args(std::move(a)) {}
  void accept(ASTVisitor &v) const override { /* TODO */ }
  void print(int indent) const override {
    std::cout << std::string(indent, ' ') << "NewExpr: " << type.name << "\n";
    for (const auto &arg : args) {
      arg->print(indent + 2);
    }
  }
};

class MatchExpr : public Expr {
public:
  ExprPtr scrutinee;
  struct Case {
    std::string variantName;
    std::optional<std::string> bindName;
    StmtPtr body;
  };
  std::vector<Case> cases;
  MatchExpr(ExprPtr s, std::vector<Case> cs)
      : scrutinee(std::move(s)), cases(std::move(cs)) {}
  void accept(ASTVisitor &v) const override { /* TODO */ }
  void print(int indent) const override {
    std::cout << std::string(indent, ' ') << "MatchExpr\n";
    if (scrutinee)
      scrutinee->print(indent + 2);
  }
};

class Stmt : public ASTNode {};

class ExprStmt : public Stmt {
public:
  ExprPtr expr;
  void accept(ASTVisitor &v) const override { /* TODO */ }
  void print(int indent) const override {
    std::cout << std::string(indent, ' ') << "ExprStmt\n";
    if (expr)
      expr->print(indent + 2);
  }
};

class ReturnStmt : public Stmt {
public:
  std::optional<ExprPtr> value;
  void accept(ASTVisitor &v) const override { /* TODO */ }
  void print(int indent) const override {
    std::cout << std::string(indent, ' ') << "ReturnStmt\n";
    if (value && *value)
      (*value)->print(indent + 2);
  }
};

class IfStmt : public Stmt {
public:
  ExprPtr condition;
  StmtPtr thenBranch;
  std::optional<StmtPtr> elseBranch;
  void accept(ASTVisitor &v) const override { /* TODO */ }
  void print(int indent) const override {
    std::cout << std::string(indent, ' ') << "IfStmt\n";
    if (condition)
      condition->print(indent + 2);
    if (thenBranch)
      thenBranch->print(indent + 2);
    if (elseBranch && *elseBranch)
      (*elseBranch)->print(indent + 2);
  }
};

class WhileStmt : public Stmt {
public:
  ExprPtr condition;
  StmtPtr body;
  void accept(ASTVisitor &v) const override { /* TODO */ }
  void print(int indent) const override {
    std::cout << std::string(indent, ' ') << "WhileStmt\n";
    if (condition)
      condition->print(indent + 2);
    if (body)
      body->print(indent + 2);
  }
};

class BlockStmt : public Stmt {
public:
  std::vector<StmtPtr> statements;
  void accept(ASTVisitor &v) const override { /* TODO */ }
  void print(int indent) const override {
    std::cout << std::string(indent, ' ') << "BlockStmt\n";
    for (const auto &stmt : statements) {
      stmt->print(indent + 2);
    }
  }
};

class Decl : public ASTNode {};

class VarDecl : public Decl {
public:
  bool isGlobal = false;
  std::string name;
  std::optional<Type> type;
  ExprPtr initializer;
  void accept(ASTVisitor &v) const override { /* TODO */ }
  void print(int indent) const override {
    std::cout << std::string(indent, ' ') << "VarDecl: " << name;
    if (type)
      std::cout << " : " << type->name;
    std::cout << "\n";
    if (initializer)
      initializer->print(indent + 2);
  }
};

class SumTypeDecl : public Decl {
public:
  std::string name;
  std::vector<std::string> generics;
  struct Variant {
    std::string name;
    std::optional<Type> payload;
  };
  std::vector<Variant> variants;
  void accept(ASTVisitor &v) const override { /* TODO */ }
  void print(int indent) const override {
    std::cout << std::string(indent, ' ') << "SumTypeDecl: " << name << "\n";
    for (const auto &variant : variants) {
      std::cout << std::string(indent + 2, ' ') << "Variant: " << variant.name
                << "\n";
    }
  }
};

class MethodDecl : public Decl {
public:
  bool isPublic = true;
  bool isConstructor = false;
  std::string name;
  ParamList params;
  Type returnType;
  StmtPtr body;
  void accept(ASTVisitor &v) const override { /* TODO */ }
  void print(int indent) const override {
    std::cout << std::string(indent, ' ') << "MethodDecl: " << name << "\n";
    for (const auto &param : params) {
      std::cout << std::string(indent + 2, ' ') << "Param: " << param.name
                << " : " << param.type.name << "\n";
    }
    if (body)
      body->print(indent + 2);
  }
};

class ClassDecl : public Decl {
public:
  bool isPublic = true;
  std::string name;
  std::vector<std::unique_ptr<VarDecl>> fields;
  std::vector<std::unique_ptr<MethodDecl>> methods;
  void accept(ASTVisitor &v) const override { /* TODO */ }
  void print(int indent) const override {
    std::cout << std::string(indent, ' ') << "ClassDecl: " << name << "\n";
    for (const auto &field : fields) {
      field->print(indent + 2);
    }
    for (const auto &method : methods) {
      method->print(indent + 2);
    }
  }
};

class Program : public ASTNode {
public:
  std::vector<std::unique_ptr<Decl>> declarations;
  void accept(ASTVisitor &v) const override { /* TODO */ }
  void print(int indent) const override {
    std::cout << std::string(indent, ' ') << "Program\n";
    for (const auto &decl : declarations) {
      decl->print(indent + 2);
    }
  }
};

#endif
