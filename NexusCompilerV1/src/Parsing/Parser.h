#ifndef PARSER_H
#define PARSER_H

#include "../Tokenizing/Token.h"
#include "AST.h"
#include <memory>
#include <stdexcept>
#include <vector>

class Parser {
private:
  std::vector<Token> tokens;
  size_t currentPos = 0;

  // Helper methods
  Token peek() const;
  Token advance();
  bool match(TokenKind kind);
  bool check(TokenKind kind) const;
  Token consume(TokenKind kind, const std::string &errorMsg);
  bool isAtEnd() const;

public:
  explicit Parser(const std::vector<Token> &tokens);

  // Main parsing entry point
  std::unique_ptr<Program> parse();

  // Parsing methods for different constructs
  std::unique_ptr<Decl> parseDeclaration();
  std::unique_ptr<ClassDecl> parseClassDecl();
  std::unique_ptr<MethodDecl> parseMethodDecl();
  std::unique_ptr<VarDecl> parseVarDecl();
  std::unique_ptr<SumTypeDecl> parseSumTypeDecl();

  std::unique_ptr<Stmt> parseStatement();
  std::unique_ptr<Stmt> parseBlockStmt();
  std::unique_ptr<Stmt> parseIfStmt();
  std::unique_ptr<Stmt> parseWhileStmt();
  std::unique_ptr<Stmt> parseReturnStmt();
  std::unique_ptr<Stmt> parseExprStmt();

  std::unique_ptr<Expr> parseExpression();
  std::unique_ptr<Expr> parseAssignment();
  std::unique_ptr<Expr> parseLogical();
  std::unique_ptr<Expr> parseEquality();
  std::unique_ptr<Expr> parseComparison();
  std::unique_ptr<Expr> parseTerm();
  std::unique_ptr<Expr> parseFactor();
  std::unique_ptr<Expr> parseUnary();
  std::unique_ptr<Expr> parseCall();
  std::unique_ptr<Expr> parsePrimary();
  std::unique_ptr<Expr> parseMatchExpr();

  Type parseType();
};

class ParseError : public std::runtime_error {
public:
  explicit ParseError(const std::string &msg) : std::runtime_error(msg) {}
};

#endif
