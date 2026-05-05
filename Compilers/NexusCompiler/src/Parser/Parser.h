#ifndef PARSER_H
#define PARSER_H

#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>

// Custom
#include "../AST/AST.h"
#include "../Token/TokenType.h"

class Parser {
private:
  std::vector<Token> tokens;
  size_t currentIndex = 0;
  const Token &peek() const;
  const Token &peekAt(size_t offset) const;
  const Token consume();
  bool match(TokenKind kind);
  bool check(TokenKind kind) const;
  Token expect(TokenKind kind, std::string_view errorMsg = {});
  bool isAtEnd() const;

protected:
  void synchronize();

public:
  explicit Parser(const std::vector<Token> &t) : tokens(t) {}
  std::unique_ptr<Program> parse();
  std::unique_ptr<ImportDecl> parseImportDecl();
  std::unique_ptr<GlobalVarDecl> parseGlobalVarDecl();
  bool isIdentWord(std::string_view word) const;
  ExternBlock parseExternBlock();
  std::unique_ptr<StructDecl> parseStructDecl();
  std::unique_ptr<VarDecl> parseVarDeclNoInit();
  std::unique_ptr<Function> parseFunctionDecl();
  std::unique_ptr<Block> parseBlock(bool uni = false);
  std::unique_ptr<Statement> parseStatement();
  std::unique_ptr<Expression> parseExpression();
  std::unique_ptr<VarDecl> parseVarDeclStatement(AssignKind kind);
  std::unique_ptr<IfStmt> parseIfStatement();
  std::unique_ptr<WhileStmt> parseWhileLoop();
  std::unique_ptr<WhileStmt> parseLoop();
  std::unique_ptr<ForRangeStmt> parseForLoop();
  std::unique_ptr<Return> parseReturnStatement();
  std::unique_ptr<Statement> parseLoopBreak();
  std::unique_ptr<Expression> parsePrimary();
  std::unique_ptr<Expression> parseNewArray();
  std::unique_ptr<Statement> parseArrayAssign();
  std::unique_ptr<Expression> parseAssignment();
  std::unique_ptr<Expression> parseOr();
  std::unique_ptr<Expression> parseAnd();
  std::unique_ptr<Expression> parseEquality();
  std::unique_ptr<Expression> parseComparison();
  std::unique_ptr<Expression> parseAdditive();
  std::unique_ptr<Expression> parseMultiplicative();
  std::unique_ptr<Expression> parseUnary();
  std::unique_ptr<Expression> parsePostfix();
};

#endif
