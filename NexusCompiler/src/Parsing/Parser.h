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

class ParseNode 
{
	public:
		ParseNode *left;
		ParseNode *right;

		ParseNode(ParseNode *left = 0, ParseNode *right = 0) : left(left), right(right) {}
		virtual ~ParseNode() {}

		virtual Type GetType() { return UNKNOWNVAL; }

		virtual Value Eval(map<string,Value>& symb)
		{
			if( left ) left->Eval(symb);
			if( right ) right->Eval(symb);
			return Value();
		}

		virtual void RunStaticChecks(map<string,bool>& idMap) 
		{
			if( left )
				left->RunStaticChecks(idMap);
			if( right )
				right->RunStaticChecks(idMap);
		}
};

class runtimeError {
	public:
		runtimeError() {
		cout << "RUNTIME ERROR: ";
		}
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
