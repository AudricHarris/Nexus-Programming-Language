#ifndef TokenType
#define TokenType

#include <string>

/*
 * Let's start with the basics and expand on that
 * We need the simplest program or a small program to get an idea on how to
 * build this
 *
 * Example test.nx :
 * Main()
 * {
 *    i32 num = 5;
 *    num++;
 *    /! I am testing code !/
 *    Printf("This is a test {num}");
 *    i32 num2 = num;
 *    /! num is now invalid if called compiler will scream !/
 *    return;
 * }
 *
 * BECOMES :
 * TOK_IDENTIFIER TOK_LPAREN TOK_RPAREN
 * TOK_LBRACE
 *    TOK_I32 TOK_IDENTIFIER TOK_ASSIGN TOK_INT TOK_SEMI
 *    TOK_COMMENT
 *    TOK_IDENTIFIER TOK_LPAREN TOK_STRING TOK_RPAREN TOK_SEMI
 *    TOK_I32 TOK_IDENTIFIER TOK_MOVE TOK_IDENTIFIER TOK_SEMI
 *    TOK_COMMENT
 *    TOK_RETURN TOK_SEMI
 * TOK_RBRACE
 *
 * This manual parsing allows me to identify different types
 * Example
 * variable/param = TYPE_X followed by KW_name
 * Method = KW_name + PUNC_lParam + (param)* + PUNC_rParam + body
 */

// This is a initial version and will see updates
enum class TokenKind {
  // KeyWords
  TOK_IDENTIFIER,
  TOK_RETURN,

  // Litterals
  TOK_INT,
  TOK_STRING,

  // Operations
  TOK_ASSIGN,
  TOK_INCREMENT,
  TOK_MOVE,

  // Punctuation / Delimiters
  TOK_LPAREN,
  TOK_RPAREN,
  TOK_LBRACE,
  TOK_RBRACE,
  TOK_SEMI,

  // SPECIAL
  TOK_COMMENT,
  TOK_EOF,
  TOK_UNKNOWN,

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
  Token(TokenKind k, std::string w, int l, int c)
      : kind(k), word(w), line(l), column(c) {}

  // getters
  TokenKind getKind() const { return this->kind; }
  std::string getWord() const { return this->word; }
  int getLine() const { return this->line; }
  int getColumn() const { return this->column; }
};
#endif
