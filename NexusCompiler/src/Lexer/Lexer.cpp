#include "Lexer.h"
#include <cstdint>
#include <emmintrin.h>
#include <iostream>
#include <string_view>
#include <vector>

/*------------*/
/* DFA states */
/*------------*/

enum class State : uint8_t {
  S0,  // start
  S1,  // after '+'
  S2,  // after '-'
  S3,  // after '/'
  S4,  // integer digits
  S5,  // digits '.' expects at least one more digit
  S6,  // float digits after '.'
  S7,  // identifier / keyword
  S8,  // after '='
  S9,  // after '<'
  S10, // after '>'
  S11, // after '&'
  S12, // after '!'
  S13, // after '|'
  S14, // inside double-quoted string
  S15, // opening single-quote
  S16, // char body
  S17, // opening single-quote
  S18, // closing single-quote expected
  ACCEPT,
  END,
  ERR
};

/*------------------*/
/* Input categories */
/*------------------*/

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

/*--------------------------------------*/
/* Transition table  T[state][inputCat] */
/*--------------------------------------*/

static constexpr State T[NSTATES][NCATS] = {
    /*S0 */ {State::S1, State::S2, State::S3, State::S4, E, State::S7,
             State::S8, State::S9, State::S10, State::S11, State::S12,
             State::S13, State::S14, State::S15, E, E, ER},
    /*S1 */ {AC, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E},
    /*S2 */ {E, AC, E, E, E, E, E, E, AC, E, E, E, E, E, E, E, E},
    /*S3 */ {E, E, AC, E, E, E, E, E, E, E, E, E, E, E, E, E, E},
    /*S4 */ {E, E, E, State::S4, State::S5, E, E, E, E, E, E, E, E, E, E, E, E},
    /*S5 */
    {ER, ER, ER, State::S6, ER, ER, ER, ER, ER, ER, ER, ER, ER, ER, ER, ER, ER},
    /*S6 */ {E, E, E, State::S6, E, E, E, E, E, E, E, E, E, E, E, E, E},
    /*S7 */ {E, E, E, State::S7, E, State::S7, E, E, E, E, E, E, E, E, E, E, E},
    /*S8 */ {E, E, E, E, E, E, AC, E, E, E, E, E, E, E, E, E, E},
    /*S9 */ {E, AC, E, E, E, E, AC, E, E, E, E, E, E, E, E, E, E},
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

/*----------------------------------------------------*/
/* State → TokenKind mapping T2[firstState][isAccept] */
/*----------------------------------------------------*/

static constexpr TokenKind T2[14][2] = {
    /* S0  */ {TokenKind::UNKNOWN, TokenKind::UNKNOWN},
    /* S1  */ {TokenKind::INCREMENT, TokenKind::ADD},
    /* S2  */ {TokenKind::DECREMENT, TokenKind::SUB},
    /* S3  */ {TokenKind::UNKNOWN, TokenKind::DIV},
    /* S4  */ {TokenKind::UNKNOWN, TokenKind::LIT_INT},
    /* S5  */ {TokenKind::UNKNOWN, TokenKind::UNKNOWN},
    /* S6  */ {TokenKind::UNKNOWN, TokenKind::LIT_FLOAT},
    /* S7  */ {TokenKind::UNKNOWN, TokenKind::IDENTIFIER},
    /* S8  */ {TokenKind::EQ, TokenKind::ASSIGN},
    /* S9  */ {TokenKind::LE, TokenKind::LT},
    /* S10 */ {TokenKind::GE, TokenKind::GT},
    /* S11 */ {TokenKind::DOUBLE_AND, TokenKind::AND},
    /* S12 */ {TokenKind::NE, TokenKind::NOT},
    /* S13 */ {TokenKind::OR, TokenKind::UNKNOWN},
};

/*-----------------------------------------*/
/* ASCII character → category lookup table */
/*-----------------------------------------*/

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

/*---------------------*/
/* Keyword recognition */
/*---------------------*/

static inline TokenKind keywordOrIdent(std::string_view w) {
  switch (w.size()) {
  case 2:
    if (w == "if")
      return TokenKind::IF;
    break;
  case 3:
    if (w == "new")
      return TokenKind::NEW;
    if (w == "for")
      return TokenKind::FOR;
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

/*------------------------------*/
/* Single-character token kinds */
/*------------------------------*/

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

/*---------------------*/
/* Whitespace skipping */
/*---------------------*/

void Lexer::skipWhitespace() {
  const char *p = this->src + this->pos;
  const char *end = this->src + this->srcLen;

  // Scalar lead-in: advance until we either reach the end or a non-space byte
  // We also track newlines here to keep line/col accurate.
  auto scalarSkip = [&]() {
    while (p < end && static_cast<unsigned char>(*p) <= 0x20) {
      if (*p == '\n') {
        ++this->line;
        this->col = 0;
      }
      ++this->col;
      ++p;
    }
  };

  scalarSkip();

  const __m128i thresh = _mm_set1_epi8(0x20);
  while (p + 16 <= end) {
    __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i *>(p));
    __m128i cmp = _mm_cmpgt_epi8(chunk, thresh);
    int mask = _mm_movemask_epi8(cmp);

    if (mask != 0) {
      int skip = __builtin_ctz(mask);
      for (int i = 0; i < skip; ++i) {
        if (p[i] == '\n') {
          ++this->line;
          this->col = 0;
        }
        ++this->col;
      }
      p += skip;
      break;
    }

    for (int i = 0; i < 16; ++i) {
      if (p[i] == '\n') {
        ++this->line;
        this->col = 0;
      }
      ++this->col;
    }
    p += 16;
  }

  scalarSkip();

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

/*---------------------------------------------------------------------------*/
/* Comment helpers                                                            */
/*---------------------------------------------------------------------------*/

static inline size_t skipLineComment(const char *src, size_t pos, size_t srcLen,
                                     size_t &col) {
  while (pos < srcLen && src[pos] != '\n') {
    ++pos;
    ++col;
  }
  return pos;
}
static inline size_t skipBlockComment(const char *src, size_t pos,
                                      size_t srcLen, char closeA, char closeB,
                                      size_t &line, size_t &col) {
  while (pos + 1 < srcLen) {
    if (src[pos] == closeA && src[pos + 1] == closeB) {
      pos += 2;
      col += 2;
      return pos;
    }
    if (src[pos] == '\n') {
      ++line;
      col = 0;
    }
    ++col;
    ++pos;
  }
  std::cerr << "\033[31mUnterminated block comment\033[0m\n";
  return srcLen;
}

/*------------------------*/
/* Main tokenisation loop */
/*------------------------*/

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

    if (c == '/' && this->pos + 1 < this->srcLen) {
      char next = this->src[this->pos + 1];

      if (next == '/') {
        this->pos += 2;
        this->col += 2;
        this->pos =
            skipLineComment(this->src, this->pos, this->srcLen, this->col);
        continue;
      }

      if (next == '*') {
        // Block comment  /* ... */
        this->pos += 2;
        this->col += 2;
        this->pos = skipBlockComment(this->src, this->pos, this->srcLen, '*',
                                     '/', this->line, this->col);
        continue;
      }

      if (next == '!') {
        // Block comment  /! ... !/
        this->pos += 2;
        this->col += 2;
        this->pos = skipBlockComment(this->src, this->pos, this->srcLen, '!',
                                     '/', this->line, this->col);
        continue;
      }
    }

    if (c == ':' && this->pos + 1 < this->srcLen &&
        this->src[this->pos + 1] == ':') {
      lstTokens.push_back(makeToken(TokenKind::COLON_COLON, "::"));
      this->pos += 2;
      this->col += 2;
      continue;
    }

    if (c == ':') {
      lstTokens.push_back(makeToken(TokenKind::COLON, ":"));
      ++this->pos;
      ++this->col;
      continue;
    }

    /*------------------------------------------------*/
    /* Compound assignment operators:  +=  -=  *=  /= */
    /*------------------------------------------------*/
    if (this->pos + 1 < this->srcLen && this->src[this->pos + 1] == '=') {
      TokenKind kind = TokenKind::UNKNOWN;
      switch (c) {
      case '+':
        kind = TokenKind::ADD_ASSIGN;
        break;
      case '-':
        kind = TokenKind::SUB_ASSIGN;
        break;
      case '*':
        kind = TokenKind::MUL_ASSIGN;
        break;
      case '/':
        kind = TokenKind::DIV_ASSIGN;
        break;
      default:
        break;
      }
      if (kind != TokenKind::UNKNOWN) {
        lstTokens.push_back(
            makeToken(kind, std::string_view(this->src + this->pos, 2)));
        this->pos += 2;
        this->col += 2;
        continue;
      }
    }

    /*------------------------*/
    /* DFA-based tokenisation */
    /*------------------------*/

    int icat = static_cast<int>(classify(c));
    State first = T[0][icat];

    // Single-char tokens exit immediately from S0 with END.
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
    bool isNumber = (first == State::S4);

    ++this->pos;
    ++this->col;

    State state = first;
    State lastSignificantState = first;

    // Drive the DFA forward one character at a time.
    while (this->pos < this->srcLen) {
      c = this->src[this->pos];
      icat = static_cast<int>(classify(c));
      State ns = T[static_cast<int>(state)][icat];

      if (state != State::ACCEPT && state != State::END && state != State::ERR)
        lastSignificantState = state;

      if (ns == State::END || ns == State::ACCEPT) {
        // Closing quotes must be consumed before we stop.
        if ((state == State::S14 &&
             static_cast<InputCat>(icat) == InputCat::QUOTE_D) ||
            ((state == State::S16 || state == State::S18) &&
             static_cast<InputCat>(icat) == InputCat::QUOTE_S)) {
          ++this->pos;
          ++this->col;
          if (state != State::ACCEPT && state != State::END)
            lastSignificantState = state;
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
        ++this->line;
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

    // Strip the surrounding quotes from string/char literals.
    if (isLiteral)
      spelling = std::string_view(this->src + spellingStart + 1,
                                  spellingLen > 2 ? spellingLen - 2 : 0);
    else
      spelling = std::string_view(this->src + spellingStart, spellingLen);

    int fi = static_cast<int>(first);

    if (isNumber) {
      bool isFloat = (lastSignificantState == State::S5 ||
                      lastSignificantState == State::S6);
      lstTokens.push_back(makeToken(
          isFloat ? TokenKind::LIT_FLOAT : TokenKind::LIT_INT, spelling));
      continue;
    }

    if (fi >= 1 && fi <= 13) {
      TokenKind kind;

      if (first == State::S2 && spelling == "->")
        kind = TokenKind::RETURN_TYPE;
      else if (first == State::S9 && spelling == "<-")
        kind = TokenKind::MOVE;
      else if (first == State::S11 && spelling == "&=")
        kind = TokenKind::BORROW;
      else if (first == State::S7)
        kind = keywordOrIdent(spelling);
      else {
        bool isAccept = (state == State::ACCEPT);
        kind = T2[fi][isAccept ? 0 : 1];
      }

      lstTokens.push_back(makeToken(kind, spelling));
      continue;
    }

    // String and char literals resolved by the last meaningful DFA state.
    switch (lastSignificantState) {
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
