#include "Parser.h"
#include "../Dictionary/TokenType.h"
#include "ParserError.h"
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// Token navigation
// ─────────────────────────────────────────────────────────────────────────────

const Token &Parser::peek() const {
  if (currentIndex >= tokens.size()) {
    static const Token EOF_TOKEN{TokenKind::TOK_EOF, "", 0, 0};
    return EOF_TOKEN;
  }
  return tokens[currentIndex];
}

const Token &Parser::peekAt(size_t offset) const {
  size_t idx = currentIndex + offset;
  if (idx >= tokens.size()) {
    static const Token EOF_TOKEN{TokenKind::TOK_EOF, "", 0, 0};
    return EOF_TOKEN;
  }
  return tokens[idx];
}

const Token Parser::consume() {
  if (currentIndex >= tokens.size()) {
    static const Token EOF_TOKEN{TokenKind::TOK_EOF, "", 0, 0};
    return EOF_TOKEN;
  }
  return tokens[currentIndex++];
}

bool Parser::match(TokenKind k) {
  if (peek().getKind() == k) {
    consume();
    return true;
  }
  return false;
}

bool Parser::check(TokenKind kind) const {
  return !isAtEnd() && peek().getKind() == kind;
}

Token Parser::expect(TokenKind kind, std::string_view errorMsg) {
  if (peek().getKind() == kind)
    return consume();
  std::string msg;
  if (errorMsg.empty()) {
    Token tmp(kind, "", 0, 0);
    msg = "Expected: " + tmp.toString() + ", got: `" + peek().getWord() + "`";
  } else {
    msg = std::string(errorMsg);
  }
  throw ParseError(peek().getLine(), peek().getColumn(), msg);
}

bool Parser::isAtEnd() const {
  return currentIndex >= tokens.size() ||
         peek().getKind() == TokenKind::TOK_EOF;
}

