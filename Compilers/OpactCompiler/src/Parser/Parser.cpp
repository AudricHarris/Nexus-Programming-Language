
#include "Parser/Parser.hpp"
#include "Token/TokenType.hpp"

//----------------------//
//-- Token Navigation --//
//----------------------//

const Token &Parser::peek() const {
  if (this->idx >= this->tokens.size()) {
    static const Token EOF_TOKEN{TokenKind::END_OF_FILE, "", 0, 0};
    return EOF_TOKEN;
  }
  return this->tokens[this->idx];
}

const Token &Parser::peekAt(size_t offset) const {
  size_t idx = this->idx + offset;
  if (idx >= this->tokens.size()) {
    static const Token EOF_TOKEN{TokenKind::END_OF_FILE, "", 0, 0};
    return EOF_TOKEN;
  }
  return this->tokens[idx];
}

const Token Parser::consume() {
  if (this->idx >= this->tokens.size()) {
    static const Token EOF_TOKEN{TokenKind::END_OF_FILE, "", 0, 0};
    return EOF_TOKEN;
  }
  return this->tokens[this->idx++];
}

bool Parser::match(TokenKind k) {
  if (peek().getKind() == k) {
    consume();
    return true;
  }
  return false;
}

bool Parser::check(TokenKind kind) const {
  return !this->isAtEnd() && this->peek().getKind() == kind;
}

bool Parser::isAtEnd() const {
  return this->idx >= this->tokens.size() ||
         this->peek().getKind() == TokenKind::END_OF_FILE;
}
