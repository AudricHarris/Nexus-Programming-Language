#include "Lexer.hpp"
#include <cstdint>
#include <emmintrin.h>
#include <iostream>

/*------------*/
/* DFA states */
/*------------*/

enum class State : uint8_t {
  S0,  // start
  S1,  // after '+'
  S2,  // after '-'
  S3,  // after '/'
  S4,  // integer digits
  S5,  // digit(s) then '.', expects at least one more digit
  S6,  // float digits after '.'
  S7,  // identifier / keyword
  S8,  // after '='
  S9,  // after '<'
  S10, // after '>'
  S11, // after '&'
  S12, // after '!'
  S13, // after '|'
  S14, // inside double-quoted string
  S15, // single-quote open
  S16, // single char body inside single-quotes
  S17, // backslash escape inside single-quotes
  S18, // character after escape sequence, awaiting closing quote

  // Multi-character operator terminal states
  S1_PLUS_PLUS,   // "++"
  S2_MINUS_MINUS, // "--"
  S2_ARROW,       // "->"
  S8_FAT_ARROW,   // "=>"
  S9_MOVE,        // "<-"
  S9_LE,          // "<="
  S10_GE,         // ">="
  S11_BORROW,     // "&="
  S11_DOUBLE_AND, // "&&"
  S12_NE,         // "!="
  S8_EQ,          // "=="
  S13_OR,         // "||"

  END,
  ERR
};

/*------------------*/
/* Input categories */
/*------------------*/

// IMPORTANT: BACKSLASH must stay just before OTHER so that all previously
// established column indices (0..15) are unchanged.  Only the two new columns
// BACKSLASH (16) and OTHER (17) are appended; existing table rows gain exactly
// two cells at the end.

enum class InputCat : uint8_t {
  PLUS,         // 0
  MINUS,        // 1
  SLASH,        // 2
  DIGIT,        // 3
  DOT,          // 4
  LETTER_UNDER, // 5
  EQUAL,        // 6
  LT,           // 7
  GT,           // 8
  AMP,          // 9
  BANG,         // 10
  PIPE,         // 11
  QUOTE_D,      // 12
  QUOTE_S,      // 13
  BRACKET,      // 14
  UNI_CHAR,     // 15
  BACKSLASH,    // 16  ← new
  OTHER,        // 17
  COUNT
};

static constexpr int NSTATES = 31;
static constexpr int NCATS = static_cast<int>(InputCat::COUNT); // 18

static constexpr State E = State::END;
static constexpr State ER = State::ERR;

/*-----------------------------------------------------------------------*/
/* Transition table  T[state][inputCat]                                  */
/* Column order: +  -  /  0  .  a  =  <  >  &  !  |  "  '  []  ~  \  ? */
/*-----------------------------------------------------------------------*/

