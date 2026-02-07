#ifndef TokenType
#define TokenType

/*
 * Let's start with the basics and expand on that
 * We need the simplest program or a small program to get an idea on how to
 * build this
 *
 * Example test.nx :
 * Main()
 * {
 *  	i32 num = 5;
 *  	num++;
 *		/! I am testing code !/
 *		Printf("This is a test {num}");
 *		i32 num2 = num;
 *		/! num is now invalid if called compiler will scream !/
 *		return;
 * }
 *
 * Main -> KW_Main
 * (    -> PUNC_lParam
 * )    -> PUNC_rParam
 * {    -> PUNC_oBody   OBody for open body
 *
 * i32 -> TYPE_i32
 * num -> KW_name
 * =   -> OP_assign
 * 5   -> LITTERAL_int
 * ;   -> PUNC_semi
 *
 * num -> KW_name
 * ++  -> OP_increment
 * ;   -> PUNC_semi
 *
 * /! I am testing code !/ -> COMMENT
 *
 * Printf -> KW_name
 * (      -> PUNC_lParam
 * "This is a test{num}" -> LITTERAL_string
 * )      -> PUNC_rParam
 *
 * i32  -> TYPE_i32
 * num2 -> KW_name
 * =    -> OP_move
 * num  -> KW_name
 * ;    -> PUNC_semi
 *
 * /! num is now invalid if called compiler will scream !/ -> COMMENT
 *
 * return -> KW_return;
 * ;      -> PUNC_semi
 *
 * } -> PUNC_cBody
 *
 * This manual parsing allows me to identify different types
 * Example
 * variable/param = TYPE_X followed by KW_name
 * Method = KW_name + PUNC_lParam + (param)* + PUNC_rParam + body
 */

#endif
