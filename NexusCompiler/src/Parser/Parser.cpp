#include "Parser.h"
#include "../Dictionary/TokenType.h"
#include "ParserError.h"

#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// -------------------------- //
//      Private methods       //
// -------------------------- //

const Token &Parser::peek() const {
  if (this->currentIndex >= this->tokens.size()) {
    static const Token EOF_TOKEN{TokenKind::TOK_EOF, "", 0, 0};
    return EOF_TOKEN;
  }
  return this->tokens[this->currentIndex];
}

const Token &Parser::peekAt(size_t offset) const {
  size_t idx = this->currentIndex + offset;
  if (idx >= this->tokens.size()) {
    static const Token EOF_TOKEN{TokenKind::TOK_EOF, "", 0, 0};
    return EOF_TOKEN;
  }
  return this->tokens[idx];
}

const Token Parser::consume() {
  if (this->currentIndex >= this->tokens.size()) {
    static const Token EOF_TOKEN{TokenKind::TOK_EOF, "", 0, 0};
    return EOF_TOKEN;
  }
  return this->tokens[this->currentIndex++];
}

bool Parser::match(TokenKind k) {
  if (this->peek().getKind() == k) {
    this->consume();
    return true;
  }
  return false;
}

bool Parser::check(TokenKind kind) const {
  return !this->isAtEnd() && peek().getKind() == kind;
}

Token Parser::expect(TokenKind kind, std::string_view errorMsg) {
  if (this->peek().getKind() == kind) {
    return this->consume();
  }

  std::string msg;
  Token tmp(kind, "", 0, 0);
  msg = "Expected : " + tmp.toString() + ", got : `" + this->peek().getWord() +
        "`";
  throw ParseError(this->peek().getLine(), this->peek().getColumn(),
                   errorMsg.empty() ? msg : std::string(errorMsg));
}

bool Parser::isAtEnd() const {
  return this->currentIndex >= this->tokens.size() ||
         this->peek().getKind() == TokenKind::TOK_EOF;
}

// -------------------------- //
//      protected methods     //
// -------------------------- //

void Parser::synchronize() {
  consume();
  while (!this->isAtEnd()) {
    if (this->peek().getKind() == TokenKind::TOK_SEMI) {
      consume();
      return;
    }
    switch (peek().getKind()) {
    case TokenKind::TOK_RETURN:
    case TokenKind::TOK_LBRACE:
      return;
    default:
      consume();
    }
  }
}

static bool looksLikeType(const Token &tok) {
  const std::string &w = tok.getWord();
  return w == "i32" || w == "i64" || w == "f32" || w == "f64" || w == "bool" ||
         w == "void" || w == "int" || w == "float" || w == "double" ||
         w == "long" || w == "integer" || w == "string" || w == "str";
}

// -------------------------- //
//      public methods        //
// -------------------------- //

std::unique_ptr<Program> Parser::parse() {
  auto prog = std::make_unique<Program>();

  while (!this->isAtEnd()) {
    try {
      auto func = this->parseFunctionDecl();
      prog->functions.push_back(std::move(func));
    } catch (const ParseError &e) {
      this->synchronize();
    }
  }

  return prog;
}

std::unique_ptr<Function> Parser::parseFunctionDecl() {
  Token nameToken = this->expect(TokenKind::TOK_IDENTIFIER,
                                 "Expected function name at top level");

  this->expect(TokenKind::TOK_LPAREN, "Expected '(' after function name");

  std::vector<Parameter> params;

  if (!this->match(TokenKind::TOK_RPAREN)) {
    do {
      Token typeToken =
          this->expect(TokenKind::TOK_IDENTIFIER, "Expected parameter type");
      Token nameTokenParam = this->expect(TokenKind::TOK_IDENTIFIER,
                                          "Expected parameter name after type");
      Parameter p{Identifier{typeToken}, Identifier{nameTokenParam}};
      params.push_back(std::move(p));
    } while (match(TokenKind::TOK_COMMA));

    this->expect(TokenKind::TOK_RPAREN, "Expected ')' after parameter list");
  }

  auto body = parseBlock();
  return std::make_unique<Function>(Identifier{nameToken}, std::move(params),
                                    std::move(body));
}

