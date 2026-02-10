#include "Lexer.h"
#include <cctype>
#include <optional>
#include <vector>

// Overwriting methods from lexer

// Constructor
Lexer::Lexer(std::string f) { this->codeFile = f; }

// private
char Lexer::peek() const {
  return this->pos < this->codeFile.length() ? this->codeFile[this->pos] : '\0';
}
char Lexer::peeknext() const {
  return this->pos + 1 < this->codeFile.length() ? this->codeFile[this->pos + 1]
                                                 : '\0';
}

char Lexer::next() {
  if (this->pos >= this->codeFile.length())
    return '\0';

  char c = this->codeFile[this->pos++];

  if (c == '\n') {
    this->line++;
    this->col = 0;
  }
  this->col++;

  return c;
}

void Lexer::skipWhitespace() {
  while (std::isspace(static_cast<unsigned char>(peek()))) {
    next();
  }
}

Token Lexer::makeToken(TokenKind k, std::string spelling) {
  Token t(k, spelling, line, col);
  return t;
}

// Public
std::vector<Token> Lexer::Tokenize() {
  std::vector<Token> lstTokens;
  while (this->pos < this->codeFile.length()) {
    // Instructions
  }
  return lstTokens;
}
