#ifndef TOKEN_H
#define TOKEN_H

#include <string>
#include <cstdint>
#include <variant>
#include <optional>
#include <iostream>

enum class TokenKind : uint16_t {
    kw_private,
    kw_public,
    kw_class,
    kw_match,
    kw_Constructor,
    kw_Options,
    kw_Some,
    kw_None,
    kw_self,
    kw_new,
    kw_while,
    kw_true,

    // Types
    type_i32,
    type_f64,
    type_name,

    // Identifiers
    identifier,

    // Literals
    lit_integer,
    lit_float,
    lit_string,
    lit_bool,

    // Operators & special punctuation
    op_assign,
    op_move,
	op_arrow,
    op_plus,
    op_minus,
    op_mult,
    op_div,
    op_int_div,
    op_mod,
    op_dot,
    op_and_equal,

    op_eq, op_neq, op_lt, op_gt, op_le, op_ge,

    // Delimiters
    delim_lbrace, delim_rbrace,
    delim_lparen, delim_rparen,
    delim_semicolon,
    delim_comma,
    delim_colon,
    delim_arrow,

    // Special
    comment,
    whitespace,
    eof,
    unknown,

    NUM_TOKENS
}

;struct TokenInfo {
	const char* name;
	const char* spelling;
	int precedence;
	bool isLeftAssociative;
	bool isKeyword;
	bool isOperator;
	bool isDelimiter;
	bool isLiteral;
};

class TokenTable {
	private:
		static TokenInfo table[];

	public:
		static const TokenInfo& getInfo(TokenKind kind);
		static bool isKeyword(TokenKind k)     { return getInfo(k).isKeyword; }
		static bool isOperator(TokenKind k)    { return getInfo(k).isOperator; }
		static int  getPrecedence(TokenKind k) { return getInfo(k).precedence; }

		static std::optional<TokenKind> getKindFromSpelling(const std::string& s);
};

class Token {
	private:
		TokenKind kind;
		std::string lexeme;
		size_t line;
		size_t column;

		std::variant<
			std::monostate,
			int64_t,
			double,
			std::string,
			bool
		> value;

	public:
		Token(TokenKind k, std::string lex, size_t ln, size_t col)
			: kind(k), lexeme(std::move(lex)), line(ln), column(col) {}

		TokenKind getKind()     const { return kind; }
		const std::string& getLexeme() const { return lexeme; }
		size_t getLine()        const { return line; }
		size_t getColumn()      const { return column; }

		const TokenInfo& getInfo() const { return TokenTable::getInfo(kind); }
		const char* getName() const {
    		const char* n = getInfo().name;
    		return n ? n : "<unnamed_token>";
		}

		std::string getNameSafe() const {
    		const char* n = getInfo().name;
    		if (n) return n;
    		return "<missing_name kind=" + std::to_string(static_cast<uint16_t>(kind)) + ">";
		}

		bool isKeyword()        const { return getInfo().isKeyword; }
		bool isOperator()       const { return getInfo().isOperator; }
		int  getPrecedence()    const { return getInfo().precedence; }

		std::string toString() const {
    		return getNameSafe() + " '" + lexeme + "' at " +
           		std::to_string(line) + ":" + std::to_string(column);
		}

};

#endif
