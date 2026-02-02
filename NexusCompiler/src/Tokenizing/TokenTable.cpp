#include "Token.h"
#include <unordered_map>
#include <algorithm>

TokenInfo TokenTable::table[static_cast<size_t>(TokenKind::NUM_TOKENS)] = {
	// Keywords
	{"kw_private",     "private",   0, true, true, false, false, false},
	{"kw_public",      "public",    0, true, true, false, false, false},
	{"kw_class",       "class",     0, true, true, false, false, false},
	{"kw_match",       "match",     0, true, true, false, false, false},
	{"kw_Constructor", "Constructor",0,true, true, false, false, false},
	{"kw_Options",     "Options",   0, true, true, false, false, false},
	{"kw_Some",        "Some",      0, true, true, false, false, false},
	{"kw_None",        "None",      0, true, true, false, false, false},
	{"kw_self",        "self",      0, true, true, false, false, false},
	{"kw_new",         "new",       0, true, true, false, false, false},
	{"kw_while",       "while",     0, true, true, false, false, false},
	{"kw_true",        "true",      0, true, true, false, false, false},

	// Types
	{"type_i32",       "i32",       0, true, true, false, false, false},
	{"type_f64",       "f64",       0, true, true, false, false, false},
	{"type_name",      nullptr,     0, true, true, false, false, false}, // generics/user types

	{"identifier",     nullptr,     0, true, false,false,false,false},

	{"lit_integer",    nullptr,     0, true, false,false,false,true},
	{"lit_float",      nullptr,     0, true, false,false,false,true},
	{"lit_string",     nullptr,     0, true, false,false,false,true},
	{"lit_bool",       nullptr,     0, true, false,false,false,true},

	// Operators
	{"op_assign",      "=",        10, true, false,true, false,false},
	{"op_move",        "<-",       10, true, false,true, false,false},
	{"op_arrow",        "->",       10, true, false,true, false,false},
	{"op_plus",        "+",         7, true, false,true, false,false},
	{"op_minus",       "-",         7, true, false,true, false,false},
	{"op_mult",        "*",         8, true, false,true, false,false},
	{"op_div",         "/",         8, true, false,true, false,false},
	{"op_int_div",     "//",        8, true, false,true, false,false},
	{"op_mod",         "%",         8, true, false,true, false,false},
	{"op_dot",         ".",        16, true, false,true, false,false},
	{"op_and_equal",   "&=",       10, true, false,true, false,false},  // tmp &= ...

	{"op_eq",          "==",        6, true, false,true, false,false},
	// ... add others as needed

	// Delimiters & special arrows
	{"delim_lbrace",   "{",         0, true, false,false,true, false},
	{"delim_rbrace",   "}",         0, true, false,false,true, false},
	{"delim_lparen",   "(",         0, true, false,false,true, false},
	{"delim_rparen",   ")",         0, true, false,false,true, false},
	{"delim_semicolon",";",         0, true, false,false,true, false},
	{"delim_comma",    ",",         0, true, false,false,true, false},
	{"delim_colon",    ":",         0, true, false,false,true, false},
	{"delim_arrow",    "=>",        0, true, false,false,true, false},  // match / while

	{"comment",        nullptr,     0, true, false,false,false,false},
	{"whitespace",     nullptr,     0, true, false,false,false,false},
	{"eof",            "<EOF>",     0, true, false,false,false,false},
	{"unknown",        "<UNKNOWN>", 0, true, false,false,false,false},
};

const TokenInfo& TokenTable::getInfo(TokenKind kind) {
    return table[static_cast<size_t>(kind)];
}

std::optional<TokenKind> TokenTable::getKindFromSpelling(const std::string& s) {
	static const std::unordered_map<std::string, TokenKind> keywordMap = {
		{"private",     TokenKind::kw_private},
		{"public",      TokenKind::kw_public},
		{"class",       TokenKind::kw_class},
		{"match",       TokenKind::kw_match},
		{"Constructor", TokenKind::kw_Constructor},
		{"Options",     TokenKind::kw_Options},
		{"Some",        TokenKind::kw_Some},
		{"None",        TokenKind::kw_None},
		{"self",        TokenKind::kw_self},
		{"new",         TokenKind::kw_new},
		{"while",       TokenKind::kw_while},
		{"true",        TokenKind::kw_true},
		{"i32",         TokenKind::type_i32},
		{"f64",         TokenKind::type_f64},
		// add more as your language grows
	};

	auto it = keywordMap.find(s);
	if (it != keywordMap.end()) return it->second;
	return std::nullopt;
}
