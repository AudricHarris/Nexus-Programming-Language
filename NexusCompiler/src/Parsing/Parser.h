#ifndef Parser_H
#define Parser_H

#include "../Tokenizing/Token.h"
#include <memory>
#include <vector>

class ASTNode
{
	public:
		virtual ~ASTNode() = default;
		virtual void print(int indent = 0) const = 0;
};

class Parser
{
	private:
		std::vector<Token> tokens;
		size_t currentPos = 0;


	public:
		Parser(const std::vector<Token>& tokens)
		{
			std::unique_ptr<ASTNode> parse();
		}
};

#endif
