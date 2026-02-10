#include "Lexer.h"
#include <cctype>
#include <iostream>
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

  std::string currentWord;
  while (this->pos < this->codeFile.length()) {
    skipWhitespace();

    if (this->peek() == '(')
    {
      std::cout << "Parenthese " << '\n';
    }

    if (this->peek() == ')')
    {
      std::cout << "Parenthese close " << '\n';
    }
    currentWord += next();
  }
  return lstTokens;
}

/*
 * Instructions for tokenize
 * Step 1 ignore white space
 * Step 2 create a string
 * step 3 check if symbol has any importance
 * Step 4 if it doesn't then add to string
 * step 5 if space then stop create token from string and reset
 * step 6 if symbol create token for string and then token for symbol
 *
 * Default : all words are identifier unknown types will be in parser
 * */
