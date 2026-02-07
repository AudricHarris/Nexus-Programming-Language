#ifndef LEXER_H
#define LEXER_H

#include "Token.h"
#include <string>
#include <vector>

class Lexer
{
	private:
		std::string source;
		size_t pos = 0;
		size_t line = 1;
		size_t col = 1;

		char peek() const;
		char peekNext() const;
		char advance();
		void skipWhitespace();

		Token makeToken(TokenKind kind, std::string lexeme);

	public:
		explicit Lexer(std::string src);
		std::vector<Token> tokenize();
};

#endif
