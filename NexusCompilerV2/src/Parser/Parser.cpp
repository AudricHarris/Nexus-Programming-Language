#include "Parser.h"
#include "../Dictionary/TokenType.h"
#include <cstdint>

struct SourceLoc {
  uint32_t line = 1;
  uint32_t column = 1;
  bool is_eof = false;
};

static const SourceLoc EOF_LOC{0, 0, true};

const Token &Parser::peek() const {
  if (currentIndex >= tokens.size()) {
    static const Token EOF_TOKEN{TokenKind::TOK_EOF, "", 0, 0};
    return EOF_TOKEN;
  }

  return tokens[currentIndex];
}
