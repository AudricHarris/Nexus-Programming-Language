#ifndef AST_HPP
#define AST_HPP

#include "../Token/TokenType.hpp"

// Expressions -> This is everything that returns a type (Example : [1+5]
// returns int, FunctionCalls(), string concat, etc...)

struct Expression {
  virtual ~Expression() = default;
};

// ALl the different default litterals :
// int, float, bool, char, str

enum class LiteralKind { Int, Float, Bool, Char, Str };
enum class NumericBase { Decimal, Hexadecimal, Octal, Binary };

struct LiteralExpr : Expression {
  LiteralKind kind;
  Token lit;

  NumericBase base = NumericBase::Decimal;
};

// Statements  -> This is
// all the code that results in an action I believe
struct Statement {
  virtual ~Statement() = default;
};

#endif // AST_H
