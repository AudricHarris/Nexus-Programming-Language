#ifndef PARSER_H
#define PARSER_H

#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>

// Custom
#include "../AST/AST.h"
#include "../Dictionary/TokenType.h"

// Let's start by defining Notions

/*
 * AST (Abstract syntax tree) :
 * - This is a visualization often in json to visualize how the code will run
 * - The code will have a Sequence of expression types
 *
 * Example in a educational pupose :
 * Sequence / StmtList
 * ├─ 0: Declare x : int = 5
 * ├─ 1: While (x < 20) { Assign x = x + y*2 }
 * └─ 2: Declare z : int = x * 2
 *
 * The more a code is complex the more it will be expanded
 * the goal of the parser is to take in tokens and determine how to make this
 * tree. This should allow LLVM to compute it's magic and generate the computer
 * code.
 *
 * A problem arrises :
 * - This works fine with functions but how do we set it up for classes
 * ( For my initial compiler version we will not take in account classes and
 * types)
 *
 * Once we do implement those it will probably look something like the following
 * Program
 * └─ ClassDecl ("Text.nx")
 *   ├─ Attribute Decleration
 *   │   ├─ name: "count"
 *   │   ├─ type: int
 *   │   └─ initializer: ICONST 0
 *   │
 *   └─ MethodDecl ("MethodExample")
 *        ├─ parameters
 *        │   └─ Parameter ("step", type: int)
 *        ├─ returnType: void
 *        └─ body: Block / Sequence / StmtList
 *             ├─ 0: Declare x : int = 5
 *             ├─ 1: While (x < 20)
 *             │    └─ body: Block / Sequence
 *             │         ├─ 0: Assign x = x + step * 2
 *             │         └─ 1: Assign count = count + 1
 *             └─ 2: Declare result : int = x * 2
 *
 * This makes thing noticeablyy more complex. the task of making the parser will
 * probably be the most advance in the project.
 */

class Parser {
private:
  std::vector<Token> tokens;
  size_t currentIndex = 0;
  const Token &peek() const;
  Token consume();
  bool match(TokenKind kind);
  Token expect(TokenKind kind, std::string_view errorMsg = {});
  bool isAtEnd() const;

public:
  explicit Parser(const std::vector<Token> &t) : tokens(t) {}
  std::unique_ptr<Program> parseFunctionDecl();
  std::unique_ptr<Block> parseBlock();
};

#endif
