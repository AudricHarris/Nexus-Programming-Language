/**
 * @file TokenType.hpp
 * @brief Starting script and main manager for compiler.
 */
#ifndef TokenType
#define TokenType

#include <string>
#include <string_view>

/**
 * @brief enum of all types in my language
 *
 * This contains all types from IDENTIFIER to FOR parsing by LIT_STRING,
 * This will most likely be expanded with the expansion of the language
 * */
enum class TokenKind {
	// KeyWords
	IDENTIFIER,
	IF,
	ELSE,
	WHILE,
	LOOP,
	FOR,
	RETURN,
	AS,
	FN,
	MUT,
	CONST,
	IMPORT,
	PUBLIC,
	PRIVATE,
	PROTECTED,
	CONTINUE,
	BREAK,
	ENUM,
	MATCH,

	// Litterals
	LIT_INT,
	LIT_FLOAT,
	LIT_STRING,
	LIT_CHAR,
	LIT_BOOL,

	// Operations
	ASSIGN,
	INCREMENT,
	DECREMENT,
	FAT_ARROW,
	BORROW,
	ADD,
	SUB,
	PROD,
	DIV,
	MOD,
	LT, //<
	GT,
	LE, // <=
	GE,
	EQ,
	NE,
	AND,
	DOUBLE_AND,
	OR,
	NOT,
	ADD_ASSIGN,
	SUB_ASSIGN,
	MUL_ASSIGN,
	DIV_ASSIGN,
	DIVF_ASSIGN,

	// Punctuation / Delimiters
	LPAREN,
	RPAREN,
	LBRACKET,
	RBRACKET,
	LBRACE,
	RBRACE,
	COMMA,
	SEMI,
	DOT,
	COLON,
	COLON_COLON,

	// SPECIAL
	RETURN_TYPE,
	NEW,
	END_OF_FILE,
	COMMENT,
	UNKNOWN,

	NUM_TOKENS
};

/**
 * @class Token : Stores the physical token
 * 
 * It contains a few attributes to extract kind of token, word it contains, the line it is at.
 * It also contains a method allowing to extract name from TokenKind
 *
 * @attributes kind : The token kind it is
 * @attributes word : physical word that the token is
 * @attributes line : the line the token is at (usefull for debugging)
 * @attributes column : the column the token is at (usefull for debugging)
 *
 * @method getKind : returns the token Kind 
 * @method getWord : returns the word
 * @method getLine : returns the line 
 * @method getColumn : returns the column
 * @method toString : returns a string format of token Kind (mostly visual)
 * */
class Token {
	private:
		TokenKind kind;
		std::string word;
		int line;
		int column;

	public:
		Token(TokenKind k, std::string_view w, int l, int c)
			: kind(k), word(w), line(l), column(c) {}

		// getters
		TokenKind getKind() const { return this->kind; }
		std::string getWord() const { return this->word; }
		int getLine() const { return this->line; }
		int getColumn() const { return this->column; }
		std::string toString();
};

#endif
