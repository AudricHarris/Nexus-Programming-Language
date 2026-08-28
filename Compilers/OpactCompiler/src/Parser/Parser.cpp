#include "CompilerPipeline.hpp"
#include "Parser/Parser.hpp"
#include "Parser/Ast.hpp"
#include "Token/TokenType.hpp"
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

//----------------------//
//-- Token Navigation --//
//----------------------//

const Token &Parser::peek() const {
	if (this->idx >= this->tokens.size()) {
		static const Token EOF_TOKEN{TokenKind::END_OF_FILE, "", 0, 0};
		return EOF_TOKEN;
	}
	return this->tokens[this->idx];
}

const Token &Parser::peekAt(size_t offset) const {
	size_t idx = this->idx + offset;
	if (idx >= this->tokens.size()) {
		static const Token EOF_TOKEN{TokenKind::END_OF_FILE, "", 0, 0};
		return EOF_TOKEN;
	}
	return this->tokens[idx];
}

const Token Parser::consume() {
	if (this->idx >= this->tokens.size()) {
		static const Token EOF_TOKEN{TokenKind::END_OF_FILE, "", 0, 0};
		return EOF_TOKEN;
	}
	return this->tokens[this->idx++];
}

bool Parser::match(TokenKind k) {
	if (peek().getKind() == k) {
		consume();
		return true;
	}
	return false;
}

bool Parser::check(TokenKind kind) const {
	return !this->isAtEnd() && this->peek().getKind() == kind;
}

bool Parser::isAtEnd() const {
	return this->idx >= this->tokens.size() ||
		this->peek().getKind() == TokenKind::END_OF_FILE;
}

//--------------------//
//-- Error handling --//
//--------------------//

std::string Parser::generateError(TokenKind kind, std::string errorMsg)
{
	std::string formattedError = "\033[31m[Parse Error] Line " + 
		std::to_string(peek().getLine()) + ":" + 
		std::to_string(peek().getColumn()) + 
		" - " + errorMsg + "\033[0m";

	return formattedError;
}

Token Parser::expect(TokenKind kind, std::string_view errorMsg) {
	if (peek().getKind() == kind)
		return consume();
	std::string msg;
	if (errorMsg.empty()) {
		Token tmp(kind, "", 0, 0);
		msg = "Expected: " + tmp.toString() + ", got: `" + peek().getWord() + "`";
	} else {
		msg = std::string(errorMsg);
	}

	std::string formattedError = this->generateError(kind, msg); 

	std::cout << formattedError << "\n";
	throw std::runtime_error(formattedError);
}

void Parser::synchronize() {
	consume();
	while (!isAtEnd()) {
		if (peek().getKind() == TokenKind::SEMI) {
			consume();
			return;
		}
		switch (peek().getKind()) {
			case TokenKind::RETURN:
				return;
			case TokenKind::LBRACE:
				consume();
				return;
			default:
				consume();
		}
	}
}
//--------------------//
//-- Parsing module --//
//--------------------//

ExprPtr Parser::parseModule()
{
	auto moduleBlock = std::make_unique<Block>();

	// while we aren't at the end Go through token list
	while (!this->isAtEnd())
	{
		try {
			// Top level is either one of the following :
			// - Import
			// - Global
			// - Struct/class 
			// - function, etc... 
			ExprPtr item = this->parseTopLevel();
			if (item != nullptr)
				moduleBlock->expressions.push_back(std::move(item));
		}
		catch (const std::exception& e)
		{
			this->synchronize();
		}
	}

	return moduleBlock;
}


ExprPtr Parser::parseTopLevel()
{
	std::optional<Token> visibility;
	if (this->check(TokenKind::PUBLIC) || this->check(TokenKind::PRIVATE))
		visibility = this->consume();

	// Detect keyword Import
	if (this->check(TokenKind::IMPORT))
	{
		this->parseImport();
		return nullptr;
	}
	// Detect keyword function
	if (this->check(TokenKind::FN))
	{
		this->parseFunction(visibility); 
		return nullptr;
	}
	// Detect keyword Class/Struct
	// Default To global

	this->consume();
	return nullptr;
}

//--------------------//
//-- Parsing Import --//
//--------------------//

void Parser::addModule(std::vector<std::string> path)
{
	std::string actualPath = "";
	for (std::string word : path)
	{
		actualPath.append(word);
		actualPath.append("/");
	}

	actualPath.erase(actualPath.size() - 1 );
	actualPath.append(".op");

	this->pipeline->enqueueFile(actualPath, this->filePath);
}

ExprPtr Parser::parseImport()
{
	// Import keyword
	this->consume();

	auto importDecl = std::make_unique<ImportExpr>();

	Token first = this->expect(TokenKind::IDENTIFIER, "Expected Module name");
	importDecl->path.segments.push_back(first.getWord());
	importDecl->path.isStdLib = (first.getWord() == "Opact" || first.getWord() == "Std");

	// While we Have more colons meaning extra path or symbols
	while ( this->check(TokenKind::COLON_COLON))
	{
		this->consume();
		// Either Symbol if brace or segment of path
		if (this->check(TokenKind::LBRACE))
		{
			this->consume();
			importDecl->isSelectiveImport = true;
			if (!this->check(TokenKind::RBRACE))
			{
				do {
					Token sym = this->expect(TokenKind::IDENTIFIER, "Expected symbol name");
					importDecl->importedSymbols.push_back(sym.getWord());
				} while (this->match(TokenKind::COMMA));

			}
			this->expect(TokenKind::RBRACE, "Expeced '}'");
			break;
		}

		// Concluded this was part of the path not symbol

		Token seg = this->expect(TokenKind::IDENTIFIER, "Expected module path segment");
		importDecl->path.segments.push_back(seg.getWord());
	}

	// This Will call a function that adds it to the module queue
	this->addModule(importDecl->path.segments);

	this->expect(TokenKind::SEMI, "Expected ';' after import");
	return importDecl;
}

