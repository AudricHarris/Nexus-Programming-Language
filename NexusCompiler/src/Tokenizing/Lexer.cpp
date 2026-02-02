#include "Lexer.h"
#include <cctype>
#include <cstdio>   // only if you keep debug prints

Lexer::Lexer(std::string src) : source(std::move(src)) {}

char Lexer::peek() const {
	return pos < source.size() ? source[pos] : '\0';
}

char Lexer::peekNext() const {
	return (pos + 1 < source.size()) ? source[pos + 1] : '\0';
}

char Lexer::advance() {
	if (pos >= source.size()) return '\0';
	char c = source[pos++];
	if (c == '\n') { line++; col = 1; }
	else { col++; }
	return c;
}

void Lexer::skipWhitespace() {
	while (std::isspace(static_cast<unsigned char>(peek()))) {
		advance();
	}
}

Token Lexer::makeToken(TokenKind kind, std::string lexeme) {
	return Token(kind, std::move(lexeme), line, col - lexeme.size());
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (pos < source.size()) {
        skipWhitespace();
        if (pos >= source.size()) break;

        char c = peek();

        if (c == '/') {
            char next = peekNext();

            if (next == '!') {
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
            }
            else if (next == '*') {
                advance();
                advance();

                while (peek() != '\n' && peek() != '\0') {
                    advance();
                }
                continue;
            }
        }

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

        if (std::isdigit(c) || (c == '.' && std::isdigit(peekNext()))) {
            size_t start = pos;
            bool hasDot = false;

            while (true) {
                char p = peek();
                if (std::isdigit(p)) {
                    advance();
                    continue;
                }
                if (p == '.' && !hasDot) {
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

        switch (c) {
            case '&':
                advance();
                if (peek() == '=') {
                    tokens.push_back(makeToken(TokenKind::op_and_equal, "&="));
                    advance();
                } else {
                    tokens.push_back(makeToken(TokenKind::unknown, "&"));
                }
                break;

            case '=':
                advance();
                if (peek() == '>') {
                    tokens.push_back(makeToken(TokenKind::delim_arrow, "=>"));
                    advance();
                } else {
                    tokens.push_back(makeToken(TokenKind::op_assign, "="));
                }
                break;

            case '<':
                advance();
                if (peek() == '-') {
                    tokens.push_back(makeToken(TokenKind::op_move, "<-"));
                    advance();
                } else {
                    tokens.push_back(makeToken(TokenKind::op_lt, "<"));
                }
                break;

            case '-':
                advance();
                if (peek() == '>') {
                    tokens.push_back(makeToken(TokenKind::op_arrow, "->"));
                    advance();
                } else {
                    tokens.push_back(makeToken(TokenKind::op_minus, "-"));
                }
                break;

            // Single-character operators & punctuation
            case '+':   tokens.push_back(makeToken(TokenKind::op_plus,     "+"));   advance(); break;
            case '*':   tokens.push_back(makeToken(TokenKind::op_mult,     "*"));   advance(); break;
            case '/':   tokens.push_back(makeToken(TokenKind::op_div,      "/"));   advance(); break;
            case '%':   tokens.push_back(makeToken(TokenKind::op_mod,      "%"));   advance(); break;

            case '(':   tokens.push_back(makeToken(TokenKind::delim_lparen,   "(")); advance(); break;
            case ')':   tokens.push_back(makeToken(TokenKind::delim_rparen,   ")")); advance(); break;
            case '{':   tokens.push_back(makeToken(TokenKind::delim_lbrace,   "{")); advance(); break;
            case '}':   tokens.push_back(makeToken(TokenKind::delim_rbrace,   "}")); advance(); break;
            case ';':   tokens.push_back(makeToken(TokenKind::delim_semicolon,";")); advance(); break;
            case ',':   tokens.push_back(makeToken(TokenKind::delim_comma,    ",")); advance(); break;
            case ':':   tokens.push_back(makeToken(TokenKind::delim_colon,    ":")); advance(); break;

            default:
                // Unknown / unhandled character
                tokens.push_back(makeToken(TokenKind::unknown, std::string(1, c)));
                advance();
                break;
        }
    }

    tokens.push_back(makeToken(TokenKind::eof, "<EOF>"));
    return tokens;
}
