#include "Lexer.h"
#include <cstdint>
#include <emmintrin.h>
#include <iostream>
#include <string_view>
#include <vector>

// ------------------- //
// DFA states          //
// ------------------- //
enum class State : uint8_t {
  S0,
  S1,
  S2,
  S3,
  S4,
  S5,
  S6,
  S7,
  S8,
  S9,
  S10,
  S11,
  S12,
  S13,
  S14,
  S15,
  S16,
  S17,
  S18,
  ACCEPT,
  END,
  ERR
};

// ---------------------- //
// Input categories       //
// ---------------------- //
enum class InputCat : uint8_t {
  PLUS,
  MINUS,
  SLASH,
  DIGIT,
  DOT,
  LETTER_UNDER,
  EQUAL,
  LT,
  GT,
  AMP,
  BANG,
  PIPE,
  QUOTE_D,
  QUOTE_S,
  BRACKET,
  UNI_CHAR,
  OTHER,
  COUNT
};

static constexpr int NSTATES = 19;
static constexpr int NCATS = static_cast<int>(InputCat::COUNT);

static constexpr State E = State::END;
static constexpr State AC = State::ACCEPT;
static constexpr State ER = State::ERR;

// -------------------------------- //
// Transition table (T[state][cat]) //
// -------------------------------- //

// If futur me updated the github the transition table should be there

// Symbol order :
//        PLUS   MINUS  SLASH  DIGIT  DOT    LTR    EQ     LT     GT     AMP
//        BANG   PIPE   Q_D    Q_S    BRKT   UNI    OTHER

static constexpr State T[NSTATES][NCATS] = {
    /*S0*/ {State::S1, State::S2, State::S3, State::S4, E, State::S7, State::S8,
            State::S9, State::S10, State::S11, State::S12, State::S13,
            State::S14, State::S15, E, E, ER},

    /*S1*/ {AC, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E},

    /*S2*/ {E, AC, E, E, E, E, E, E, AC, E, E, E, E, E, E, E, E},

    /*S3*/ {E, E, AC, E, E, E, E, E, E, E, E, E, E, E, E, E, E},

    /*S4*/ {E, E, E, State::S4, State::S5, E, E, E, E, E, E, E, E, E, E, E, E},

    /*S5*/
    {ER, ER, ER, State::S6, ER, ER, ER, ER, ER, ER, ER, ER, ER, ER, ER, ER, ER},

    /*S6*/ {E, E, E, State::S6, E, E, E, E, E, E, E, E, E, E, E, E, E},

    /*S7*/ {E, E, E, State::S7, E, State::S7, E, E, E, E, E, E, E, E, E, E, E},

    /*S8*/ {E, E, E, E, E, E, AC, E, E, E, E, E, E, E, E, E, E},

    /*S9*/ {E, AC, E, E, E, E, AC, E, E, E, E, E, E, E, E, E, E},

    /*S10*/ {E, E, E, E, E, E, AC, E, E, E, E, E, E, E, E, E, E},

    /*S11*/ {E, E, E, E, E, E, AC, E, E, AC, E, E, E, E, E, E, E},

    /*S12*/ {E, E, E, E, E, E, AC, E, E, E, E, E, E, E, E, E, E},

    /*S13*/
    {ER, ER, ER, ER, ER, ER, ER, ER, ER, ER, ER, AC, ER, ER, ER, ER, ER},

    /*S14*/
    {State::S14, State::S14, State::S14, State::S14, State::S14, State::S14,
     State::S14, State::S14, State::S14, State::S14, State::S14, State::S14, E,
     State::S14, State::S14, State::S14, State::S14},

    /*S15*/
    {State::S16, State::S16, State::S16, State::S16, State::S16, State::S16,
     State::S16, State::S16, State::S16, State::S16, State::S16, State::S16,
     State::S16, ER, State::S16, State::S16, State::S16},

    /*S16*/ {ER, ER, ER, ER, ER, ER, ER, ER, ER, ER, ER, ER, ER, E, ER, ER, ER},

    /*S17*/
    {State::S18, State::S18, State::S18, State::S18, State::S18, State::S18,
     State::S18, State::S18, State::S18, State::S18, State::S18, State::S18,
     State::S18, State::S18, State::S18, State::S18, State::S18},

    /*S18*/ {ER, ER, ER, ER, ER, ER, ER, ER, ER, ER, ER, ER, ER, E, ER, ER, ER},
};

