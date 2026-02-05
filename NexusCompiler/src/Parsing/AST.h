#ifndef AST_H
#define AST_H


// Includes
#include "../Tokenizing/Token.h"
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <variant>

// Classes

class ASTVisitor;

struct Type
{
	std::string name;
	std::vector<Type> generics;
	bool isArray = false;
};

struct Param
{
	std::string name;
	Type type;
};

using ParamList = std::vector<Param>;

class Expr;
class Stmt;

using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;

class ASTNode
{
	public:
		virtual ~ASTNode() = default;
		virtual void accept(ASTVisitor& v) const = 0;
		virtual void print(int indent = 0) const = 0;
};


class Expr : public ASTNode
{
	public:
		Type type;
};

class LiteralExpr : public Expr
{
	public:
		Token token;
		std::variant<int64_t, double, std::string, bool> value;

		LiteralExpr(Token t);
		void accept(ASTVisitor& v) const override;
		void print(int indent) const override;
};

class IdentifierExpr : public Expr 
{
	public:
		std::string name;
		IdentifierExpr(std::string n, const Token& tok);
};

class BinaryExpr : public Expr
{
	public:
		ExprPtr left;
		Token op;
		ExprPtr right;
		BinaryExpr(ExprPtr l, Token o, ExprPtr r);
};

class MemberAccessExpr : public Expr
{
	public:
		ExprPtr object;
		std::string member;
		MemberAccessExpr(ExprPtr obj, std::string m);
};

class CallExpr : public Expr
{
	public:
		ExprPtr callee;
		std::vector<ExprPtr> args;
		CallExpr(ExprPtr c, std::vector<ExprPtr> a);
};

class NewExpr : public Expr
{
	public:
		Type type;
		std::vector<ExprPtr> args;
		NewExpr(Type t, std::vector<ExprPtr> a);
};

class MatchExpr : public Expr
{
	public:
		ExprPtr scrutinee;
		struct Case
		{
			std::string variantName;
			std::optional<std::string> bindName;
			StmtPtr body;
		};
		std::vector<Case> cases;
		MatchExpr(ExprPtr s, std::vector<Case> cs);
};


class Stmt : public ASTNode {};

class ExprStmt : public Stmt
{
	public:
		ExprPtr expr;
};

class ReturnStmt : public Stmt
{
	public:
		std::optional<ExprPtr> value;
};

class IfStmt : public Stmt
{
	public:
		ExprPtr condition;
		StmtPtr thenBranch;
		std::optional<StmtPtr> elseBranch;
};

class WhileStmt : public Stmt
{
	public:
		ExprPtr condition;
		StmtPtr body;
};

class BlockStmt : public Stmt
{
	public:
		std::vector<StmtPtr> statements;
};


class Decl : public ASTNode {};

class VarDecl : public Decl
{
	public:
		bool isGlobal = false;
		std::string name;
		std::optional<Type> type;
		ExprPtr initializer;
};

class SumTypeDecl : public Decl 
{
	public:
		std::string name;
		std::vector<std::string> generics;
		struct Variant
		{
			std::string name;
			std::optional<Type> payload;
	};
	std::vector<Variant> variants;
};

class MethodDecl : public Decl
{
	public:
		bool isPublic = true;
		bool isConstructor = false;
		std::string name;
		ParamList params;
		Type returnType;
		StmtPtr body;
};

class ClassDecl : public Decl
{
	public:
		bool isPublic = true;
		std::string name;
		std::vector<std::unique_ptr<VarDecl>> fields;
		std::vector<std::unique_ptr<MethodDecl>> methods;
};

class Program : public ASTNode
{
	public:
		std::vector<std::unique_ptr<Decl>> declarations;
		void accept(ASTVisitor& v) const override;
		void print(int indent) const override;
};

#endif
