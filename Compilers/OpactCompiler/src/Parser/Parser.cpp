#include "Parser/Parser.hpp"
#include "Parser/Ast.hpp"
#include "Token/TokenType.hpp"
#include <exception>
#include <iostream>
#include <memory>
#include <optional>

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
        std::cout << "It's a import\n";
        this->consume();
        return nullptr;
    }
    // Detect keyword function
    if (this->check(TokenKind::FN))
    {
        std::cout << "It's a function\n";
        this->consume();
        if (visibility.has_value())
        {
            Token t = visibility.value();
            Token name = this->consume();
            std::cout << "\tVisibility : " << t.toString() << "\n";
            std::cout << "\tName : " << name.getWord() << "\n";
        }
        return nullptr;
    }
    // Detect keyword Class/Struct
    // Default To global

    this->consume();
    return nullptr;
}
