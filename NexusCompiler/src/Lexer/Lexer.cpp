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

bool Lexer::isIdentifierStart(char c) {
  return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

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
      if (this->peek() == '=') {
        this->next();
        lstTokens.push_back(makeToken(TokenKind::TOK_EQ, "=="));
      } else {
        lstTokens.push_back(makeToken(TokenKind::TOK_ASSIGN, "="));
      }
      continue;
    }
    if (this->peek() == '<') {
      this->next();
      if (this->peek() == '=') {
        this->next();
        lstTokens.push_back(makeToken(TokenKind::TOK_LE, "<="));
      } else if (this->peek() == '-') {
        this->next();
        lstTokens.push_back(makeToken(TokenKind::TOK_MOVE, "<-"));
      } else {
        lstTokens.push_back(makeToken(TokenKind::TOK_LT, "<"));
      }
      continue;
    }
    if (this->peek() == '>') {
      this->next();
      if (this->peek() == '=') {
        this->next();
        lstTokens.push_back(makeToken(TokenKind::TOK_GE, ">="));
      } else {
        lstTokens.push_back(makeToken(TokenKind::TOK_GT, ">"));
      }
      continue;
    }
    if (this->peek() == '&' && this->peeknext() == '=') {
      this->next();
      this->next();
      lstTokens.push_back(makeToken(TokenKind::TOK_BORROW, "&="));
      continue;
    }
    if (this->peek() == '!' && this->peeknext() == '=') {
      this->next();
      this->next();
      lstTokens.push_back(makeToken(TokenKind::TOK_NE, "!="));
      continue;
    }
    if (this->peek() == ';') {
      this->next();
      lstTokens.push_back(makeToken(TokenKind::TOK_SEMI, ";"));
      continue;
    }
    if (this->peek() == '+') {
      this->next();
      if (this->peek() == '+') {
        this->next();
        lstTokens.push_back(makeToken(TokenKind::TOK_INCREMENT, "++"));
      } else {
        lstTokens.push_back(makeToken(TokenKind::TOK_ADD, "+"));
      }

      continue;
    }
    if (this->peek() == '-') {
      this->next();
      if (this->peek() == '-') {
        this->next();
        lstTokens.push_back(makeToken(TokenKind::TOK_DECREMENT, "--"));
      } else {
        lstTokens.push_back(makeToken(TokenKind::TOK_SUB, "-"));
      }
      continue;
    }
    if (this->peek() == '*') {
      this->next();
      lstTokens.push_back(makeToken(TokenKind::TOK_PROD, "*"));
      continue;
    }
    if (this->peek() == '%') {
      this->next();
      lstTokens.push_back(makeToken(TokenKind::TOK_MOD, "%"));
      continue;
    }

    if (this->peek() == '/') {
      this->next();
      if (this->peek() == '/') {
        this->next();
        lstTokens.push_back(makeToken(TokenKind::TOK_DIV_FLOOR, "//"));
      } else {
        lstTokens.push_back(makeToken(TokenKind::TOK_DIV, "/"));
      }
      continue;
    }
    // Identifiers
    if (this->isIdentifierStart(this->peek())) {
      std::string currentWord;
      while (this->isIdentifierChar(this->peek())) {
        currentWord += this->next();
      }
      if (currentWord == "return") {
        lstTokens.push_back(makeToken(TokenKind::TOK_RETURN, currentWord));
      } else if (currentWord == "if") {
        lstTokens.push_back(makeToken(TokenKind::TOK_IF, currentWord));
      } else if (currentWord == "else") {
        lstTokens.push_back(makeToken(TokenKind::TOK_ElSE, currentWord));
      } else if (currentWord == "while") {
        lstTokens.push_back(makeToken(TokenKind::TOK_WHILE, currentWord));
      } else {
        lstTokens.push_back(makeToken(TokenKind::TOK_IDENTIFIER, currentWord));
      }
      continue;
    }

    if (this->peek() == '"') {
      std::string currentWord;
      this->next();
      while (this->pos < this->codeFile.length() && this->peek() != '"') {
        currentWord += this->next();
      }

      lstTokens.push_back(makeToken(TokenKind::TOK_STRING, currentWord));
      this->next();
      continue;
    }

    if (std::isdigit(this->peek())) {
      std::string currentWord;
      do {
        currentWord += this->next();
      } while (std::isdigit(this->peek()));
      if (this->peek() == '.') {
        do {
          currentWord += this->next();
        } while (std::isdigit(this->peek()));
        lstTokens.push_back(makeToken(TokenKind::TOK_FLOAT, currentWord));
      } else {
        lstTokens.push_back(makeToken(TokenKind::TOK_INT, currentWord));
      }
      continue;
    }

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

  return lstTokens;
}
