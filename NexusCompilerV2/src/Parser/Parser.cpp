#include "Parser.h"
#include "../Dictionary/TokenType.h"
#include "ParserError.h"

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

  while (this->isAtEnd()) {
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
  Token nameToken =
      expect(TokenKind::TOK_IDENTIFIER, "Expected function name at top level");

  // (
  expect(TokenKind::TOK_LPAREN, "Expected '(' after function name");

  std::vector<Parameter> params;

  // Optional parameters
  if (!match(TokenKind::TOK_RPAREN)) {
    do {
      Token typeToken =
          expect(TokenKind::TOK_IDENTIFIER, "Expected parameter type");
      Token nameTokenParam = expect(TokenKind::TOK_IDENTIFIER,
                                    "Expected parameter name after type");

      Parameter p{Identifier{typeToken}, Identifier{nameTokenParam}};

      params.push_back(std::move(p));
    } while (match(TokenKind::TOK_COMMA));

    expect(TokenKind::TOK_RPAREN, "Expected ')' after parameter list");
  }

  // Body
  // auto body = parseBlock();

  // Construct and return
  return std::make_unique<Function>(Identifier{nameToken}, std::move(params),
                                    std::move(nullptr));
}
