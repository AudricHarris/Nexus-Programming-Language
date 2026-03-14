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
    {"TOK_IF", "if"},
    {"TOK_ELSE", "else"},
    {"TOK_WHILE", "while"},
    {"TOK_RETURN", "return"},
    {"TOK_CONST", "const"},
    {"TOK_INT", nullptr},
    {"TOK_FLOAT", nullptr},
    {"TOK_STRING", nullptr},
    {"TOK_BOOL", nullptr},
    {"TOK_ASSIGN", "="},
    {"TOK_INCREMENT", "++"},
    {"TOK_DECREMENT", "--"},
    {"TOK_MOVE", "<-"},
    {"TOK_BORROW", "&="},
    {"TOK_ADD", "+"},
    {"TOK_SUB", "-"},
    {"TOK_PROD", "*"},
    {"TOK_DIV", "/"},
    {"TOK_DIV_FLOOR", "//"},
    {"TOK_MOD", "%"},
    {"TOK_LT", "<"},
    {"TOK_GT", ">"},
    {"TOK_LE", "<="},
    {"TOK_GE", ">="},
    {"TOK_EQ", "=="},
    {"TOK_NE", "!="},
    {"TOK_AND", "&"},
    {"TOK_DOUBLE_AND", "&&"},
    {"TOK_OR", "||"},
    {"TOK_NOT", "!"},
    {"TOK_LPAREN", "("},
    {"TOK_RPAREN", ")"},
    {"TOK_LBRACKET", "["},
    {"TOK_RBRACKET", "]"},
    {"TOK_LPAREN", "("},
    {"TOK_LBRACE", "{"},
    {"TOK_RBRACE", "}"},
    {"TOK_COMMA", ","},
    {"TOK_SEMI", ";"},
    {"TOK_DIV_FLOOR", "."},
    {"TOK_RETURN_TYPE", "->"},
    {"TOK_NEW", "new"},
    {"TOK_EOF", "<EOF>"},
    {"TOK_UNKNOWN", "<UNKNOWN>"},
};

std::string Token::toString() {
  switch (this->kind) {
  case TokenKind::TOK_IDENTIFIER:
    return "TOK_IDENTIFIER  ";
  case TokenKind::TOK_IF:
    return "TOK_IF  ";
  case TokenKind::TOK_ElSE:
    return "TOK ELSE  ";
  case TokenKind::TOK_WHILE:
    return "TOK_WHILE  ";
  case TokenKind::TOK_RETURN:
    return "TOK_RETURN  ";
  case TokenKind::TOK_CONST:
    return "TOK_CONST  ";
  case TokenKind::TOK_INT:
    return "TOK_INT  ";
  case TokenKind::TOK_FLOAT:
    return "TOK_FLOAT  ";
  case TokenKind::TOK_STRING:
    return "TOK_STRING  ";
  case TokenKind::TOK_CHAR:
    return "TOK_CHAR  ";
  case TokenKind::TOK_BOOL:
    return "TOK_BOOL  ";
  case TokenKind::TOK_ASSIGN:
    return "TOK_ASSIGN  ";
  case TokenKind::TOK_INCREMENT:
    return "TOK_INCREMENT  ";
  case TokenKind::TOK_DECREMENT:
    return "TOK_DECREMENT  ";
  case TokenKind::TOK_MOVE:
    return "TOK_MOVE  ";
  case TokenKind::TOK_BORROW:
    return "TOK_BORROW  ";
  case TokenKind::TOK_ADD:
    return "TOK_ADD  ";
  case TokenKind::TOK_SUB:
    return "TOK_SUB  ";
  case TokenKind::TOK_PROD:
    return "TOK_DIV  ";
  case TokenKind::TOK_DIV:
    return "TOK_DIV  ";
  case TokenKind::TOK_DIV_FLOOR:
    return "TOK_DIV_FLOOR  ";
  case TokenKind::TOK_MOD:
    return "TOK_MOD  ";
  case TokenKind::TOK_LT:
    return "TOK_LT  ";
  case TokenKind::TOK_GT:
    return "TOK_GT  ";
  case TokenKind::TOK_LE:
    return "TOK_LE  ";
  case TokenKind::TOK_GE:
    return "TOK_GE  ";
  case TokenKind::TOK_EQ:
    return "TOK_EQ  ";
  case TokenKind::TOK_NE:
    return "TOK_NE  ";
  case TokenKind::TOK_AND:
    return "TOK_AND  ";
  case TokenKind::TOK_DOUBLE_AND:
    return "TOK_DOUBLE_AND  ";
  case TokenKind::TOK_OR:
    return "TOK_OR  ";
  case TokenKind::TOK_NOT:
    return "TOK_NOT  ";
  case TokenKind::TOK_LPAREN:
    return "TOK_LPAREN  ";
  case TokenKind::TOK_RPAREN:
    return "TOK_RPAREN  ";
  case TokenKind::TOK_LBRACKET:
    return "TOK_LBRACKET  ";
  case TokenKind::TOK_RBRACKET:
    return "TOK_RBRACKET  ";
  case TokenKind::TOK_LBRACE:
    return "\nTOK_LBRACE\n";
  case TokenKind::TOK_RBRACE:
    return "TOK_RBRACE\n";
  case TokenKind::TOK_COMMA:
    return "TOK_COMMA  ";
  case TokenKind::TOK_SEMI:
    return "TOK_SEMI\n";
  case TokenKind::TOK_DOT:
    return "TOK_DOT  ";
  case TokenKind::TOK_RETURN_TYPE:
    return "TOK_RETURN_TYPE  ";
  case TokenKind::TOK_NEW:
    return "TOK_NEW  ";
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
