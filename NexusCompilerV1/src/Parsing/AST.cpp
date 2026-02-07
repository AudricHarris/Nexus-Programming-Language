#include "AST.h"
#include <stdexcept>

LiteralExpr::LiteralExpr(Token t) : token(std::move(t)) {
  // Parse the value based on token kind
  switch (token.getKind()) {
  case TokenKind::lit_integer:
    try {
      value = std::stoll(token.getLexeme());
    } catch (...) {
      value = 0LL;
    }
    break;

  case TokenKind::lit_float:
    try {
      value = std::stod(token.getLexeme());
    } catch (...) {
      value = 0.0;
    }
    break;

  case TokenKind::lit_string:
    value = token.getLexeme();
    break;

  case TokenKind::kw_true:
  case TokenKind::lit_bool:
    value = (token.getLexeme() == "true");
    break;

  default:
    // Unknown literal type, store as string
    value = token.getLexeme();
    break;
  }
}