void Parser::synchronize() {
  consume();
  while (!isAtEnd()) {
    if (peek().getKind() == TokenKind::TOK_SEMI) {
      consume();
      return;
    }
    switch (peek().getKind()) {
    case TokenKind::TOK_RETURN:
    case TokenKind::TOK_LBRACE:
      return;
    default:
      consume();
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Type-keyword detection
// ─────────────────────────────────────────────────────────────────────────────

static bool isScalarTypeName(const std::string &w) {
  return w == "i32" || w == "i64" || w == "i16" || w == "i8" || w == "f32" ||
         w == "f64" || w == "bool" || w == "void" || w == "int" ||
         w == "integer" || w == "long" || w == "short" || w == "float" ||
         w == "double" || w == "string" || w == "str";
}

static bool looksLikeType(const Token &tok) {
  return tok.getKind() == TokenKind::TOK_IDENTIFIER &&
         isScalarTypeName(tok.getWord());
}

// ─────────────────────────────────────────────────────────────────────────────
// Top-level parse
// ─────────────────────────────────────────────────────────────────────────────

std::unique_ptr<Program> Parser::parse() {
  auto prog = std::make_unique<Program>();
  while (!isAtEnd()) {
    try {
      prog->functions.push_back(parseFunctionDecl());
    } catch (const ParseError &) {
      synchronize();
    }
  }
  return prog;
}

// ─────────────────────────────────────────────────────────────────────────────
// Function declaration
// ─────────────────────────────────────────────────────────────────────────────

std::unique_ptr<Function> Parser::parseFunctionDecl() {
  Token nameToken = expect(TokenKind::TOK_IDENTIFIER, "Expected function name");
  expect(TokenKind::TOK_LPAREN, "Expected '(' after function name");

  std::vector<Parameter> params;
  if (!match(TokenKind::TOK_RPAREN)) {
    do {
      bool isBorrowRef = false, isConst = false;
      if (peek().getKind() == TokenKind::TOK_AND) {
        consume();
        isBorrowRef = true;
      }
      if (peek().getKind() == TokenKind::TOK_CONST) {
        consume();
        isConst = true;
      }
      Token typeTok =
          expect(TokenKind::TOK_IDENTIFIER, "Expected parameter type");
      Token nameTok =
          expect(TokenKind::TOK_IDENTIFIER, "Expected parameter name");
      params.emplace_back(TypeDesc(Identifier{typeTok}), Identifier{nameTok},
                          isBorrowRef, isConst);
    } while (match(TokenKind::TOK_COMMA));
    expect(TokenKind::TOK_RPAREN, "Expected ')'");
  }

  if (match(TokenKind::TOK_RETURN_TYPE)) {
    Token retTok = expect(TokenKind::TOK_IDENTIFIER, "Expected return type");
    auto body = parseBlock();
    return std::make_unique<Function>(Identifier{nameToken}, std::move(params),
                                      std::move(body),
                                      TypeDesc(Identifier{retTok}));
  }
  Token voidTok{TokenKind::TOK_IDENTIFIER, "void", nameToken.getLine(), 0};
  auto body = parseBlock();
  return std::make_unique<Function>(Identifier{nameToken}, std::move(params),
                                    std::move(body),
                                    TypeDesc(Identifier{voidTok}));
}

// ─────────────────────────────────────────────────────────────────────────────
// Block
// ─────────────────────────────────────────────────────────────────────────────

std::unique_ptr<Block> Parser::parseBlock(bool allowSingleStmt) {
  auto block = std::make_unique<Block>();
  if (check(TokenKind::TOK_LBRACE)) {
    expect(TokenKind::TOK_LBRACE, "Expected '{'");
    while (!check(TokenKind::TOK_RBRACE) && !isAtEnd()) {
      try {
        auto s = parseStatement();
        if (s)
          block->statements.push_back(std::move(s));
      } catch (const ParseError &) {
        synchronize();
      }
    }
    expect(TokenKind::TOK_RBRACE, "Expected '}'");
  } else if (allowSingleStmt) {
    try {
      auto s = parseStatement();
      if (s)
        block->statements.push_back(std::move(s));
    } catch (const ParseError &) {
      synchronize();
    }
  }
  return block;
}

// ─────────────────────────────────────────────────────────────────────────────
// Statement dispatch
// ─────────────────────────────────────────────────────────────────────────────

std::unique_ptr<Statement> Parser::parseStatement() {
  if (match(TokenKind::TOK_RETURN))
    return parseReturnStatement();
  if (match(TokenKind::TOK_IF))
    return parseIfStatement();
  if (match(TokenKind::TOK_WHILE))
    return parseWhileLoop();

  // const var decl
  if (peek().getKind() == TokenKind::TOK_CONST)
    return parseVarDeclStatement(AssignKind::Copy);

  if (looksLikeType(peekAt(0))) {
    size_t offset = 1;
    while (peekAt(offset).getKind() == TokenKind::TOK_LBRACKET &&
           peekAt(offset + 1).getKind() == TokenKind::TOK_RBRACKET) {
      offset += 2;
    }
    if (peekAt(offset).getKind() == TokenKind::TOK_IDENTIFIER) {
      size_t opOff = offset + 1;
      TokenKind op = peekAt(opOff).getKind();
      if (op == TokenKind::TOK_ASSIGN)
        return parseVarDeclStatement(AssignKind::Copy);
      if (op == TokenKind::TOK_MOVE)
        return parseVarDeclStatement(AssignKind::Move);
      if (op == TokenKind::TOK_BORROW)
        return parseVarDeclStatement(AssignKind::Borrow);
      if (op == TokenKind::TOK_SEMI)
        throw ParseError(peek().getLine(), peek().getColumn(),
                         "Variables must be initialised at declaration");
    }
  }

  auto expr = parseExpression();
  expect(TokenKind::TOK_SEMI, "Expected ';'");
  return std::make_unique<ExprStmt>(std::move(expr));
}

// ─────────────────────────────────────────────────────────────────────────────
// Array assignment helpers
// ─────────────────────────────────────────────────────────────────────────────

std::unique_ptr<Statement> Parser::parseArrayAssign() {
  Token arrTok = consume(); // array name
  std::vector<ExprPtr> indices;

  while (true) {
    expect(TokenKind::TOK_LBRACKET, "Expected '['");
    indices.push_back(parseExpression());
    expect(TokenKind::TOK_RBRACKET, "Expected ']'");
    if (peek().getKind() != TokenKind::TOK_LBRACKET)
      break;
  }

  consume(); // assignment operator (= or += etc. – adjust if you support more)
  auto value = parseExpression();
  expect(TokenKind::TOK_SEMI, "Expected ';'");

  auto e = std::make_unique<ArrayIndexAssignExpr>(
      Identifier{arrTok}, std::move(indices), std::move(value));
  return std::make_unique<ExprStmt>(std::move(e));
}
// ─────────────────────────────────────────────────────────────────────────────
// Control flow
// ─────────────────────────────────────────────────────────────────────────────

std::unique_ptr<IfStmt> Parser::parseIfStatement() {
  expect(TokenKind::TOK_LPAREN, "Expected '('");
  auto cond = parseExpression();
  expect(TokenKind::TOK_RPAREN, "Expected ')'");
  auto thenBlock = parseBlock(true);
  std::unique_ptr<Block> elseBlock;
  if (match(TokenKind::TOK_ElSE)) {
    if (match(TokenKind::TOK_IF)) {
      auto ei = parseIfStatement();
      elseBlock = std::make_unique<Block>();
      elseBlock->statements.push_back(std::move(ei));
    } else {
      elseBlock = parseBlock(true);
    }
  }
  return std::make_unique<IfStmt>(std::move(cond), std::move(thenBlock),
                                  std::move(elseBlock));
}

std::unique_ptr<WhileStmt> Parser::parseWhileLoop() {
  expect(TokenKind::TOK_LPAREN, "Expected '('");
  auto cond = parseExpression();
  expect(TokenKind::TOK_RPAREN, "Expected ')'");
  auto body = parseBlock();
  return std::make_unique<WhileStmt>(std::move(cond), std::move(body));
}

std::unique_ptr<Return> Parser::parseReturnStatement() {
  auto ret = std::make_unique<Return>();
  if (match(TokenKind::TOK_SEMI))
    return ret;
  ret->value = parseExpression();
  expect(TokenKind::TOK_SEMI, "Expected ';'");
  return ret;
}

// ─────────────────────────────────────────────────────────────────────────────
// Variable declaration:  [const] T [[][]] name (= | <- | &=) expr ;
// ─────────────────────────────────────────────────────────────────────────────

std::unique_ptr<VarDecl> Parser::parseVarDeclStatement(AssignKind kind) {
  bool isConst = false;
  if (peek().getKind() == TokenKind::TOK_CONST) {
    consume();
    isConst = true;
  }

  Token typeTok = consume();
  int dims = 0;
  while (peek().getKind() == TokenKind::TOK_LBRACKET) {
    expect(TokenKind::TOK_LBRACKET, "");
    expect(TokenKind::TOK_RBRACKET, "");
    ++dims;
  }
  Token nameTok = expect(TokenKind::TOK_IDENTIFIER, "Expected variable name");

  if (match(TokenKind::TOK_ASSIGN))
    kind = AssignKind::Copy;
  else if (match(TokenKind::TOK_MOVE))
    kind = AssignKind::Move;
  else if (match(TokenKind::TOK_BORROW))
    kind = AssignKind::Borrow;
  else
    throw ParseError(peek().getLine(), peek().getColumn(),
                     "Expected '=', '<-', or '&='");

  auto init = parseExpression();
  expect(TokenKind::TOK_SEMI, "Expected ';'");

  TypeDesc td(Identifier{Token{TokenKind::TOK_IDENTIFIER, typeTok.getWord(),
                               typeTok.getLine(), typeTok.getColumn()}},
              dims, isConst);
  return std::make_unique<VarDecl>(std::move(td), Identifier{nameTok},
                                   std::move(init), kind, isConst);
}

// ─────────────────────────────────────────────────────────────────────────────
// Expressions  (precedence climbing)
// ─────────────────────────────────────────────────────────────────────────────

std::unique_ptr<Expression> Parser::parseExpression() {
  return parseAssignment();
}

std::unique_ptr<Expression> Parser::parseAssignment() {
  auto left = parseEquality();
  if (match(TokenKind::TOK_ASSIGN)) {
    if (auto *id = dynamic_cast<IdentExpr *>(left.get())) {
      auto val = parseAssignment();
      return std::make_unique<AssignExpr>(id->name, std::move(val),
                                          AssignKind::Copy);
    }
    if (auto *ai = dynamic_cast<ArrayIndexExpr *>(left.get())) {
      auto val = parseAssignment();
      return std::make_unique<ArrayIndexAssignExpr>(
          ai->array, std::move(ai->indices), std::move(val));
    }
    throw ParseError(peek().getLine(), peek().getColumn(),
                     "Invalid assignment target");
  }
  if (match(TokenKind::TOK_MOVE)) {
    if (auto *id = dynamic_cast<IdentExpr *>(left.get())) {
      auto val = parseAssignment();
      return std::make_unique<AssignExpr>(id->name, std::move(val),
                                          AssignKind::Move);
    }
    throw ParseError(peek().getLine(), peek().getColumn(),
                     "'<-' requires an identifier");
  }
  if (match(TokenKind::TOK_BORROW)) {
    if (auto *id = dynamic_cast<IdentExpr *>(left.get())) {
      auto val = parseAssignment();
      return std::make_unique<AssignExpr>(id->name, std::move(val),
                                          AssignKind::Borrow);
    }
    throw ParseError(peek().getLine(), peek().getColumn(),
                     "'&=' requires an identifier");
  }
  return left;
}

std::unique_ptr<Expression> Parser::parseEquality() {
  auto expr = parseComparison();
  while (true) {
    if (match(TokenKind::TOK_EQ)) {
      expr = std::make_unique<BinaryExpr>(BinaryOp::Eq, std::move(expr),
                                          parseComparison());
    } else if (match(TokenKind::TOK_NE)) {
      expr = std::make_unique<BinaryExpr>(BinaryOp::Ne, std::move(expr),
                                          parseComparison());
    } else
      break;
  }
  return expr;
}

std::unique_ptr<Expression> Parser::parseComparison() {
  auto expr = parseAdditive();
  while (true) {
    BinaryOp op;
    if (match(TokenKind::TOK_LT))
      op = BinaryOp::Lt;
    else if (match(TokenKind::TOK_GT))
      op = BinaryOp::Gt;
    else if (match(TokenKind::TOK_LE))
      op = BinaryOp::Le;
    else if (match(TokenKind::TOK_GE))
      op = BinaryOp::Ge;
    else
      break;
    expr = std::make_unique<BinaryExpr>(op, std::move(expr), parseAdditive());
  }
  return expr;
}

std::unique_ptr<Expression> Parser::parseAdditive() {
  auto expr = parseMultiplicative();
  while (true) {
    if (match(TokenKind::TOK_ADD))
      expr = std::make_unique<BinaryExpr>(BinaryOp::Add, std::move(expr),
                                          parseMultiplicative());
    else if (match(TokenKind::TOK_SUB))
      expr = std::make_unique<BinaryExpr>(BinaryOp::Sub, std::move(expr),
                                          parseMultiplicative());
    else
      break;
  }
  return expr;
}

std::unique_ptr<Expression> Parser::parseMultiplicative() {
  auto expr = parseUnary();
  while (true) {
    BinaryOp op;
    if (match(TokenKind::TOK_PROD))
      op = BinaryOp::Mul;
    else if (match(TokenKind::TOK_DIV_FLOOR))
      op = BinaryOp::DivFloor;
    else if (match(TokenKind::TOK_DIV))
      op = BinaryOp::Div;
    else if (match(TokenKind::TOK_MOD))
      op = BinaryOp::Mod;
    else
      break;
    expr = std::make_unique<BinaryExpr>(op, std::move(expr), parseUnary());
  }
  return expr;
}

std::unique_ptr<Expression> Parser::parseUnary() {
  if (match(TokenKind::TOK_SUB)) {
    return std::make_unique<UnaryExpr>(UnaryOp::Negate, parseUnary());
  }
  return parsePostfix();
}

std::unique_ptr<Expression> Parser::parsePostfix() {
  auto expr = parsePrimary();

  // arr[i], arr[i][j], arr[i][j][k], ... — any number of dimensions
  if (match(TokenKind::TOK_LBRACKET)) {
    std::vector<ExprPtr> indices;
    do {
      indices.push_back(parseExpression());
      expect(TokenKind::TOK_RBRACKET, "Expected ']'");
    } while (match(TokenKind::TOK_LBRACKET));

    if (auto *id = dynamic_cast<IdentExpr *>(expr.get())) {
      return std::make_unique<ArrayIndexExpr>(id->name, std::move(indices));
    }
    throw ParseError(peek().getLine(), peek().getColumn(),
                     "Indexing requires an identifier");
  }

  // .length or .text
  if (match(TokenKind::TOK_DOT)) {
    Token prop = expect(TokenKind::TOK_IDENTIFIER, "Expected property name");
    if (auto *id = dynamic_cast<IdentExpr *>(expr.get())) {
      if (prop.getWord() == "length")
        return std::make_unique<LengthPropertyExpr>(id->name);
    }
    throw ParseError(peek().getLine(), peek().getColumn(),
                     "Unknown property: " + prop.getWord());
  }
  if (match(TokenKind::TOK_INCREMENT)) {
    if (auto *id = dynamic_cast<IdentExpr *>(expr.get()))
      return std::make_unique<Increment>(id->name);
    throw ParseError(peek().getLine(), peek().getColumn(),
                     "'++' requires an identifier");
  }
  if (match(TokenKind::TOK_DECREMENT)) {
    if (auto *id = dynamic_cast<IdentExpr *>(expr.get()))
      return std::make_unique<Decrement>(id->name);
    throw ParseError(peek().getLine(), peek().getColumn(),
                     "'--' requires an identifier");
  }
  return expr;
}

std::unique_ptr<Expression> Parser::parsePrimary() {
  Token tok = consume();
  switch (tok.getKind()) {
  case TokenKind::TOK_INT:
    return std::make_unique<IntLitExpr>(tok);
  case TokenKind::TOK_FLOAT:
    return std::make_unique<FloatLitExpr>(tok);
  case TokenKind::TOK_STRING:
    return std::make_unique<StrLitExpr>(tok);
  case TokenKind::TOK_CHAR:
    return std::make_unique<CharLitExpr>(tok);
  case TokenKind::TOK_BOOL:
    return std::make_unique<BoolLitExpr>(tok);
  case TokenKind::TOK_NEW:
    --currentIndex;
    return parseNewArray();
  case TokenKind::TOK_IDENTIFIER: {
    Identifier id{tok};
    if (match(TokenKind::TOK_LPAREN)) {
      std::vector<ExprPtr> args;
      if (!match(TokenKind::TOK_RPAREN)) {
        do {
          args.push_back(parseExpression());
        } while (match(TokenKind::TOK_COMMA));
        expect(TokenKind::TOK_RPAREN, "Expected ')'");
      }
      return std::make_unique<CallExpr>(id, std::move(args));
    }
    return std::make_unique<IdentExpr>(id);
  }
  default:
    throw ParseError(tok.getLine(), tok.getColumn(),
                     "Unexpected token: `" + tok.getWord() + "`");
  }
}

std::unique_ptr<Expression> Parser::parseNewArray() {
  expect(TokenKind::TOK_NEW, "Expected 'new'");
  Token elemTok = expect(TokenKind::TOK_IDENTIFIER, "Expected element type");

  std::vector<ExprPtr> sizes;
  expect(TokenKind::TOK_LBRACKET, "Expected '['");

  do {
    sizes.push_back(parseExpression());
    expect(TokenKind::TOK_RBRACKET, "Expected ']'");
    if (peek().getKind() != TokenKind::TOK_LBRACKET)
      break;
    consume(); // next [
  } while (true);

  const std::string &baseName = elemTok.getWord();
  TypeDesc td(Identifier{Token{TokenKind::TOK_IDENTIFIER, baseName,
                               elemTok.getLine(), elemTok.getColumn()}},
              sizes.size());

  return std::make_unique<NewArrayExpr>(std::move(td), std::move(sizes));
}
