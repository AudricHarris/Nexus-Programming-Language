#ifndef PARSER_H
#define PARSER_H 

#include "Token/TokenType.hpp"
#include <cstddef>
#include <vector>

class Parser {
    private:
        std::vector<Token> tokens;
        size_t idx;
        const Token &peek() const;
        const Token &peekAt(size_t offset) const;
        const Token consume();
        bool match(TokenKind Kind);
        bool check(TokenKind kind) const;
        bool isAtEnd() const;
};


#endif // !PARSER_H