static constexpr State T[NSTATES][NCATS] = {
    /*S0 */
    {State::S1, State::S2, State::S3, State::S4, E, State::S7, State::S8,
     State::S9, State::S10, State::S11, State::S12, State::S13, State::S14,
     State::S15, E, E, /*\*/ E, ER},

    /*S1  after '+' */
    {State::S1_PLUS_PLUS, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E},

    /*S2  after '-' */
    {E, State::S2_MINUS_MINUS, E, E, E, E, E, E, State::S2_ARROW, E, E, E, E, E,
     E, E, E, E},

    /*S3  after '/' — single slash, comments already stripped above */
    {E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E},

    /*S4  integer digits */
    {E, E, E, State::S4, State::S5, E, E, E, E, E, E, E, E, E, E, E, E, E},

    /*S5  after digit+dot — must be followed by at least one digit */
    {ER, ER, ER, State::S6, ER, ER, ER, ER, ER, ER, ER, ER, ER, ER, ER, ER, ER,
     ER},

    /*S6  float digits after '.' */
    {E, E, E, State::S6, E, E, E, E, E, E, E, E, E, E, E, E, E, E},

    /*S7  identifier / keyword */
    {E, E, E, State::S7, E, State::S7, E, E, E, E, E, E, E, E, E, E, E, E},

    /*S8  after '=' */
    {E, E, E, E, E, E, State::S8_EQ, E, State::S8_FAT_ARROW, E, E, E, E, E, E,
     E, E, E},

    /*S9  after '<' */
    {E, State::S9_MOVE, E, E, E, E, State::S9_LE, E, E, E, E, E, E, E, E, E, E,
     E},

    /*S10 after '>' */
    {E, E, E, E, E, E, State::S10_GE, E, E, E, E, E, E, E, E, E, E, E},

    /*S11 after '&' */
    {E, E, E, E, E, E, State::S11_BORROW, E, E, State::S11_DOUBLE_AND, E, E, E,
     E, E, E, E, E},

    /*S12 after '!' */
    {E, E, E, E, E, E, State::S12_NE, E, E, E, E, E, E, E, E, E, E, E},

    /*S13 after '|' — only '||' is valid */
    {ER, ER, ER, ER, ER, ER, ER, ER, ER, ER, ER, State::S13_OR, ER, ER, ER, ER,
     ER, ER},

    /*S14 inside double-quoted string — everything passes through except closing
       '"' */
    {State::S14, State::S14, State::S14, State::S14, State::S14, State::S14,
     State::S14, State::S14, State::S14, State::S14, State::S14, State::S14, E,
     State::S14, State::S14, State::S14,
     /*\*/ State::S14, State::S14},

    /*S15 opening single-quote — next char is the body (or start of escape) */
    {State::S16, State::S16, State::S16, State::S16, State::S16, State::S16,
     State::S16, State::S16, State::S16, State::S16, State::S16, State::S16,
     State::S16, ER, State::S16, State::S16,
     /*\*/ State::S17, State::S16},

    /*S16 single char body — only closing '\'' is valid next */
    {ER, ER, ER, ER, ER, ER, ER, ER, ER, ER, ER, ER, ER, E, ER, ER, ER, ER},

    /*S17 after '\' inside char literal — any char is a valid escape target */
    {State::S18, State::S18, State::S18, State::S18, State::S18, State::S18,
     State::S18, State::S18, State::S18, State::S18, State::S18, State::S18,
     State::S18, State::S18, State::S18, State::S18,
     /*\*/ State::S18, State::S18},

    /*S18 after escape body — only closing '\'' is valid */
    {ER, ER, ER, ER, ER, ER, ER, ER, ER, ER, ER, ER, ER, E, ER, ER, ER, ER},

    // All multi-char terminal states are self-terminal; no further transitions.
    /*S1_PLUS_PLUS  */ {E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E},
    /*S2_MINUS_MINUS*/ {E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E},
    /*S2_ARROW      */ {E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E},
    /*S8_FAT_ARROW  */ {E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E},
    /*S9_MOVE       */ {E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E},
    /*S9_LE         */ {E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E},
    /*S10_GE        */ {E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E},
    /*S11_BORROW    */ {E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E},
    /*S11_DOUBLE_AND*/ {E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E},
    /*S12_NE        */ {E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E},
    /*S8_EQ         */ {E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E},
    /*S13_OR        */ {E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E},
};

/*------------------------------*/
/* State → TokenKind flat map   */
/*------------------------------*/

static constexpr TokenKind StateToToken[] = {
    /* S0               */ TokenKind::UNKNOWN,
    /* S1               */ TokenKind::ADD,
    /* S2               */ TokenKind::SUB,
    /* S3               */ TokenKind::DIV,
    /* S4               */ TokenKind::LIT_INT,
    /* S5               */ TokenKind::UNKNOWN,
    /* S6               */ TokenKind::LIT_FLOAT,
    /* S7               */ TokenKind::IDENTIFIER,
    /* S8               */ TokenKind::ASSIGN,
    /* S9               */ TokenKind::LT,
    /* S10              */ TokenKind::GT,
    /* S11              */ TokenKind::AND,
    /* S12              */ TokenKind::NOT,
    /* S13              */ TokenKind::UNKNOWN,
    /* S14              */ TokenKind::LIT_STRING,
    /* S15              */ TokenKind::LIT_CHAR,
    /* S16              */ TokenKind::LIT_CHAR,
    /* S17              */ TokenKind::LIT_CHAR,
    /* S18              */ TokenKind::LIT_CHAR,
    /* S1_PLUS_PLUS     */ TokenKind::INCREMENT,
    /* S2_MINUS_MINUS   */ TokenKind::DECREMENT,
    /* S2_ARROW         */ TokenKind::RETURN_TYPE,
    /* S8_FAT_ARROW     */ TokenKind::FAT_ARROW,
    /* S9_MOVE          */ TokenKind::MOVE,
    /* S9_LE            */ TokenKind::LE,
    /* S10_GE           */ TokenKind::GE,
    /* S11_BORROW       */ TokenKind::BORROW,
    /* S11_DOUBLE_AND   */ TokenKind::DOUBLE_AND,
    /* S12_NE           */ TokenKind::NE,
    /* S8_EQ            */ TokenKind::EQ,
    /* S13_OR           */ TokenKind::OR,
};