std::unique_ptr<Block> Parser::parseBlock(bool uni) {
  auto block = std::make_unique<Block>();
  if (this->check(TokenKind::TOK_LBRACE)) {
    this->expect(TokenKind::TOK_LBRACE, "Expected `{` at start of block");

    while (!this->check(TokenKind::TOK_RBRACE) && !isAtEnd()) {
      try {
        auto stmt = this->parseStatement();
        if (stmt) {
          block->statements.push_back(std::move(stmt));
        }
      } catch (const ParseError &e) {
        synchronize();
      }
    }

    this->expect(TokenKind::TOK_RBRACE, "Expected `}` to close the block");
  } else if (uni) {
    try {
      auto stmt = this->parseStatement();
      if (stmt) {
        block->statements.push_back(std::move(stmt));
      }
    } catch (const ParseError &e) {
      synchronize();
    }
  }

  return block;
}

std::unique_ptr<Statement> Parser::parseStatement() {
  if (this->match(TokenKind::TOK_RETURN)) {
    return this->parseReturnStatement();
  }
  if (this->match(TokenKind::TOK_IF)) {
    return this->parseIfStatement();
  }
  if (this->match(TokenKind::TOK_WHILE)) {
    return this->parseWhileLoop();
  }

  if (looksLikeType(peekAt(0)) &&
      peekAt(1).getKind() == TokenKind::TOK_IDENTIFIER &&
      peekAt(2).getKind() == TokenKind::TOK_ASSIGN) {
    return this->parseVarDeclStatement(AssignKind::Copy);
  }

  if (looksLikeType(peekAt(0)) &&
      peekAt(1).getKind() == TokenKind::TOK_IDENTIFIER &&
      peekAt(2).getKind() == TokenKind::TOK_MOVE) {
    return this->parseVarDeclStatement(AssignKind::Move);
  }

  if (looksLikeType(peekAt(0)) &&
      peekAt(1).getKind() == TokenKind::TOK_IDENTIFIER &&
      peekAt(2).getKind() == TokenKind::TOK_BORROW) {
    return this->parseVarDeclStatement(AssignKind::Borrow);
  }

  if (looksLikeType(peekAt(0)) &&
      peekAt(1).getKind() == TokenKind::TOK_IDENTIFIER &&
      peekAt(2).getKind() == TokenKind::TOK_SEMI) {
    throw ParseError(this->peek().getLine(), this->peek().getColumn(),
                     "Cannot have a null object, Var decl must have value");
  }

  auto expr = this->parseExpression();
  this->expect(TokenKind::TOK_SEMI, "Expected ';' after expression statement");
  return std::make_unique<ExprStmt>(std::move(expr));
}

std::unique_ptr<IfStmt> Parser::parseIfStatement() {
  this->expect(TokenKind::TOK_LPAREN, "Expected '(' to start comparison");
  auto condition = this->parseExpression();
  this->expect(TokenKind::TOK_RPAREN, "Expected ')' after comparison");
  auto thenBlock = this->parseBlock(true);

  std::unique_ptr<Block> elseBlock = nullptr;
  if (this->match(TokenKind::TOK_ElSE)) {
    if (this->match(TokenKind::TOK_IF)) {
      auto elseIf = this->parseIfStatement();
      elseBlock = std::make_unique<Block>();
      elseBlock->statements.push_back(std::move(elseIf));
    } else {
      elseBlock = this->parseBlock(true);
    }
  }

  return std::make_unique<IfStmt>(std::move(condition), std::move(thenBlock),
                                  std::move(elseBlock));
}

std::unique_ptr<WhileStmt> Parser::parseWhileLoop() {
  this->expect(TokenKind::TOK_LPAREN, "Expected '(' to start comparison");
  auto condition = this->parseExpression();
  this->expect(TokenKind::TOK_RPAREN, "Expected ')' after comparison");
  auto doBlock = this->parseBlock();

  return std::make_unique<WhileStmt>(std::move(condition), std::move(doBlock));
}

std::unique_ptr<Return> Parser::parseReturnStatement() {
  auto ret = std::make_unique<Return>();

  if (this->match(TokenKind::TOK_SEMI)) {
    return ret;
  }

  ret->value = this->parseExpression();
  this->expect(TokenKind::TOK_SEMI, "Expected ';' after return value");
  return ret;
}

