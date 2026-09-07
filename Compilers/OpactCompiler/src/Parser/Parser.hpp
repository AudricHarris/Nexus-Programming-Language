/**
 * @file Parser.hpp
 * @brief The header file that defines the Parser Class 
 */

#ifndef PARSER_H
#define PARSER_H 

// Custom
#include "Parser/Ast.hpp"
#include "Token/TokenType.hpp"
// Library
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

class CompilerPipeline;

/**
 * @class Parser
 * @brief The parser is the file that transform a list of tokens into a ast
 * The parser is based of the very common gradient descent iteration (It was the simplest and most common so I chose that)
 * It uses CompilerPipeline for adding modules and making treating multiple files in multiple threads speeding up the process
 */
class Parser {
	private:
		std::vector<Token> tokens; ///< The list of tokens that originated from the source file
		size_t idx = 0; ///< The idx of the current token.

		// Module Manager
		CompilerPipeline* pipeline = nullptr; ///< The pipeline which allows to parse modules and therefore do multithreaded parsing with each file
		std::string filePath; ///< The file name (not too used in our situation but it might be used for debugging 

		/** @brief this allows to return the token at current idx */
		const Token &peek() const;

		/** @brief this allows to return the token at the offset of the current idx */
		const Token &peekAt(size_t offset) const;
		
		/** @brief this return the token at current idx and increments the idx */
		const Token consume();

		/** @brief this return true if the current token at idx corresponds to the same type as the param token kind */
		bool match(TokenKind Kind);
		
		/** @brief this return true if the current token at idx corresponds to the same type as the param token kind, if true does this->consume()*/
		bool check(TokenKind kind) const;
		
		/** @brief this returns true if there are no more tokens */
		bool isAtEnd() const;
		
		/** @brief formats a string response for the Parser errors */
		std::string generateError(TokenKind kind, std::string errorMsg);
		
		/** @brief this expects a token if yes then returns the token else returns an error */
		Token expect(TokenKind kind, std::string_view errorMsg);
		
		/** @brief Adds the module with the CompilerPipeline if one is detected*/
		void addModule(std::vector<std::string> path);
	protected:
		/** @brief If there is an error with an expression this will allow to continue reading the file. Usually if this is called it means your code is invalid*/
		void synchronize();

	public:
		// Creation Process
		/** @brief Creates The parser with 3 params : list of tokens, name of path, the compiler pipeline*/
		Parser(std::vector<Token> tok, std::string path = "", CompilerPipeline* pipe = nullptr)
			: tokens(std::move(tok)), idx(0), filePath(std::move(path)), pipeline(pipe) {}

		// Methods
		ExprPtr parseModule();
		ExprPtr parseTopLevel();
		ExprPtr parseImport();
		ExprPtr parseFunction(std::optional<Token> visib);
		ExprPtr parseBlock();
		ExprPtr parseExpression();
		ExprPtr parseIf();
		ExprPtr parseWhile();
		ExprPtr parseLoop();
		ExprPtr parsePostfix(ExprPtr expr);
		ExprPtr parseCallExpr(ExprPtr callee);
		ExprPtr parseReturn();
		bool parseIsVarDeclRef();
		ExprPtr parseVarDecl();
		ExprPtr parseBorrow();
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
