#ifndef TOKEN_H
#define TOKEN_H

#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <variant>

enum class TokenKind : uint16_t {
  // Keywords (indices 0-11)
  kw_private,     // 0
  kw_public,      // 1
  kw_class,       // 2
  kw_match,       // 3
  kw_Constructor, // 4
  kw_Options,     // 5
  kw_Some,        // 6
  kw_None,        // 7
  kw_self,        // 8
  kw_new,         // 9
  kw_while,       // 10
  kw_true,        // 11

  // Types (indices 12-14)
  type_i32,  // 12
  type_f64,  // 13
  type_name, // 14

  // Identifiers (index 15)
  identifier, // 15

  // Literals (indices 16-19)
  lit_integer, // 16
  lit_float,   // 17
  lit_string,  // 18
  lit_bool,    // 19

  // Operators (indices 20-37)
  op_assign,    // 20
  op_move,      // 21
  op_arrow,     // 22
  op_plus,      // 23
  op_minus,     // 24
  op_mult,      // 25
  op_div,       // 26
  op_int_div,   // 27
  op_mod,       // 28
  op_dot,       // 29
  op_and_equal, // 30

  op_eq,  // 31
  op_neq, // 32
  op_lt,  // 33
  op_gt,  // 34
  op_le,  // 35
  op_ge,  // 36

  // Delimiters (indices 37-44)
  delim_lbrace,    // 37 (but showing as 38 in debug!)
  delim_rbrace,    // 38 (but showing as 39 in debug!)
  delim_lparen,    // 39 (but showing as 40 in debug!)
  delim_rparen,    // 40 (but showing as 41 in debug!)
  delim_semicolon, // 41 (but showing as 42 in debug!)
  delim_comma,     // 42
  delim_colon,     // 43
  delim_arrow,     // 44

  // Special (indices 45-48)
  comment,    // 45
  whitespace, // 46
  eof,        // 47
  unknown,    // 48

  NUM_TOKENS
};

struct TokenInfo {
  const char *name;
  const char *spelling;

  int precedence;
  bool isLeftAssociative;
  bool isKeyword;
  bool isOperator;
  bool isDelimiter;
  bool isLiteral;
};

class TokenTable {
private:
  static TokenInfo table[];

public:
  static const TokenInfo &getInfo(TokenKind kind);
  static bool isKeyword(TokenKind k) { return getInfo(k).isKeyword; }
  static bool isOperator(TokenKind k) { return getInfo(k).isOperator; }
  static int getPrecedence(TokenKind k) { return getInfo(k).precedence; }

  static std::optional<TokenKind> getKindFromSpelling(const std::string &s);
};

class Token {
private:
  TokenKind kind;
  std::string lexeme;
  size_t line;
  size_t column;

  std::variant<std::monostate, int64_t, double, std::string, bool> value;

public:
  Token(TokenKind k, std::string lex, size_t ln, size_t col)
      : kind(k), lexeme(std::move(lex)), line(ln), column(col) {}

  TokenKind getKind() const { return kind; }
  const std::string &getLexeme() const { return lexeme; }
  size_t getLine() const { return line; }
  size_t getColumn() const { return column; }

  const TokenInfo &getInfo() const { return TokenTable::getInfo(kind); }
  const char *getName() const {
    const char *n = getInfo().name;
    return n ? n : "<unnamed_token>";
  }

  std::string getNameSafe() const {
    const char *n = getInfo().name;
    if (n)
      return n;
    return "<missing_name kind=" + std::to_string(static_cast<uint16_t>(kind)) +
           ">";
  }

  bool isKeyword() const { return getInfo().isKeyword; }
  bool isOperator() const { return getInfo().isOperator; }
  int getPrecedence() const { return getInfo().precedence; }

  std::string toString() const {
    return getNameSafe() + " '" + lexeme + "' at " + std::to_string(line) +
           ":" + std::to_string(column);
  }
};

#endif