static constexpr TokenKind T2[14][2] = {
    {TokenKind::UNKNOWN, TokenKind::UNKNOWN},
    {TokenKind::INCREMENT, TokenKind::ADD},
    {TokenKind::DECREMENT, TokenKind::SUB},
    {TokenKind::DIV_FLOOR, TokenKind::DIV},
    {TokenKind::UNKNOWN, TokenKind::LIT_INT},
    {TokenKind::UNKNOWN, TokenKind::UNKNOWN},
    {TokenKind::UNKNOWN, TokenKind::LIT_FLOAT},
    {TokenKind::UNKNOWN, TokenKind::IDENTIFIER},
    {TokenKind::EQ, TokenKind::ASSIGN},
    {TokenKind::LE, TokenKind::LT},
    {TokenKind::GE, TokenKind::GT},
    {TokenKind::DOUBLE_AND, TokenKind::AND},
    {TokenKind::NE, TokenKind::NOT},
    {TokenKind::OR, TokenKind::UNKNOWN},
};

// ------------------- //
// Helper functions    //
// ------------------- //

// Lookup table: maps ASCII (0-127) to InputCat
static constexpr InputCat catTable[128] = {
    InputCat::OTHER,        InputCat::OTHER,        InputCat::OTHER,
    InputCat::OTHER,        InputCat::OTHER,        InputCat::OTHER,
    InputCat::OTHER,        InputCat::OTHER,        InputCat::OTHER,
    InputCat::OTHER,        InputCat::OTHER,        InputCat::OTHER,
    InputCat::OTHER,        InputCat::OTHER,        InputCat::OTHER,
    InputCat::OTHER,        InputCat::OTHER,        InputCat::OTHER,
    InputCat::OTHER,        InputCat::OTHER,        InputCat::OTHER,
    InputCat::OTHER,        InputCat::OTHER,        InputCat::OTHER,
    InputCat::OTHER,        InputCat::OTHER,        InputCat::OTHER,
    InputCat::OTHER,        InputCat::OTHER,        InputCat::OTHER,
    InputCat::OTHER,        InputCat::OTHER,        InputCat::OTHER,
    InputCat::BANG,         InputCat::QUOTE_D,      InputCat::OTHER,
    InputCat::OTHER,        InputCat::UNI_CHAR,     InputCat::AMP,
    InputCat::QUOTE_S,      InputCat::BRACKET,      InputCat::BRACKET,
    InputCat::UNI_CHAR,     InputCat::PLUS,         InputCat::UNI_CHAR,
    InputCat::MINUS,        InputCat::DOT,          InputCat::SLASH,
    InputCat::DIGIT,        InputCat::DIGIT,        InputCat::DIGIT,
    InputCat::DIGIT,        InputCat::DIGIT,        InputCat::DIGIT,
    InputCat::DIGIT,        InputCat::DIGIT,        InputCat::DIGIT,
    InputCat::DIGIT,        InputCat::OTHER,        InputCat::UNI_CHAR,
    InputCat::LT,           InputCat::EQUAL,        InputCat::GT,
    InputCat::OTHER,        InputCat::OTHER,        InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER, InputCat::LETTER_UNDER, InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER, InputCat::LETTER_UNDER, InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER, InputCat::LETTER_UNDER, InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER, InputCat::LETTER_UNDER, InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER, InputCat::LETTER_UNDER, InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER, InputCat::LETTER_UNDER, InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER, InputCat::LETTER_UNDER, InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER, InputCat::LETTER_UNDER, InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER, InputCat::BRACKET,      InputCat::OTHER,
    InputCat::BRACKET,      InputCat::OTHER,        InputCat::LETTER_UNDER,
    InputCat::OTHER,        InputCat::LETTER_UNDER, InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER, InputCat::LETTER_UNDER, InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER, InputCat::LETTER_UNDER, InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER, InputCat::LETTER_UNDER, InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER, InputCat::LETTER_UNDER, InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER, InputCat::LETTER_UNDER, InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER, InputCat::LETTER_UNDER, InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER, InputCat::LETTER_UNDER, InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER, InputCat::LETTER_UNDER, InputCat::LETTER_UNDER,
    InputCat::BRACKET,      InputCat::PIPE,         InputCat::BRACKET,
    InputCat::OTHER,        InputCat::OTHER,
};

