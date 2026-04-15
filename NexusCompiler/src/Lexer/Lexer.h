#ifndef LEXER_H
#define LEXER_H
#include "../Token/TokenType.h"
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

class Lexer {
private:
  const std::string &codeFile;
  const char *src = nullptr;
  size_t srcLen = 0;
  size_t pos = 0;
  size_t line = 1;
  size_t col = 1;
  void skipWhitespace();
  bool isIdentifierStart(char c);
  bool isIdentifierChar(char c);
  Token makeToken(TokenKind k, std::string_view spelling);

public:
  Lexer(const std::string &file) : codeFile(file) {}
  std::vector<Token> Tokenize();
};
#endif
