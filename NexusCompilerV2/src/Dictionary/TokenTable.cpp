#include "TokenType.h"
#include <optional>
#include <string>
#include <unordered_map>

class TokenTable {
private:
  static TokenInfo table[];

public:
  static const TokenInfo &getInfo(TokenKind k) {
    return TokenTable::table[static_cast<int>(k)];
  }

  static std::optional<TokenKind> getKindFromSpelling(const std::string &s) {
    static const std::unordered_map<std::string, TokenKind> keywordMap = {
        {"i32", TokenKind::TOK_IDENTIFIER},
        {"return", TokenKind::TOK_RETURN},
    };

    auto it = keywordMap.find(s);
    if (it != keywordMap.end())
      return it->second;

    return std::nullopt;
  }
};

TokenInfo TokenTable::table[static_cast<int>(TokenKind::NUM_TOKENS)] = {
    {"TOK_IDENTIFIER", nullptr},
    {"TOK_RETURN", "return"},
    {"TOK_INT", nullptr},
    {"TOK_STRING", nullptr},
    {"TOK_ASSIGN", "="},
    {"TOK_INCREMENT", "++"},
    {"TOK_MOVE", "<-"},
    {"TOK_LPAREN", "("},
    {"TOK_RPAREN", ")"},
    {"TOK_LBRACE", "{"},
    {"TOK_LBRACE", "}"},
    {"TOK_COMMA", ","},
    {"TOK_SEMI", ";"},
    {"TOK_EOF", "<EOF>"},
    {"TOK_UNKNOWN", "<UNKNOWN>"},
};

std::string Token::toString() {
  switch (this->kind) {
  case TokenKind::TOK_IDENTIFIER:
    return "TOK_IDENTIFIER  ";
  case TokenKind::TOK_RETURN:
    return "TOK_RETURN  ";
  case TokenKind::TOK_INT:
    return "TOK_INT  ";
  case TokenKind::TOK_STRING:
    return "TOK_STRING  ";
  case TokenKind::TOK_ASSIGN:
    return "TOK_ASSIGN  ";
  case TokenKind::TOK_INCREMENT:
    return "TOK_INCREMENT  ";
  case TokenKind::TOK_MOVE:
    return "TOK_MOVE  ";
  case TokenKind::TOK_LPAREN:
    return "TOK_LPAREN  ";
  case TokenKind::TOK_RPAREN:
    return "TOK_RPAREN  ";
  case TokenKind::TOK_LBRACE:
    return "\nTOK_LBRACE\n";
  case TokenKind::TOK_RBRACE:
    return "TOK_RBRACE\n";
  case TokenKind::TOK_COMMA:
    return "TOK_COMMA  ";
  case TokenKind::TOK_SEMI:
    return "TOK_SEMI\n";
  case TokenKind::TOK_EOF:
    return "TOK_EOF\n\n";
  case TokenKind::TOK_UNKNOWN:
    return "TOK_UNKNOWN  ";
  case TokenKind::NUM_TOKENS:
    return "NUM_TOKENS  ";
  default:
    return "??? (invalid kind)";
  }
}