static inline InputCat classify(char c) {
  unsigned char uc = static_cast<unsigned char>(c);
  return uc < 128 ? catTable[uc] : InputCat::OTHER;
}

static inline TokenKind keywordOrIdent(std::string_view w) {
  switch (w.size()) {
  case 2:
    if (w == "if")
      return TokenKind::IF;
    break;
  case 3:
    if (w == "new")
      return TokenKind::NEW;
    break;
  case 4:
    if (w == "else")
      return TokenKind::ElSE;
    if (w == "true")
      return TokenKind::LIT_BOOL;
    break;
  case 5:
    if (w == "while")
      return TokenKind::WHILE;
    if (w == "false")
      return TokenKind::LIT_BOOL;
    if (w == "const")
      return TokenKind::CONST;
    if (w == "break")
      return TokenKind::BREAK;
    break;
  case 6:
    if (w == "return")
      return TokenKind::RETURN;
    if (w == "import")
      return TokenKind::IMPORT;
    if (w == "public")
      return TokenKind::PUBLIC;
    break;

  case 7:
    if (w == "private")
      return TokenKind::PRIVATE;
    break;
  case 8:
    if (w == "continue")
      return TokenKind::CONTINUE;
    break;
  case 9:
    if (w == "protected")
      return TokenKind::PRIVATE;
    break;
  }
  return TokenKind::IDENTIFIER;
}

static inline TokenKind singleCharKind(char c) {
  switch (c) {
  case '(':
    return TokenKind::LPAREN;
  case ')':
    return TokenKind::RPAREN;
  case '[':
    return TokenKind::LBRACKET;
  case ']':
    return TokenKind::RBRACKET;
  case '{':
    return TokenKind::LBRACE;
  case '}':
    return TokenKind::RBRACE;
  case ';':
    return TokenKind::SEMI;
  case ',':
    return TokenKind::COMMA;
  case '*':
    return TokenKind::PROD;
  case '%':
    return TokenKind::MOD;
  case '.':
    return TokenKind::DOT;
  default:
    return TokenKind::UNKNOWN;
  }
}

// ------------------- //
// Lexer methods       //
// ------------------- //

void Lexer::skipWhitespace() {
  const char *p = this->src + this->pos;
  const char *end = this->src + this->srcLen;

  while (p < end && static_cast<unsigned char>(*p) <= 0x20) {
    if (*p == '\n') {
      this->line++;
      this->col = 0;
    }
    ++this->col;
    ++p;
  }

  const __m128i thresh = _mm_set1_epi8(0x20);
  while (p + 16 <= end) {
    __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i *>(p));
    __m128i cmp = _mm_cmpgt_epi8(chunk, thresh);
    int mask = _mm_movemask_epi8(cmp);
    if (mask != 0) {
      int skip = __builtin_ctz(mask);
      for (int i = 0; i < skip; ++i) {
        if (p[i] == '\n') {
          this->line++;
          this->col = 0;
        }
        ++this->col;
      }
      p += skip;
      break;
    }
    for (int i = 0; i < 16; ++i) {
      if (p[i] == '\n') {
        this->line++;
        this->col = 0;
      }
      ++this->col;
    }
    p += 16;
  }

  while (p < end && static_cast<unsigned char>(*p) <= 0x20) {
    if (*p == '\n') {
      this->line++;
      this->col = 0;
    }
    ++this->col;
    ++p;
  }

  this->pos = static_cast<size_t>(p - this->src);
}

bool Lexer::isIdentifierChar(char c) {
  unsigned char uc = static_cast<unsigned char>(c);
  return (uc < 128) && (catTable[uc] == InputCat::LETTER_UNDER ||
                        catTable[uc] == InputCat::DIGIT);
}

Token Lexer::makeToken(TokenKind k, std::string_view spelling) {
  return Token(k, spelling, line, col);
}

