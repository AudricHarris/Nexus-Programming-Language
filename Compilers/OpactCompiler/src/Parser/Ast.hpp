#ifndef AST_HPP
#define AST_HPP

#include "../Token/TokenType.hpp"
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Type representation :

enum class DataType { Int, Float, Bool, Char, Str, Void, Custom };

struct TypeDesc {
  DataType type;
  std::optional<Token> customName;
  std::vector<int> dim;

  bool isArray() const { return !dim.empty(); }
};

// Base nodes :

// Expressions -> This is everything that returns a type (Example : [1+5]
// returns int, FunctionCalls(), string concat, etc...)
struct Expression {
  virtual ~Expression() = default;
};

// all the code that results in an action I believe
// Avoids writing std::unique_ptr makes things cleaner
using ExprPtr = std::unique_ptr<Expression>;

// ALl the different default litterals :
// int, float, bool, char, str

enum class LiteralKind { Int, Float, Bool, Char, Str };
enum class NumericBase { Decimal, Hexadecimal, Octal, Binary };

struct LiteralExpr : Expression {
  LiteralKind kind;
  Token lit;

  NumericBase base = NumericBase::Decimal;
};

struct IdentifierExpr : Expression {
  Token name;
};

// Operation
enum class BinaryOp {
  Add,
  Sub,
  Mul,
  Div,
  Mod,
  Eq,
  Ne,
  Lt,
  Gt,
  Le,
  Ge,
  And,
  BitAnd,
  Or
};

enum class UnaryOp { Negate, Not };

struct BinaryExpr : Expression {
  BinaryOp op;
  ExprPtr left;
  ExprPtr right;
};

struct UnaryExpr : Expression {
  UnaryOp op;
  ExprPtr expr;
};

struct CallExpr : Expression {
  ExprPtr callee;
  std::vector<ExprPtr> arguments;
};

struct GenericCallExpr : Expression {
  ExprPtr callee;
  std::vector<TypeDesc> typeArgs;
  std::vector<ExprPtr> arguments;
};

struct CastExpr : Expression {
  ExprPtr expr;
  TypeDesc castType;
};

enum class AssignKind { Assign, Move, Borrow };

struct AssignExpr : Expression {
  ExprPtr target;
  ExprPtr value;
  AssignKind kind;
};

struct VarDeclExpr : Expression {
  Token name;
  std::optional<TypeDesc> type;
  ExprPtr expr;
};

struct YieldExpr : Expression {
  ExprPtr expr;
};

struct Block : Expression {
  std::vector<ExprPtr> expressions;
};

struct IfExpr : Expression {
  ExprPtr condition;
  std::unique_ptr<Block> ifBranch;
  std::optional<std::unique_ptr<Block>> elseBranch;
};

struct WhileExpr : Expression {
  ExprPtr condition;
  std::unique_ptr<Block> loopBranch;
};

struct MemberAccessExpr : Expression {
  ExprPtr object;
  Token memberName;
};

struct IndexExpr : Expression {
  ExprPtr array;
  ExprPtr index;
};

struct ArrayLiteralExpr : Expression {
  std::vector<ExprPtr> elements;
};

struct TupleLiteralExpr : Expression {
  std::vector<ExprPtr> elements;
};

struct BreakExpr : Expression {
  std::optional<ExprPtr> value;
};

struct ReturnExpr : Expression {
  std::optional<ExprPtr> value;
};

struct ContinueExpr : Expression {};

// Import

struct ImportPath {
    std::vector<std::string> segments;
    bool isStdLib;
};

struct ImportExpr : Expression {
    ImportPath path; 
    std::vector<std::string> importedSymbols;
    bool isSelectiveImport;
};

// Functions & params
struct Parameter {
    std::string name;
    std::string typeName;
    bool isMutable = false;
    bool isReference = false;
    std::optional<ExprPtr> defaultVal = nullptr;
};

struct FunctionDeclExpr : Expression {
    std::string name;
    std::vector<Parameter> params;
    std::string returnTypeName;
    ExprPtr body;
    bool isPublic = true;
    bool isStatic = false;
};

#endif // AST_HPP