/*------------------------------------------------------------------*/
/* ASCII character → InputCat lookup (128 entries, index = codepoint) */
/*------------------------------------------------------------------*/

static constexpr InputCat catTable[128] = {
    // 0x00-0x1F  control characters
    InputCat::OTHER,
    InputCat::OTHER,
    InputCat::OTHER,
    InputCat::OTHER, // 00-03
    InputCat::OTHER,
    InputCat::OTHER,
    InputCat::OTHER,
    InputCat::OTHER, // 04-07
    InputCat::OTHER,
    InputCat::OTHER,
    InputCat::OTHER,
    InputCat::OTHER, // 08-0B
    InputCat::OTHER,
    InputCat::OTHER,
    InputCat::OTHER,
    InputCat::OTHER, // 0C-0F
    InputCat::OTHER,
    InputCat::OTHER,
    InputCat::OTHER,
    InputCat::OTHER, // 10-13
    InputCat::OTHER,
    InputCat::OTHER,
    InputCat::OTHER,
    InputCat::OTHER, // 14-17
    InputCat::OTHER,
    InputCat::OTHER,
    InputCat::OTHER,
    InputCat::OTHER, // 18-1B
    InputCat::OTHER,
    InputCat::OTHER,
    InputCat::OTHER,
    InputCat::OTHER, // 1C-1F

    // 0x20-0x2F  punctuation / operators
    InputCat::OTHER,    // 0x20 ' '
    InputCat::BANG,     // 0x21 '!'
    InputCat::QUOTE_D,  // 0x22 '"'
    InputCat::OTHER,    // 0x23 '#'
    InputCat::OTHER,    // 0x24 '$'
    InputCat::UNI_CHAR, // 0x25 '%'
    InputCat::AMP,      // 0x26 '&'
    InputCat::QUOTE_S,  // 0x27 '\''
    InputCat::BRACKET,  // 0x28 '('
    InputCat::BRACKET,  // 0x29 ')'
    InputCat::UNI_CHAR, // 0x2A '*'
    InputCat::PLUS,     // 0x2B '+'
    InputCat::UNI_CHAR, // 0x2C ','
    InputCat::MINUS,    // 0x2D '-'
    InputCat::DOT,      // 0x2E '.'
    InputCat::SLASH,    // 0x2F '/'

    // 0x30-0x39  '0'-'9'
    InputCat::DIGIT,
    InputCat::DIGIT,
    InputCat::DIGIT,
    InputCat::DIGIT,
    InputCat::DIGIT,
    InputCat::DIGIT,
    InputCat::DIGIT,
    InputCat::DIGIT,
    InputCat::DIGIT,
    InputCat::DIGIT,

    // 0x3A-0x40
    InputCat::OTHER,    // 0x3A ':'
    InputCat::UNI_CHAR, // 0x3B ';'
    InputCat::LT,       // 0x3C '<'
    InputCat::EQUAL,    // 0x3D '='
    InputCat::GT,       // 0x3E '>'
    InputCat::OTHER,    // 0x3F '?'
    InputCat::OTHER,    // 0x40 '@'

    // 0x41-0x5A  'A'-'Z'
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,

    // 0x5B-0x60
    InputCat::BRACKET,      // 0x5B '['
    InputCat::BACKSLASH,    // 0x5C '\'  ← was OTHER, now BACKSLASH
    InputCat::BRACKET,      // 0x5D ']'
    InputCat::OTHER,        // 0x5E '^'
    InputCat::LETTER_UNDER, // 0x5F '_'
    InputCat::OTHER,        // 0x60 '`'

    // 0x61-0x7A  'a'-'z'
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,
    InputCat::LETTER_UNDER,

    // 0x7B-0x7F
    InputCat::BRACKET, // 0x7B '{'
    InputCat::PIPE,    // 0x7C '|'
    InputCat::BRACKET, // 0x7D '}'
    InputCat::OTHER,   // 0x7E '~'
    InputCat::OTHER,   // 0x7F DEL
};

