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
    {"TOK_IDENTIFIER", nullptr},  {"TOK_RETURN", "return"},
    {"TOK_INT", nullptr},         {"TOK_STRING", nullptr},
    {"TOK_ASSIGN", "="},          {"TOK_INCREMENT", "++"},
    {"TOK_MOVE", "<-"},           {"TOK_LPAREN", "("},
    {"TOK_RPAREN", ")"},          {"TOK_LBRACE", "{"},
    {"TOK_LBRACE", "}"},          {"TOK_SEMI", ";"},
    {"TOK_COMMENT", nullptr},     {"TOK_EOF", "<EOF>"},
    {"TOK_UNKNOWN", "<UNKNOWN>"},
};
