#include "TokenType.h"
#include <string>

class Token {
private:
  TokenKind kind;
  std::string word;
  int line;
  int column;

public:
  Token(TokenKind k, std::string w, int l, int c)
      : kind(k), word(w), line(l), column(c) {}

  // getters
  TokenKind getKind() const { return this->kind; }
  std::string getWord() const { return this->word; }
  int getLine() const { return this->line; }
  int getColumn() const { return this->column; }
};