static inline InputCat classify(char c) {
  const auto uc = static_cast<unsigned char>(c);
  return uc < 128 ? catTable[uc] : InputCat::OTHER;
}

/*-----------------*/
/* Keyword matcher */
/*-----------------*/

static inline TokenKind keywordOrIdent(std::string_view w) {
  switch (w.size()) {
  case 2:
    if (w == "if")
      return TokenKind::IF;
    if (w == "fn")
      return TokenKind::FN;
    if (w == "as")
      return TokenKind::AS;
    break;
  case 3:
    if (w == "new")
      return TokenKind::NEW;
    if (w == "for")
      return TokenKind::FOR;
    if (w == "mut")
      return TokenKind::MUT;
    break;
  case 4:
    if (w == "else")
      return TokenKind::ElSE;
    if (w == "true")
      return TokenKind::LIT_BOOL;
    if (w == "loop")
      return TokenKind::LOOP;
    if (w == "enum")
      return TokenKind::ENUM;
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
    if (w == "match")
      return TokenKind::MATCH;
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

/*-------------------*/
/* Single-char kinds */
/*-------------------*/

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

/*------------------*/
/* Comment skippers */
/*------------------*/

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

/*--------------------*/
/* Whitespace skipper */
/*--------------------*/

void Lexer::skipWhitespace() {
  const char *p = src + pos;
  const char *end = src + srcLen;

  // Scalar prefix — handles newlines and brings p to a natural alignment
  // for the SIMD block (or exits early if already at non-whitespace).
  auto scalarSkip = [&]() {
    while (p < end && static_cast<unsigned char>(*p) <= 0x20) {
      if (*p == '\n') {
        ++line;
        col = 0;
      }
      ++col;
      ++p;
    }
  };

  scalarSkip();

  // SSE2: skip 16 bytes at a time when the entire chunk is whitespace.
  const __m128i thresh = _mm_set1_epi8(0x20);
  while (p + 16 <= end) {
    __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i *>(p));
    __m128i cmp = _mm_cmpgt_epi8(chunk, thresh);
    int mask = _mm_movemask_epi8(cmp);

    if (mask != 0) {
      // At least one non-whitespace byte in this chunk.
      const int skip = __builtin_ctz(mask); // leading whitespace bytes
      for (int i = 0; i < skip; ++i) {
        if (p[i] == '\n') {
          ++line;
          col = 0;
        }
        ++col;
      }
      p += skip;
      break;
    }

    // Whole 16-byte chunk is whitespace — track newlines and advance.
    for (int i = 0; i < 16; ++i) {
      if (p[i] == '\n') {
        ++line;
        col = 0;
      }
      ++col;
    }
    p += 16;
  }

  // Scalar tail — remainder after the last full 16-byte chunk.
  scalarSkip();
  pos = static_cast<size_t>(p - src);
}

/*--------------*/
/* Token helper */
/*--------------*/

Token Lexer::makeToken(TokenKind k, std::string_view spelling) const {
  return Token(k, spelling, line, col);
}

/*------------------------*/
/* Main tokenisation loop */
/*------------------------*/

std::vector<Token> Lexer::Tokenize() {
  std::vector<Token> tokens;
  tokens.reserve(srcLen > 16 ? (srcLen / 4) + 8 : 8);

  while (pos < srcLen) {
    skipWhitespace();
    if (pos >= srcLen)
      break;

    char c = src[pos];

    // Null byte → EOF marker (shouldn't appear in well-formed source).
    if (c == '\0') {
      ++pos;
      tokens.push_back(makeToken(TokenKind::END_OF_FILE, "<EOF>"));
      continue;
    }

    // Comments — must be checked before the DFA so '//' isn't seen as two DIVs.
    if (c == '/' && pos + 1 < srcLen) {
      const char next = src[pos + 1];
      if (next == '/') {
        pos += 2;
        col += 2;
        pos = skipLineComment(src, pos, srcLen, col);
        continue;
      }
      if (next == '*') {
        pos += 2;
        col += 2;
        pos = skipBlockComment(src, pos, srcLen, '*', '/', line, col);
        continue;
      }
      if (next == '!') {
        pos += 2;
        col += 2;
        pos = skipBlockComment(src, pos, srcLen, '!', '/', line, col);
        continue;
      }
    }

    // '::' and ':'
    if (c == ':') {
      if (pos + 1 < srcLen && src[pos + 1] == ':') {
        tokens.push_back(makeToken(TokenKind::COLON_COLON, "::"));
        pos += 2;
        col += 2;
      } else {
        tokens.push_back(makeToken(TokenKind::COLON, ":"));
        ++pos;
        ++col;
      }
      continue;
    }

    // Compound assignment operators: +=  -=  *=  /=
    // Checked before the DFA so they don't get split into two tokens.
    if (pos + 1 < srcLen && src[pos + 1] == '=') {
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
        tokens.push_back(makeToken(kind, std::string_view(src + pos, 2)));
        pos += 2;
        col += 2;
        continue;
      }
    }

    /*------------------------*/
    /* DFA-based tokenisation */
    /*------------------------*/

    const int icat = static_cast<int>(classify(c));
    const State first = T[0][icat];

    if (first == State::END) {
      tokens.push_back(
          makeToken(singleCharKind(c), std::string_view(src + pos, 1)));
      ++pos;
      ++col;
      continue;
    }

    if (first == State::ERR) {
      std::cerr << "\033[31mUnknown symbol [" << c << "]\033[0m\n";
      tokens.push_back(makeToken(TokenKind::UNKNOWN, "<UNKNOWN>"));
      ++pos;
      ++col;
      continue;
    }

    const size_t spellingStart = pos;
    const bool isLiteral = (first == State::S14 || first == State::S15);

    ++pos;
    ++col;

    State state = first;
    State lastSignificantState = first;

    while (pos < srcLen) {
      c = src[pos];
      const int icat2 = static_cast<int>(classify(c));
      const State ns = T[static_cast<int>(state)][icat2];

      if (ns == State::ERR) {
        state = ns;
        break;
      }

      // Multi-char terminal operator: consume the final character and stop.
      if (ns >= State::S1_PLUS_PLUS && ns <= State::S13_OR) {
        lastSignificantState = ns;
        ++pos;
        ++col;
        state = State::END;
        break;
      }

      if (state != State::END && state != State::ERR)
        lastSignificantState = state;

      if (ns == State::END) {
        // Consume the closing quote for string/char literals.
        if ((state == State::S14 &&
             static_cast<InputCat>(icat2) == InputCat::QUOTE_D) ||
            ((state == State::S16 || state == State::S18) &&
             static_cast<InputCat>(icat2) == InputCat::QUOTE_S)) {
          ++pos;
          ++col;
          lastSignificantState = state;
        }
        state = ns;
        break;
      }

      if (c == '\n') {
        ++line;
        col = 0;
      }
      ++col;
      ++pos;
      state = ns;
    }

    if (state == State::ERR) {
      std::cerr << "\033[31mLexer error near ["
                << std::string_view(src + spellingStart, pos - spellingStart)
                << "]\033[0m\n";
      tokens.push_back(makeToken(TokenKind::UNKNOWN, "<UNKNOWN>"));
      continue;
    }

    const size_t spellingLen = pos - spellingStart;
    // Strip enclosing quotes from string/char literals so the spelling
    // contains only the content (e.g. hello from "hello", n from '\n').
    std::string_view spelling =
        isLiteral ? std::string_view(src + spellingStart + 1,
                                     spellingLen > 2 ? spellingLen - 2 : 0)
                  : std::string_view(src + spellingStart, spellingLen);

    const TokenKind finalKind =
        (lastSignificantState == State::S7)
            ? keywordOrIdent(spelling)
            : StateToToken[static_cast<size_t>(lastSignificantState)];

    tokens.push_back(makeToken(finalKind, spelling));
  }

  return tokens;
}
