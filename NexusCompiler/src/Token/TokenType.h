#ifndef TokenType
#define TokenType

#include <iostream>
#include <string>
#include <string_view>

enum class TokenKind {
  // KeyWords
  IDENTIFIER,
  IF,
  ElSE,
  WHILE,
  FOR,
  RETURN,
  CONST,
  IMPORT,
  PUBLIC,
  PRIVATE,
  PROTECTED,
  CONTINUE,
  BREAK,

  // Litterals
  LIT_INT,
  LIT_FLOAT,
  LIT_STRING,
  LIT_CHAR,
  LIT_BOOL,

  // Operations
  ASSIGN,
  INCREMENT,
  DECREMENT,
  MOVE,
  BORROW,
  ADD,
  SUB,
  PROD,
  DIV,
  MOD,
  LT, //<
  GT,
  LE, // <=
  GE,
  EQ,
  NE,
  AND,
  DOUBLE_AND,
  OR,
  NOT,
  ADD_ASSIGN,
  SUB_ASSIGN,
  MUL_ASSIGN,
  DIV_ASSIGN,
  DIVF_ASSIGN,

  // Punctuation / Delimiters
  LPAREN,
  RPAREN,
  LBRACKET,
  RBRACKET,
  LBRACE,
  RBRACE,
  COMMA,
  SEMI,
  DOT,
  COLON,
  COLON_COLON,

  // SPECIAL
  RETURN_TYPE,
  NEW,
  END_OF_FILE,
  COMMENT,
  UNKNOWN,

  NUM_TOKENS
};

struct TokenInfo {
  const char *typing;
  const char *spelling;
};

class Token {
private:
  TokenKind kind;
  std::string word;
  int line;
  int column;

public:
  Token(TokenKind k, std::string_view w, int l, int c)
      : kind(k), word(w), line(l), column(c) {}

  // getters
  TokenKind getKind() const { return this->kind; }
  std::string getWord() const { return this->word; }
  int getLine() const { return this->line; }
  int getColumn() const { return this->column; }
  std::string toString();
};

#endif
