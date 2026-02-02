#include <cstdint>

enum class TokenType : uint16_t {
	// Keywords
	kw_class,
	kw_public,
	kw_private,
	kw_protected,
	kw_match,
	kw_Options,
	kw_Some,
	kw_None,
	kw_self,
	kw_new,
	kw_return,
	kw_print,
	kw_printf,

	// Types
	type_i4,
	type_i8,
 	type_i16,
	type_i32,
	type_i64,
	type_128,
	type_256,

}