std::unique_ptr<VarDecl> Parser::parseVarDeclStatement(AssignKind kind) {
  Token typeTok = this->consume();
  Token nameTok =
      this->expect(TokenKind::TOK_IDENTIFIER, "Expected variable name");
  bool moved = false;
  switch (kind) {
  case AssignKind::Copy:
    this->expect(TokenKind::TOK_ASSIGN, "Expected '=' after variable name");
    break;
  case AssignKind::Move:
    this->expect(TokenKind::TOK_MOVE, "Expected '<-' after variable name");
    moved = true;
    break;
  case AssignKind::Borrow:
    this->expect(TokenKind::TOK_BORROW, "Expected '&=' after variable name");
    break;
  }

  auto init = this->parseExpression();
  this->expect(TokenKind::TOK_SEMI, "Expected ';' after variable declaration");

  return std::make_unique<VarDecl>(Identifier{typeTok}, Identifier{nameTok},
                                   std::move(init), kind, moved);
}

std::unique_ptr<Expression> Parser::parseExpression() {
  return parseAssignment();
}

std::unique_ptr<Expression> Parser::parsePrimary() {
  Token tok = consume();

  switch (tok.getKind()) {
  case TokenKind::TOK_INT: {
    return std::make_unique<IntLitExpr>(IntLitExpr{IntegerLiteral{tok}});
  }

  case TokenKind::TOK_FLOAT: {
    std::cout << tok.getWord() << "\n";
    return std::make_unique<FloatLitExpr>(FloatLitExpr{FloatLiteral{tok}});
  }

  case TokenKind::TOK_STRING: {
    return std::make_unique<StrLitExpr>(StrLitExpr{StringLiteral{tok}});
  }

  case TokenKind::TOK_IDENTIFIER: {
    Identifier id{tok};

    if (match(TokenKind::TOK_LPAREN)) {
      auto call =
          std::make_unique<CallExpr>(CallExpr{id, std::vector<ExprPtr>()});

      if (!match(TokenKind::TOK_RPAREN)) {
        do {
          call->arguments.push_back(parseExpression());
        } while (match(TokenKind::TOK_COMMA));
        expect(TokenKind::TOK_RPAREN, "Expected ')' after call arguments");
      }
      return call;
    }

    return std::make_unique<IdentExpr>(IdentExpr{id});
  }

  default:
    throw ParseError(tok.getLine(), tok.getColumn(),
                     "Expected expression, got `" + tok.getWord() + "`");
  }
}

std::unique_ptr<Expression> Parser::parseAssignment() {
  auto left = this->parseEquality();

  if (match(TokenKind::TOK_ASSIGN)) {
    auto value = parseAssignment();
    if (auto *id = dynamic_cast<IdentExpr *>(left.get())) {
      return std::make_unique<AssignExpr>(id->name, std::move(value),
                                          AssignKind::Copy);
    }
    throw ParseError(peek().getLine(), peek().getColumn(),
                     "Left-hand side of assignment must be an identifier");
  }

  if (match(TokenKind::TOK_MOVE)) {
    auto value = parseAssignment();
    if (auto *id = dynamic_cast<IdentExpr *>(left.get())) {
      return std::make_unique<AssignExpr>(id->name, std::move(value),
                                          AssignKind::Move);
    }
    throw ParseError(peek().getLine(), peek().getColumn(),
                     "Left-hand side of move assignment must be an identifier");
  }

  if (match(TokenKind::TOK_BORROW)) {
    auto value = parseAssignment();
    if (auto *id = dynamic_cast<IdentExpr *>(left.get())) {
      return std::make_unique<AssignExpr>(id->name, std::move(value),
                                          AssignKind::Borrow);
    }
    throw ParseError(
        peek().getLine(), peek().getColumn(),
        "Left-hand side of borrow assignment must be an identifier");
  }

  return left;
}