//----------------------//
//-- Parsing Function --//
//----------------------//

ExprPtr Parser::parseFunction(std::optional<Token> visib)
{
	// Process 
	auto fnDecl = std::make_unique<FunctionDeclExpr>();
	// 1st keyword fn 
	this->consume();
	if (visib.has_value())
	{
		Token t = visib.value();
		if (t.getKind() == TokenKind::PUBLIC)
			fnDecl->isPublic = true;
	}

	// 2nd name
	Token name = this->expect(TokenKind::IDENTIFIER, "Expected identifier as name");
	fnDecl->name = name.getWord();

	// Then we do the params
	this->expect(TokenKind::LPAREN, "Expected a '(' for opening functions params");
	if (!this->check(TokenKind::RPAREN))
	{
		do
		{
			// Check for mutable and reference
			bool isMut = false, isRef = false;
			if (this->check(TokenKind::AND))
			{
				isRef = true;
				this->consume();
				if (this->check(TokenKind::MUT))
				{
					isMut = true;
					this->consume();
				}
			}
			// Type (for now we will consider a type as a identifier the typechecker will do heavy lifting)
			// In the long run this won't be true bc of arguments like Array<T>
			Token ptype = this->expect(TokenKind::IDENTIFIER, "Expected type for the param");
			// IDENTIFIER
			Token pname = this->expect(TokenKind::IDENTIFIER, "Expected name for the param");
			//Default value
			if (this->check(TokenKind::EQ))
			{
				this->consume();

			}
			Parameter p;

			p.name = pname.getWord();
			p.typeName = ptype.getWord();
			p.isMutable = isMut;
			p.isReference = isRef;


			fnDecl->params.push_back(std::move(p));
		}
		while (this->check(TokenKind::COMMA));
	}

	this->expect(TokenKind::RPAREN, "Expected a ')' for closing functions params");

	// Finally return type (optional)
	if (this->check(TokenKind::RETURN_TYPE))
	{
		this->consume();
		Token returnType = this->expect(TokenKind::IDENTIFIER, "Expected a return type for the function");
		fnDecl->returnTypeName = returnType.getWord();
	}

	std::cout << "Function [" << fnDecl->name << "], visibile = " 
		<< fnDecl->isPublic << "\n return type : " << fnDecl->returnTypeName << "\n";

	// Most important part the body
	this->parseBlock();

	return fnDecl;
}

//------------------//
//-- Parsing body --//
//------------------//

ExprPtr Parser::parseBlock()
{
	auto block = std::make_unique<Block>();

	// 2 cases either A it has braces so multiple expressions or b it doesn't
	if ( this->check(TokenKind::LBRACE) )
	{
		// Multiple expression
		this->consume();
		while ( this->check(TokenKind::RBRACE))
		{
			// this is a block of stmt
			try {
				this->parseExpression();
			} catch (std::exception e) {
				std::cerr << e.what() << "\n";
				this->synchronize();
			}
		}
	}
	else
	{
		// Single Expression
		try {
			this->parseExpression();
		} catch (std::exception e) {
			std::cerr << e.what() << "\n";
			this->synchronize();
		}
	}

	return block;
}

//------------------------//
//-- Parsing expression --//
//------------------------//

ExprPtr Parser::parseExpression()
{
	ExprPtr expr;
	this->parsePrimary();
	return expr;
}

ExprPtr Parser::parsePrimary()
{
	Token tok = this->consume();

	switch (tok.getKind()) {
		case TokenKind::LIT_INT:
			{
				// Important step identify the numeral type
				// Word contains first 2 leters Ox, 0b or 0o do the check
				std::string number = tok.getWord();
				std::cout << number << "\n";
				NumericBase type = NumericBase::Decimal;
				if (number.rfind("0x",0))
				{
					std::cout << "Hexadecimal\n";
					type = NumericBase::Hexadecimal;
				}
				if (number.rfind("0b",0))
					type = NumericBase::Binary;
				if (number.rfind("0o",0))
					type = NumericBase::Octal;
				
				return std::make_unique<LiteralExpr>(LiteralKind::Int, tok, type);
			}

		case TokenKind::LIT_CHAR:
			return std::make_unique<LiteralExpr>(LiteralKind::Char, tok);

		case TokenKind::LIT_FLOAT:
			return std::make_unique<LiteralExpr>(LiteralKind::Float, tok);

		case TokenKind::LIT_BOOL:
			return std::make_unique<LiteralExpr>(LiteralKind::Bool, tok);

		case TokenKind::LIT_STRING:
			{
				// Allowing Concatenation for string so "Hello, " "World!" as an example
				std::string merged = tok.getWord();
				while (this->check(TokenKind::LIT_STRING))
				{
					Token str = this->consume();
					merged += str.getWord();
				}

				Token res{TokenKind::LIT_STRING, tok.getWord(),tok.getLine(), tok.getColumn()};

				return std::make_unique<LiteralExpr>(LiteralKind::Str, res);
			}

		default: {
					 std::string error = this->generateError(
							 tok.getKind(), 
							 "Unexpected token '" + tok.getWord() + "'"
							 );
					 std::cerr << error;
					 throw std::runtime_error(error);
				 }
	}
}
