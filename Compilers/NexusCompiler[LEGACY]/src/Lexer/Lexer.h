#ifndef LEXER_H
#define LEXER_H
#include "../Token/TokenType.h"
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
/*
 * The lexer :
 * - Let's break down what it's goal and how it works. the lexer will be
 *   the code that actually will read the file and classify each fragment
 *   of code into tokens. Tokens vary with some beign keywords, others being
 *   Litterals and so much more. The true question is how do we go from
 *   code to tokens
 *
 *   My idea is to read the code character by character.
 *   By default if a token is found it will be added to a buffer and considered
 *   a identifier no matter what it is. The code will check if the current
 *   character at pos is meaningfull if not then add to buffer and increment pos
 *   we do this until having meaningfull character or a space character.
 *
 *   The idea of writing it down like this will allow me to code easilier the
 *   algorithm and classes.
 */
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
