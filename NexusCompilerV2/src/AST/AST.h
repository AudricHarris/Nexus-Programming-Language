#ifndef AST_H
#define AST_H

#include <iostream>
#include <memory>
#include <string>
#include <vector>
enum class NodeType {
  Program,
  FunctionDef,
  VarDecl,
  Assignement,
  Incremement,
  CallExpr,
  ReturnStmt,
  Identifier,
  IntegerLiteral,
  StringLiteral,
};
/*
 *
 Parsing should give us this :
Main() -> NodeType::FunctionDef
{
        i32 num = 5008526; --> NodeType::VarDecl  + NodeType::IntegerLiteral
        num++; --> NodeType::Increment
        Printf("This is a test {num}"); --> NodeType::CallExpr +
NodeType::StringLiteral

        i32 num2 = num; --> NodeType::Assignement
        return; --> NodeType::ReturnStmt
}
*/

class ASTVisitor;

class ASTNode {
public:
  virtual ~ASTNode() = default;
  virtual void accept(ASTVisitor &v) const = 0;
  virtual void print(int indent = 0) const = 0;
};

class Decl : public ASTNode {};

class Program : public ASTNode {
public:
  std::vector<std::unique_ptr<Decl>> declarations;
  void accept(ASTVisitor &v) const override {}
  void print(int indent) const override {
    std::cout << std::string(indent, ' ') << "Program\n";
    for (const auto &decl : declarations) {
      decl->print(indent + 2);
    }
  }
};

class Block : public ASTNode {
public:
  std::vector<std::unique_ptr<Decl>> declarations;
  void accept(ASTVisitor &v) const override {}
  void print(int indent) const override {
    std::cout << std::string(indent, ' ') << "Program\n";
    for (const auto &decl : declarations) {
      decl->print(indent + 2);
    }
  }
};

#endif // !AST_H
