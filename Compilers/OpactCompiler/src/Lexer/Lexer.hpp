#ifndef LEXER_H
#define LEXER_H

#include "../Token/TokenType.hpp"
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

class Lexer {
public:
  explicit Lexer(const std::string &source)
      : src(source.data()), srcLen(source.size()) {}

  std::vector<Token> Tokenize();

private:
  const char *src;
  size_t srcLen;
  size_t pos = 0;
  size_t line = 1;
  size_t col = 1;

  void skipWhitespace();
  Token makeToken(TokenKind k, std::string_view spelling) const;
};

#endif // LEXER_H
