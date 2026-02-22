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

  // Did not match expected type ( this is not clean and I will probably not
  // optimize it bc lazy)
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

// allows the parser to read all files so even if error it shows multiple
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
  // Consume function name
  Token nameToken = this->expect(TokenKind::TOK_IDENTIFIER,
                                 "Expected function name at top level");

  // (
  this->expect(TokenKind::TOK_LPAREN, "Expected '(' after function name");

  std::vector<Parameter> params;

  // Optional parameters
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

  // Body
  auto body = parseBlock();
  // Construct and return
  return std::make_unique<Function>(Identifier{nameToken}, std::move(params),
                                    std::move(body));
}

std::unique_ptr<Block> Parser::parseBlock() {
  auto block = std::make_unique<Block>();

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

  return block;
}

std::unique_ptr<Statement> Parser::parseStatement() {
  Token current = this->peek();
  if (this->match(TokenKind::TOK_RETURN)) {
    return this->parseReturnStatement();
  }
  if (check(TokenKind::TOK_IDENTIFIER)) {
    Token next = tokens[currentIndex + 1];
    if (next.getKind() == TokenKind::TOK_IDENTIFIER) {
      return this->parseVarDeclStatement();
    }
  }

  auto expr = this->parseExpression();
  this->expect(TokenKind::TOK_SEMI, "Expected ';' after expression statement");
  return std::make_unique<ExprStmt>(std::move(expr));
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

std::unique_ptr<VarDecl> Parser::parseVarDeclStatement() {
  Token typeTok = this->expect(TokenKind::TOK_IDENTIFIER,
                               "Expected type in variable declaration");
  Token nameTok =
      this->expect(TokenKind::TOK_IDENTIFIER, "Expected variable name");

  this->expect(TokenKind::TOK_ASSIGN, "Expected '=' after variable name");

  auto init = this->parseExpression();

  this->expect(TokenKind::TOK_SEMI, "Expected ';' after variable declaration");

  auto vd = std::make_unique<VarDecl>(
      VarDecl{Identifier{typeTok}, Identifier{nameTok}, std::move(init)});

  return vd;
}

std::unique_ptr<Expression> Parser::parseExpression() {
  auto expr = this->parsePrimary();

  // postfix ++
  if (match(TokenKind::TOK_INCREMENT)) {
    if (auto *id = dynamic_cast<IdentExpr *>(expr.get())) {
      return std::make_unique<Increment>(Increment{id->name});
    }
    throw ParseError(peek().getLine(), peek().getColumn(),
                     "++ can only follow an identifier");
  }

  if (match(TokenKind::TOK_ASSIGN)) {
    auto value = parseExpression();

    if (auto *id = dynamic_cast<IdentExpr *>(expr.get())) {
      auto assign =
          std::make_unique<Assignment>(Assignment{id->name, std::move(value)});
      return this->parseAssignment();
    }
    throw ParseError(peek().getLine(), peek().getColumn(),
                     "Left side of = must be identifier");
  }

  return expr;
}

std::unique_ptr<Expression> Parser::parsePrimary() {
  Token tok = consume();

  switch (tok.getKind()) {
  case TokenKind::TOK_INT: {
    auto lit = std::make_unique<IntLitExpr>(IntLitExpr{IntegerLiteral{tok}});
    return lit;
  }

  case TokenKind::TOK_STRING: {
    auto lit = std::make_unique<StrLitExpr>(StrLitExpr{StringLiteral{tok}});
    return lit;
  }

  case TokenKind::TOK_IDENTIFIER: {
    Identifier id{tok};

    // function call: ident ( args )
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

    // plain variable use
    auto identExpr = std::make_unique<IdentExpr>(IdentExpr{id});
    return identExpr;
  }

  default:
    throw ParseError(tok.getLine(), tok.getColumn(),
                     "Expected expression, got " + tok.toString());
  }
}

std::unique_ptr<Expression> Parser::parseAssignment() {
  auto expr = parsePrimary(); // or parseCall() / parsePostfix() etc.

  if (match(TokenKind::TOK_ASSIGN)) {
    auto value = parseAssignment(); // right-recursive → right-associative

    if (auto *id = dynamic_cast<IdentExpr *>(expr.get())) {
      return std::make_unique<AssignExpr>(id->name, std::move(value));
    }

    throw ParseError(peek().getLine(), peek().getColumn(),
                     "Left-hand side of assignment must be an identifier");
  }

  return expr;
}
