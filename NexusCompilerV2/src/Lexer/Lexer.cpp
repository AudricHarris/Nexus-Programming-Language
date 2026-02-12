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

// Determine if it can be the start of identifier
bool Lexer::isIdentifierStart(char c) {
  return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

// Determine if it can be in the content of a identifier
bool Lexer::isIdentifierChar(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

Token Lexer::makeToken(TokenKind k, std::string spelling) {
  Token t(k, spelling, line, col);
  return t;
}

// Public
std::vector<Token> Lexer::Tokenize() {
  std::vector<Token> lstTokens;

  while (this->pos < this->codeFile.length()) {
    skipWhitespace();

    // Sybols
    if (this->peek() == '(') {
      this->next();
      lstTokens.push_back(makeToken(TokenKind::TOK_LPAREN, "("));
      continue;
    }
    if (this->peek() == ')') {
      this->next();
      lstTokens.push_back(makeToken(TokenKind::TOK_RPAREN, ")"));
      continue;
    }
    if (this->peek() == '{') {
      this->next();
      lstTokens.push_back(makeToken(TokenKind::TOK_LBRACE, "{"));
      continue;
    }
    if (this->peek() == '}') {
      this->next();
      lstTokens.push_back(makeToken(TokenKind::TOK_RBRACE, "}"));
      continue;
    }
    if (this->peek() == '=') {
      this->next();
      lstTokens.push_back(makeToken(TokenKind::TOK_ASSIGN, "="));
      continue;
    }
    if (this->peek() == ';') {
      this->next();
      lstTokens.push_back(makeToken(TokenKind::TOK_SEMI, ";"));
      continue;
    }
    if (this->peek() == '+') {
      this->next();
      if (this->peeknext() == '+') {
        this->next();
        lstTokens.push_back(makeToken(TokenKind::TOK_INCREMENT, "++"));
      }
      // else
      //  makeToken(TokenKind::TOK_, "+");
      continue;
    }

    // Identifiers
    if (this->isIdentifierStart(this->peek())) {
      std::string currentWord;
      while (this->isIdentifierChar(this->peek())) {
        currentWord += this->next();
      }
      std::cout << "Identifier " << currentWord << '\n';
      lstTokens.push_back(makeToken(TokenKind::TOK_IDENTIFIER, currentWord));
      continue;
    }

    if (this->peek() == '"') {
      std::string currentWord;
      this->next();
      while (this->pos < this->codeFile.length() && this->peek() != '"') {
        currentWord += this->next();
      }
      std::cerr << "\033[46m" << "Litteral string found : " << currentWord
                << "\033[0m"
                << "\n";
      lstTokens.push_back(makeToken(TokenKind::TOK_STRING, currentWord));
      this->next();
      continue;
    }

    // Determine if number litteral
    if (std::isdigit(this->peek())) {
      std::string currentWord;
      do {
        currentWord += this->next();
      } while (std::isdigit(this->peek()));

      std::cerr << "\033[42m" << "Litteral Number found : " << currentWord
                << "\033[0m"
                << "\n";
      lstTokens.push_back(makeToken(TokenKind::TOK_INT, currentWord));
      continue;
    }

    // End case so \0 I assume
    if (this->peek() == '\0') {
      this->next();
      lstTokens.push_back(makeToken(TokenKind::TOK_EOF, "<EOF>"));
      continue;
    }

    std::cerr << "\033[31m" << "Unknown symbol [" << this->peek() << "]\033[0m"
              << "\n";
    lstTokens.push_back(makeToken(TokenKind::TOK_UNKNOWN, "<UNKNOWN>"));
    this->next();
  }

  std::cout << "Line & col " << this->line << " " << this->col << '\n';

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
