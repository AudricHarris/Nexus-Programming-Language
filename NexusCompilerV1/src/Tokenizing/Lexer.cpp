#include "Lexer.h"
#include <cctype>
#include <iostream>

Lexer::Lexer(std::string src) : source(std::move(src)) {}

char Lexer::peek() const { return pos < source.size() ? source[pos] : '\0'; }

char Lexer::peekNext() const {
  return (pos + 1 < source.size()) ? source[pos + 1] : '\0';
}

char Lexer::advance() {
  if (pos >= source.size())
    return '\0';
  char c = source[pos++];
  if (c == '\n') {
    line++;
    col = 1;
  } else {
    col++;
  }
  return c;
}

void Lexer::skipWhitespace() {
  while (std::isspace(static_cast<unsigned char>(peek()))) {
    advance();
  }
}

Token Lexer::makeToken(TokenKind kind, std::string lexeme) {
  Token t(kind, std::move(lexeme), line, col);
  // Debug output
  std::cerr << "Creating token: kind=" << static_cast<int>(kind) << " lexeme='"
            << t.getLexeme() << "' at " << line << ":" << col << "\n";
  return t;
}

std::vector<Token> Lexer::tokenize() {
  std::vector<Token> tokens;

  while (pos < source.size()) {
    skipWhitespace();
    if (pos >= source.size())
      break;

    char c = peek();
    std::cerr << "Processing character: '" << c << "' at pos " << pos << "\n";

    // Handle comments
    if (c == '/') {
      char next = peekNext();

      if (next == '!') {
        std::cerr << "Found multi-line comment\n";
        advance();
        advance();

        while (peek() != '\0') {
          if (peek() == '!' && peekNext() == '/') {
            advance();
            advance();
            break;
          }
          advance();
        }
        continue;
      } else if (next == '*') {
        std::cerr << "Found single-line comment\n";
        advance();
        advance();

        while (peek() != '\n' && peek() != '\0') {
          advance();
        }
        continue;
      } else if (next == '/') {
        std::cerr << "Found // operator\n";
        advance();
        advance();
        tokens.push_back(makeToken(TokenKind::op_int_div, "//"));
        continue;
      }
    }

    // Identifiers and keywords
    if (std::isalpha(c) || c == '_') {
      size_t start = pos;
      while (std::isalnum(peek()) || peek() == '_') {
        advance();
      }
      std::string id = source.substr(start, pos - start);

      auto kw = TokenTable::getKindFromSpelling(id);
      if (kw) {
        tokens.push_back(makeToken(*kw, std::move(id)));
      } else {
        tokens.push_back(makeToken(TokenKind::identifier, std::move(id)));
      }
      continue;
    }

    // Numbers
    if (std::isdigit(c) || (c == '.' && std::isdigit(peekNext()))) {
      size_t start = pos;
      bool hasDot = false;

      while (true) {
        char p = peek();
        if (std::isdigit(p)) {
          advance();
          continue;
        }
        if (p == '.' && !hasDot && std::isdigit(peekNext())) {
          hasDot = true;
          advance();
          continue;
        }
        break;
      }

      std::string num = source.substr(start, pos - start);
      TokenKind kind = hasDot ? TokenKind::lit_float : TokenKind::lit_integer;
      tokens.push_back(makeToken(kind, std::move(num)));
      continue;
    }

    // String literals
    if (c == '"') {
      advance();
      size_t start = pos;
      while (peek() != '"' && peek() != '\0') {
        advance();
      }
      std::string str = source.substr(start, pos - start);
      if (peek() == '"') {
        advance();
      }
      tokens.push_back(makeToken(TokenKind::lit_string, std::move(str)));
      continue;
    }

    // Now handle all operators and delimiters
    // IMPORTANT: Check multi-char operators first!

    if (c == '(') {
      advance();
      std::cerr << "Adding LPAREN\n";
      tokens.push_back(makeToken(TokenKind::delim_lparen, "("));
      continue;
    }

    if (c == ')') {
      advance();
      std::cerr << "Adding RPAREN\n";
      tokens.push_back(makeToken(TokenKind::delim_rparen, ")"));
      continue;
    }

    if (c == '{') {
      advance();
      std::cerr << "Adding LBRACE\n";
      tokens.push_back(makeToken(TokenKind::delim_lbrace, "{"));
      continue;
    }

    if (c == '}') {
      advance();
      std::cerr << "Adding RBRACE\n";
      tokens.push_back(makeToken(TokenKind::delim_rbrace, "}"));
      continue;
    }

    if (c == ';') {
      advance();
      std::cerr << "Adding SEMICOLON\n";
      tokens.push_back(makeToken(TokenKind::delim_semicolon, ";"));
      continue;
    }

    if (c == ',') {
      advance();
      tokens.push_back(makeToken(TokenKind::delim_comma, ","));
      continue;
    }

    if (c == ':') {
      advance();
      tokens.push_back(makeToken(TokenKind::delim_colon, ":"));
      continue;
    }

    if (c == '.') {
      advance();
      tokens.push_back(makeToken(TokenKind::op_dot, "."));
      continue;
    }

    if (c == '*') {
      advance();
      tokens.push_back(makeToken(TokenKind::op_mult, "*"));
      continue;
    }

    if (c == '/') {
      advance();
      tokens.push_back(makeToken(TokenKind::op_div, "/"));
      continue;
    }

    if (c == '%') {
      advance();
      tokens.push_back(makeToken(TokenKind::op_mod, "%"));
      continue;
    }

    if (c == '&') {
      advance();
      if (peek() == '=') {
        advance();
        tokens.push_back(makeToken(TokenKind::op_and_equal, "&="));
      } else {
        tokens.push_back(makeToken(TokenKind::unknown, "&"));
      }
      continue;
    }

    if (c == '=') {
      advance();
      if (peek() == '>') {
        advance();
        tokens.push_back(makeToken(TokenKind::delim_arrow, "=>"));
      } else if (peek() == '=') {
        advance();
        tokens.push_back(makeToken(TokenKind::op_eq, "=="));
      } else {
        tokens.push_back(makeToken(TokenKind::op_assign, "="));
      }
      continue;
    }

    if (c == '<') {
      advance();
      if (peek() == '-') {
        advance();
        tokens.push_back(makeToken(TokenKind::op_move, "<-"));
      } else if (peek() == '=') {
        advance();
        tokens.push_back(makeToken(TokenKind::op_le, "<="));
      } else {
        tokens.push_back(makeToken(TokenKind::op_lt, "<"));
      }
      continue;
    }

    if (c == '>') {
      advance();
      if (peek() == '=') {
        advance();
        tokens.push_back(makeToken(TokenKind::op_ge, ">="));
      } else {
        tokens.push_back(makeToken(TokenKind::op_gt, ">"));
      }
      continue;
    }

    if (c == '-') {
      advance();
      if (peek() == '>') {
        advance();
        tokens.push_back(makeToken(TokenKind::op_arrow, "->"));
      } else {
        tokens.push_back(makeToken(TokenKind::op_minus, "-"));
      }
      continue;
    }

    if (c == '+') {
      advance();
      if (peek() == '+') {
        advance();
        tokens.push_back(makeToken(TokenKind::op_plus, "++"));
      } else {
        tokens.push_back(makeToken(TokenKind::op_plus, "+"));
      }
      continue;
    }

    if (c == '!') {
      advance();
      if (peek() == '=') {
        advance();
        tokens.push_back(makeToken(TokenKind::op_neq, "!="));
      } else {
        tokens.push_back(makeToken(TokenKind::unknown, "!"));
      }
      continue;
    }

    // Unknown character
    std::cerr << "Unknown character: '" << c << "'\n";
    advance();
    tokens.push_back(makeToken(TokenKind::unknown, std::string(1, c)));
  }

  tokens.push_back(makeToken(TokenKind::eof, "<EOF>"));
  return tokens;
}
