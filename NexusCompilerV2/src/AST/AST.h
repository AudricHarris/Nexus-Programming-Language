#ifndef AST_H
#define AST_H

enum class NodeType {
  Program,
  FunctionDef,
  VarDecl,
  Assignement,
  Incremement,
  CallExpr,
  ReturnStmt,
  Identifier,
  IntegerLiteral,
  StringLiteral,
};
/*
 *
 Parsing should give us this :
Main() -> NodeType::FunctionDef
{
        i32 num = 5008526; --> NodeType::VarDecl  + NodeType::IntegerLiteral
        num++; --> NodeType::Increment
        Printf("This is a test {num}"); --> NodeType::CallExpr +
NodeType::StringLiteral

        i32 num2 = num; --> NodeType::Assignement
        return; --> NodeType::ReturnStmt
}
*/
#endif // !AST_H
