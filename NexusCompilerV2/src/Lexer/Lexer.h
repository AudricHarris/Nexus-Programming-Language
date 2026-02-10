#ifndef LEXER_H
#define LEXER_H

#include "../Dictionary/TokenType.h"
#include <string>
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
  std::string codeFile;
  int pos = 0;
  int line = 1;
  int col = 1;

  char peek() const;
  char peeknext() const;
  char next();
  void skipWhitespace();
  Token makeToken(TokenKind k, std::string spelling);

public:
  Lexer(std::string file);
  std::vector<Token> Tokenize();
};

#endif
