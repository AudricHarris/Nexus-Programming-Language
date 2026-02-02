#include <cstdint>

enum class TokenType : uint16_t
{
	// Keywords
	kw_class,
	kw_public,
	kw_private,
	kw_protected,
	kw_match,
	kw_options,
	kw_some,
	kw_none,
	kw_self,
	kw_new,
	kw_return,
	kw_print,
	kw_printf,

	// Types
	// - Intergers
	type_i4,
	type_i8,
	type_i16,
	type_i32,
	type_i64,
	type_i128,
	type_i256,

	// - Float
	type_f8,
	type_f16,
	type_f32,
	type_f64,
	type_f128,
	type_f256

	// -other
	type_bool,
	type_char,
	type_string,
	type_name, // User defined

	// Identifier
	identifier,

	//Litterals
	lit_bool,
	lit_char,
	lit_string,
	lit_integer,
	lit_float,

	// Operators
	op_assign,
	op_move,
	op_plus,
	op_minus,
	op_mult,
	op_div,
	op_int_div,
	op_mod,
	op_dot,
	op_arrow,
	op_eq,
	op_neq,
	op_lt,
	op_gt,
	op_le,
	op_ge,
	
	// Delimiters
	delim_lbrace,
	delim_rbrace,
	delim_lparen,
	delim_rparen,
	delim_lbracket,
	delim_rbracket,
	delim_semicolon,
	delim_colon,
	delim_comma,
	delim_pipe,
	
	// Special
	comment,
	whitespace,
	eof,
	unknown,
	
	// Count (for array sizing)
	NUM_TOKENS

}

// Might change in the future
struct TokenInfo
{
	const char* name;
	const char* spelling;
	int precedence;
	bool isLeftAssociative;
	bool isKeyword;
	bool isOperator;
	bool isDelimiter;
	bool isLiteral;
}

class TokenTable 
{
	private:
		static TokenInfo table[];

	public:
		static const TokenInfo& getInfo(TokenType type)
		{
			return table[static_cast<size_t>(type)];
		}
		
		static bool isKeyword(TokenType type)
		{
			return getInfo(type).isKeyword;
		}

		static bool isOperator(TokenType type)
		{
			return getInfo(type).isOperator;
		}

		static bool getPrecedence(TokenType type)
		{
			return getInfo(type).precedence;
		}
}