// ------------------- //
// Main tokenization   //
// ------------------- //
std::vector<Token> Lexer::Tokenize() {
  this->src = this->codeFile.data();
  this->srcLen = this->codeFile.size();

  std::vector<Token> lstTokens;
  lstTokens.reserve(this->srcLen / 4 + 8);

  while (this->pos < this->srcLen) {
    skipWhitespace();
    if (this->pos >= this->srcLen)
      break;

    char c = this->src[this->pos];
    if (c == '\0') {
      ++this->pos;
      lstTokens.push_back(makeToken(TokenKind::END_OF_FILE, "<EOF>"));
      continue;
    }

    int icat = static_cast<int>(classify(c));
    State first = T[0][icat];

    if (first == State::END) {
      lstTokens.push_back(makeToken(
          singleCharKind(c), std::string_view(this->src + this->pos, 1)));
      ++this->pos;
      ++this->col;
      continue;
    }
    if (first == State::ERR) {
      std::cerr << "\033[31mUnknown symbol [" << c << "]\033[0m\n";
      ++this->pos;
      ++this->col;
      lstTokens.push_back(makeToken(TokenKind::UNKNOWN, "<UNKNOWN>"));
      continue;
    }

    size_t spellingStart = this->pos;
    bool isLiteral = (first == State::S14 || first == State::S15);

    ++this->pos;
    ++this->col;

    State state = first;

    while (this->pos < this->srcLen) {
      c = this->src[this->pos];
      icat = static_cast<int>(classify(c));
      State ns = T[static_cast<int>(state)][icat];

      if (ns == State::END || ns == State::ACCEPT) {
        if ((state == State::S14 &&
             static_cast<InputCat>(icat) == InputCat::QUOTE_D) ||
            ((state == State::S16 || state == State::S18) &&
             static_cast<InputCat>(icat) == InputCat::QUOTE_S)) {
          ++this->pos;
          ++this->col;
        } else if (ns == State::ACCEPT) {
          ++this->pos;
          ++this->col;
        }

        state = ns;
        break;
      }
      if (ns == State::ERR) {
        state = ns;
        break;
      }

      if (c == '\n') {
        this->line++;
        this->col = 0;
      }
      ++this->col;
      ++this->pos;
      state = ns;
    }

    if (state == State::ERR) {
      std::string_view bad(this->src + spellingStart,
                           this->pos - spellingStart);
      std::cerr << "\033[31mLexer error near [" << bad << "]\033[0m\n";
      lstTokens.push_back(makeToken(TokenKind::UNKNOWN, "<UNKNOWN>"));
      continue;
    }

    size_t spellingLen = this->pos - spellingStart;
    std::string_view spelling;
    if (isLiteral)
      spelling = std::string_view(this->src + spellingStart + 1,
                                  spellingLen > 2 ? spellingLen - 2 : 0);
    else
      spelling = std::string_view(this->src + spellingStart, spellingLen);

    int fi = static_cast<int>(first);

    if (fi >= 1 && fi <= 13) {
      TokenKind kind;
      if (first == State::S2 && spelling == "->") {
        kind = TokenKind::RETURN_TYPE;
      } else if (first == State::S9 && spelling == "<-") {
        kind = TokenKind::MOVE;
      } else if (first == State::S11 && spelling == "&=") {
        kind = TokenKind::BORROW;
      } else if (first == State::S7) {
        kind = keywordOrIdent(spelling);
      } else {
        kind = T2[fi][state == State::ACCEPT ? 0 : 1];
      }
      lstTokens.push_back(makeToken(kind, spelling));
      continue;
    }

    switch (first) {
    case State::S4:
      lstTokens.push_back(makeToken(TokenKind::LIT_INT, spelling));
      break;
    case State::S6:
      lstTokens.push_back(makeToken(TokenKind::LIT_FLOAT, spelling));
      break;
    case State::S14:
      lstTokens.push_back(makeToken(TokenKind::LIT_STRING, spelling));
      break;
    case State::S15:
    case State::S16:
    case State::S17:
    case State::S18:
      lstTokens.push_back(makeToken(TokenKind::LIT_CHAR, spelling));
      break;
    default:
      lstTokens.push_back(makeToken(TokenKind::UNKNOWN, "<UNKNOWN>"));
      break;
    }
  }

  return lstTokens;
}
