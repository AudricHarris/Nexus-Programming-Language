#ifndef PARSER_H
#define PARSER_H 

#include "Parser/Ast.hpp"
#include "Token/TokenType.hpp"
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>


class CompilerPipeline;

class Parser {
	private:
		std::vector<Token> tokens;
		size_t idx = 0;

		// Module Manager
		CompilerPipeline* pipeline = nullptr;
		std::string filePath;

		const Token &peek() const;
		const Token &peekAt(size_t offset) const;
		const Token consume();

		bool match(TokenKind Kind);
		bool check(TokenKind kind) const;
		bool isAtEnd() const;
		
		std::string generateError(TokenKind kind, std::string errorMsg);
		
		Token expect(TokenKind kind, std::string_view errorMsg);
		
		void addModule(std::vector<std::string> path);
	protected:
		void synchronize();

	public:
		// Creation Process
		Parser(std::vector<Token> tok, std::string path = "", CompilerPipeline* pipe = nullptr)
			: tokens(std::move(tok)), idx(0), filePath(std::move(path)), pipeline(pipe) {}

		// Methods
		ExprPtr parseModule();
		ExprPtr parseTopLevel();
		ExprPtr parseImport();
		ExprPtr parseFunction(std::optional<Token> visib);
		ExprPtr parseBlock();
		ExprPtr parseExpression();
		ExprPtr parseVarDecl();
		ExprPtr parseAssignement();
		ExprPtr parseOr();
		ExprPtr parseAnd();
		ExprPtr parseEquality();
		ExprPtr parseComparison();
		ExprPtr parseAdditive();
		ExprPtr parseMultiplicative();
		ExprPtr parseUnary();
		ExprPtr parsePrimary();
};


#endif // !PARSER_H