std::unique_ptr<Expression> Parser::parseEquality() {
  auto expr = this->parseComparison();

  while (true) {
    if (this->match(TokenKind::TOK_EQ)) {
      auto right = this->parseComparison();
      expr = std::make_unique<BinaryExpr>(BinaryOp::Eq, std::move(expr),
                                          std::move(right));
    } else if (this->match(TokenKind::TOK_NE)) {
      auto right = this->parseComparison();
      expr = std::make_unique<BinaryExpr>(BinaryOp::Ne, std::move(expr),
                                          std::move(right));
    } else {
      break;
    }
  }
  return expr;
}

std::unique_ptr<Expression> Parser::parseComparison() {
  auto expr = this->parseAdditive();

  while (true) {
    if (this->match(TokenKind::TOK_LT)) {
      auto right = this->parseComparison();
      expr = std::make_unique<BinaryExpr>(BinaryOp::Lt, std::move(expr),
                                          std::move(right));
    } else if (this->match(TokenKind::TOK_GT)) {
      auto right = this->parseComparison();
      expr = std::make_unique<BinaryExpr>(BinaryOp::Gt, std::move(expr),
                                          std::move(right));
    } else if (this->match(TokenKind::TOK_LE)) {
      auto right = this->parseComparison();
      expr = std::make_unique<BinaryExpr>(BinaryOp::Le, std::move(expr),
                                          std::move(right));
    } else if (this->match(TokenKind::TOK_GE)) {
      auto right = this->parseComparison();
      expr = std::make_unique<BinaryExpr>(BinaryOp::Ge, std::move(expr),
                                          std::move(right));
    } else {
      break;
    }
  }

  return expr;
}

std::unique_ptr<Expression> Parser::parseAdditive() {
  auto expr = parseMultiplicative();

  while (true) {
    if (match(TokenKind::TOK_ADD)) {
      auto right = parseMultiplicative();
      expr = std::make_unique<BinaryExpr>(BinaryOp::Add, std::move(expr),
                                          std::move(right));
    } else if (match(TokenKind::TOK_SUB)) {
      auto right = parseMultiplicative();
      expr = std::make_unique<BinaryExpr>(BinaryOp::Sub, std::move(expr),
                                          std::move(right));
    } else {
      break;
    }
  }

  return expr;
}

std::unique_ptr<Expression> Parser::parseMultiplicative() {
  auto expr = parseUnary();

  while (true) {
    if (match(TokenKind::TOK_PROD)) {
      auto right = parseUnary();
      expr = std::make_unique<BinaryExpr>(BinaryOp::Mul, std::move(expr),
                                          std::move(right));
    } else if (match(TokenKind::TOK_DIV_FLOOR)) {
      // Check // before / so it takes priority
      auto right = parseUnary();
      expr = std::make_unique<BinaryExpr>(BinaryOp::DivFloor, std::move(expr),
                                          std::move(right));
    } else if (match(TokenKind::TOK_DIV)) {
      auto right = parseUnary();
      expr = std::make_unique<BinaryExpr>(BinaryOp::Div, std::move(expr),
                                          std::move(right));
    } else if (match(TokenKind::TOK_MOD)) {
      auto right = parseUnary();
      expr = std::make_unique<BinaryExpr>(BinaryOp::Mod, std::move(expr),
                                          std::move(right));
    } else {
      break;
    }
  }

  return expr;
}

std::unique_ptr<Expression> Parser::parseUnary() {
  if (match(TokenKind::TOK_SUB)) {
    auto operand = parseUnary();
    return std::make_unique<UnaryExpr>(UnaryOp::Negate, std::move(operand));
  }
  return parsePostfix();
}

std::unique_ptr<Expression> Parser::parsePostfix() {
  auto expr = parsePrimary();

  if (match(TokenKind::TOK_INCREMENT)) {
    if (auto *id = dynamic_cast<IdentExpr *>(expr.get())) {
      return std::make_unique<Increment>(id->name);
    }
    throw ParseError(peek().getLine(), peek().getColumn(),
                     "++ can only follow an identifier");
  }
  if (match(TokenKind::TOK_DECREMENT)) {
    if (auto *id = dynamic_cast<IdentExpr *>(expr.get())) {
      return std::make_unique<Decrement>(id->name);
    }
    throw ParseError(peek().getLine(), peek().getColumn(),
                     "-- can only follow an identifier");
  }

  return expr;
}
