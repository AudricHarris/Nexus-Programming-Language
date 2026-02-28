#include "Parser.h"
#include "../Dictionary/TokenType.h"
#include "ParserError.h"

#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// -------------------------- //
//      Private methods       //
// -------------------------- //

const Token &Parser::peek() const {
  if (this->currentIndex >= this->tokens.size()) {
    static const Token EOF_TOKEN{TokenKind::TOK_EOF, "", 0, 0};
    return EOF_TOKEN;
  }
  return this->tokens[this->currentIndex];
}

const Token &Parser::peekAt(size_t offset) const {
  size_t idx = this->currentIndex + offset;
  if (idx >= this->tokens.size()) {
    static const Token EOF_TOKEN{TokenKind::TOK_EOF, "", 0, 0};
    return EOF_TOKEN;
  }
  return this->tokens[idx];
}

const Token Parser::consume() {
  if (this->currentIndex >= this->tokens.size()) {
    static const Token EOF_TOKEN{TokenKind::TOK_EOF, "", 0, 0};
    return EOF_TOKEN;
  }
  return this->tokens[this->currentIndex++];
}

bool Parser::match(TokenKind k) {
  if (this->peek().getKind() == k) {
    this->consume();
    return true;
  }
  return false;
}

bool Parser::check(TokenKind kind) const {
  return !this->isAtEnd() && peek().getKind() == kind;
}

Token Parser::expect(TokenKind kind, std::string_view errorMsg) {
  if (this->peek().getKind() == kind) {
    return this->consume();
  }

  std::string msg;
  Token tmp(kind, "", 0, 0);
  msg = "Expected : " + tmp.toString() + ", got : `" + this->peek().getWord() +
        "`";
  throw ParseError(this->peek().getLine(), this->peek().getColumn(),
                   errorMsg.empty() ? msg : std::string(errorMsg));
}

bool Parser::isAtEnd() const {
  return this->currentIndex >= this->tokens.size() ||
         this->peek().getKind() == TokenKind::TOK_EOF;
}

// -------------------------- //
//      protected methods     //
// -------------------------- //

void Parser::synchronize() {
  consume();
  while (!this->isAtEnd()) {
    if (this->peek().getKind() == TokenKind::TOK_SEMI) {
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

static bool looksLikeType(const Token &tok) {
  const std::string &w = tok.getWord();
  return w == "i32" || w == "i64" || w == "f32" || w == "f64" || w == "bool" ||
         w == "void" || w == "int" || w == "float" || w == "double" ||
         w == "long" || w == "integer" || w == "string" || w == "str";
}

// -------------------------- //
//      public methods        //
// -------------------------- //

std::unique_ptr<Program> Parser::parse() {
  auto prog = std::make_unique<Program>();

  while (!this->isAtEnd()) {
    try {
      auto func = this->parseFunctionDecl();
      prog->functions.push_back(std::move(func));
    } catch (const ParseError &e) {
      this->synchronize();
    }
  }

  return prog;
}

std::unique_ptr<Function> Parser::parseFunctionDecl() {
  Token nameToken = this->expect(TokenKind::TOK_IDENTIFIER,
                                 "Expected function name at top level");

  this->expect(TokenKind::TOK_LPAREN, "Expected '(' after function name");

  std::vector<Parameter> params;
  if (!this->match(TokenKind::TOK_RPAREN)) {
    do {
      Token typeToken =
          this->expect(TokenKind::TOK_IDENTIFIER, "Expected parameter type");
      Token nameTokenParam = this->expect(TokenKind::TOK_IDENTIFIER,
                                          "Expected parameter name after type");
      Parameter p{Identifier{typeToken}, Identifier{nameTokenParam}};
      params.push_back(std::move(p));
    } while (match(TokenKind::TOK_COMMA));

    this->expect(TokenKind::TOK_RPAREN, "Expected ')' after parameter list");
  }

  // Return types default to void
  if (this->match(TokenKind::TOK_RETURN_TYPE)) {
    Token retTypeToken =
        this->expect(TokenKind::TOK_IDENTIFIER, "expected a type");

    Identifier returnType = Identifier{retTypeToken};
    auto body = parseBlock();
    return std::make_unique<Function>(Identifier{nameToken}, std::move(params),
                                      std::move(body), returnType);
  } else {
    Identifier returnType = Identifier{
        Token{TokenKind::TOK_IDENTIFIER, "void", nameToken.getLine(), 0}};
    auto body = parseBlock();
    return std::make_unique<Function>(Identifier{nameToken}, std::move(params),
                                      std::move(body), returnType);
  }
}

std::unique_ptr<Block> Parser::parseBlock(bool uni) {
  auto block = std::make_unique<Block>();
  if (this->check(TokenKind::TOK_LBRACE)) {
    this->expect(TokenKind::TOK_LBRACE, "Expected `{` at start of block");

    while (!this->check(TokenKind::TOK_RBRACE) && !isAtEnd()) {
      try {
        auto stmt = this->parseStatement();
        if (stmt) {
          block->statements.push_back(std::move(stmt));
        }
      } catch (const ParseError &e) {
        synchronize();
      }
    }

    this->expect(TokenKind::TOK_RBRACE, "Expected `}` to close the block");
  } else if (uni) {
    try {
      auto stmt = this->parseStatement();
      if (stmt) {
        block->statements.push_back(std::move(stmt));
      }
    } catch (const ParseError &e) {
      synchronize();
    }
  }

  return block;
}

std::unique_ptr<Statement> Parser::parseStatement() {
  if (this->match(TokenKind::TOK_RETURN)) {
    return this->parseReturnStatement();
  }
  if (this->match(TokenKind::TOK_IF)) {
    return this->parseIfStatement();
  }
  if (this->match(TokenKind::TOK_WHILE)) {
    return this->parseWhileLoop();
  }

  // ── Array element assignment detection ─────────────────────────────────────
  // Handles two syntaxes:
  //   ident[index] = value;          (bare: peekAt(0)=ident, peekAt(1)=[)
  //   type ident[index] = value;     (typed: peekAt(0)=type, peekAt(1)=ident,
  //   peekAt(2)=[)
  //
  // We scan past the [...] to find the closing ] then check for an assign op.
  {
    // Determine starting offset of "ident [" within lookahead
    size_t identOff = 0; // offset of the array-name identifier token
    bool isTypedArrayAssign = false;

    if (peekAt(0).getKind() == TokenKind::TOK_IDENTIFIER &&
        peekAt(1).getKind() == TokenKind::TOK_LBRACKET) {
      // bare: ident[
      identOff = 0;
    } else if (looksLikeType(peekAt(0)) &&
               peekAt(1).getKind() == TokenKind::TOK_IDENTIFIER &&
               peekAt(2).getKind() == TokenKind::TOK_LBRACKET) {
      // typed: type ident[
      identOff = 1;
      isTypedArrayAssign = true;
    }

    if (identOff == 0 || isTypedArrayAssign) {
      // Scan past the [ ... ] starting at identOff+1
      size_t k = identOff + 2; // first token inside [
      int depth = 1;
      while (depth > 0 && this->currentIndex + k < this->tokens.size()) {
        TokenKind kk = peekAt(k).getKind();
        if (kk == TokenKind::TOK_LBRACKET)
          ++depth;
        else if (kk == TokenKind::TOK_RBRACKET)
          --depth;
        if (depth > 0)
          ++k;
        else
          break;
      }
      // peekAt(k) = closing ], peekAt(k+1) = operator
      TokenKind opAfter = peekAt(k + 1).getKind();
      if (opAfter == TokenKind::TOK_ASSIGN || opAfter == TokenKind::TOK_MOVE ||
          opAfter == TokenKind::TOK_BORROW) {
        if (isTypedArrayAssign)
          this->consume(); // discard the leading type token
        return this->parseArrayElementAssignStatement();
      }
    }
  }

  if (looksLikeType(peekAt(0))) {
    bool isArrayType = peekAt(1).getKind() == TokenKind::TOK_LBRACKET &&
                       peekAt(2).getKind() == TokenKind::TOK_RBRACKET &&
                       peekAt(3).getKind() == TokenKind::TOK_IDENTIFIER;

    bool isScalarType = peekAt(1).getKind() == TokenKind::TOK_IDENTIFIER;

    size_t assignOffset = isArrayType ? 4 : 2;
    TokenKind nextOp = peekAt(assignOffset).getKind();

    if ((isArrayType || isScalarType)) {
      if (nextOp == TokenKind::TOK_ASSIGN) {
        return this->parseVarDeclStatement(AssignKind::Copy);
      }
      if (nextOp == TokenKind::TOK_MOVE) {
        return this->parseVarDeclStatement(AssignKind::Move);
      }
      if (nextOp == TokenKind::TOK_BORROW) {
        return this->parseVarDeclStatement(AssignKind::Borrow);
      }
      if (nextOp == TokenKind::TOK_SEMI) {
        throw ParseError(this->peek().getLine(), this->peek().getColumn(),
                         "Cannot have a null object, Var decl must have value");
      }
    }
  }

  auto expr = this->parseExpression();
  this->expect(TokenKind::TOK_SEMI, "Expected ';' after expression statement");
  return std::make_unique<ExprStmt>(std::move(expr));
}

// Parse: identifier[index] = expr;  (array element assignment as a statement)
std::unique_ptr<Statement> Parser::parseArrayElementAssignStatement() {
  Token arrTok = this->consume(); // identifier
  this->expect(TokenKind::TOK_LBRACKET, "Expected '[' for array index");
  auto index = this->parseExpression();
  this->expect(TokenKind::TOK_RBRACKET, "Expected ']' after index");

  // consume the assignment operator (we only really need '=' for array elem)
  this->consume(); // TOK_ASSIGN / TOK_MOVE / TOK_BORROW

  auto value = this->parseExpression();
  this->expect(TokenKind::TOK_SEMI,
               "Expected ';' after array element assignment");

  Identifier arrIdent{arrTok};
  auto assignExpr = std::make_unique<ArrayIndexAssignExpr>(
      arrIdent, std::move(index), std::move(value));
  return std::make_unique<ExprStmt>(std::move(assignExpr));
}

std::unique_ptr<IfStmt> Parser::parseIfStatement() {
  this->expect(TokenKind::TOK_LPAREN, "Expected '(' to start comparison");
  auto condition = this->parseExpression();
  this->expect(TokenKind::TOK_RPAREN, "Expected ')' after comparison");
  auto thenBlock = this->parseBlock(true);

  std::unique_ptr<Block> elseBlock = nullptr;
  if (this->match(TokenKind::TOK_ElSE)) {
    if (this->match(TokenKind::TOK_IF)) {
      auto elseIf = this->parseIfStatement();
      elseBlock = std::make_unique<Block>();
      elseBlock->statements.push_back(std::move(elseIf));
    } else {
      elseBlock = this->parseBlock(true);
    }
  }

  return std::make_unique<IfStmt>(std::move(condition), std::move(thenBlock),
                                  std::move(elseBlock));
}

std::unique_ptr<WhileStmt> Parser::parseWhileLoop() {
  this->expect(TokenKind::TOK_LPAREN, "Expected '(' to start comparison");
  auto condition = this->parseExpression();
  this->expect(TokenKind::TOK_RPAREN, "Expected ')' after comparison");
  auto doBlock = this->parseBlock();

  return std::make_unique<WhileStmt>(std::move(condition), std::move(doBlock));
}

std::unique_ptr<Return> Parser::parseReturnStatement() {
  auto ret = std::make_unique<Return>();

  if (this->match(TokenKind::TOK_SEMI)) {
    return ret;
  }

  ret->value = this->parseExpression();
  this->expect(TokenKind::TOK_SEMI, "Expected ';' after return value");
  return ret;
}

std::unique_ptr<VarDecl> Parser::parseVarDeclStatement(AssignKind kind) {
  Token firstTok = this->consume();

  if (this->peek().getKind() == TokenKind::TOK_LBRACKET) {
    this->expect(TokenKind::TOK_LBRACKET, "Expected '['");
    this->expect(TokenKind::TOK_RBRACKET, "Expected ']'");

    ArrayType arrType(Identifier{firstTok});
    Token nameTok =
        this->expect(TokenKind::TOK_IDENTIFIER, "Expected variable name");

    bool moved = false;
    switch (kind) {
    case AssignKind::Copy:
      this->expect(TokenKind::TOK_ASSIGN, "Expected '=' after variable name");
      break;
    case AssignKind::Move:
      this->expect(TokenKind::TOK_MOVE, "Expected '<-' after variable name");
      moved = true;
      break;
    case AssignKind::Borrow:
      this->expect(TokenKind::TOK_BORROW, "Expected '&=' after variable name");
      break;
    }

    auto init = this->parseExpression();
    this->expect(TokenKind::TOK_SEMI,
                 "Expected ';' after variable declaration");

    return std::make_unique<VarDecl>(arrType, Identifier{nameTok},
                                     std::move(init), kind, moved);
  } else {
    Token nameTok =
        this->expect(TokenKind::TOK_IDENTIFIER, "Expected variable name");
    bool moved = false;
    switch (kind) {
    case AssignKind::Copy:
      this->expect(TokenKind::TOK_ASSIGN, "Expected '=' after variable name");
      break;
    case AssignKind::Move:
      this->expect(TokenKind::TOK_MOVE, "Expected '<-' after variable name");
      moved = true;
      break;
    case AssignKind::Borrow:
      this->expect(TokenKind::TOK_BORROW, "Expected '&=' after variable name");
      break;
    }

    auto init = this->parseExpression();
    this->expect(TokenKind::TOK_SEMI,
                 "Expected ';' after variable declaration");

    return std::make_unique<VarDecl>(Identifier{firstTok}, Identifier{nameTok},
                                     std::move(init), kind, moved);
  }
}

std::unique_ptr<Expression> Parser::parseExpression() {
  return parseAssignment();
}

std::unique_ptr<Expression> Parser::parsePrimary() {
  Token tok = consume();

  switch (tok.getKind()) {
  case TokenKind::TOK_INT: {
    return std::make_unique<IntLitExpr>(IntLitExpr{IntegerLiteral{tok}});
  }

  case TokenKind::TOK_FLOAT: {
    return std::make_unique<FloatLitExpr>(FloatLitExpr{FloatLiteral{tok}});
  }
  case TokenKind::TOK_NEW: {
    this->currentIndex--;
    return parseNewArray();
  }
  case TokenKind::TOK_STRING: {
    return std::make_unique<StrLitExpr>(StrLitExpr{StringLiteral{tok}});
  }

  case TokenKind::TOK_CHAR: {
    return std::make_unique<CharLitExpr>(CharLitExpr{CharLiteral{tok}});
  }

  case TokenKind::TOK_BOOL: {
    return std::make_unique<BoolLitExpr>(BoolLitExpr{BoolLiteral{tok}});
  }

  case TokenKind::TOK_IDENTIFIER: {
    Identifier id{tok};

    if (match(TokenKind::TOK_LPAREN)) {
      auto call =
          std::make_unique<CallExpr>(CallExpr{id, std::vector<ExprPtr>()});

      if (!match(TokenKind::TOK_RPAREN)) {
        do {
          call->arguments.push_back(parseExpression());
        } while (match(TokenKind::TOK_COMMA));
        expect(TokenKind::TOK_RPAREN, "Expected ')' after call arguments");
      }
      return call;
    }

    return std::make_unique<IdentExpr>(IdentExpr{id});
  }

  default:
    throw ParseError(tok.getLine(), tok.getColumn(),
                     "Expected expression, got `" + tok.getWord() + "`");
  }
}

std::unique_ptr<Expression> Parser::parseNewArray() {
  this->expect(TokenKind::TOK_NEW, "Expected 'new' for array allocation");

  Token elemTypeTok =
      this->expect(TokenKind::TOK_IDENTIFIER, "Expected element type");
  this->expect(TokenKind::TOK_LBRACKET, "Expected '[' after array type");
  this->expect(TokenKind::TOK_RBRACKET, "Expected ']' after array type");

  ArrayType arrType(Identifier{elemTypeTok});

  this->expect(TokenKind::TOK_LPAREN, "Expected '(' after array type");
  auto size = parseExpression();
  this->expect(TokenKind::TOK_RPAREN, "Expected ')' after array size");

  return std::make_unique<NewArrayExpr>(arrType, std::move(size));
}

std::unique_ptr<Expression> Parser::parseAssignment() {
  auto left = this->parseEquality(); // full precedence chain: equality →
                                     // comparison → additive → ...

  if (match(TokenKind::TOK_ASSIGN)) {
    auto value = parseAssignment();
    if (auto *id = dynamic_cast<IdentExpr *>(left.get())) {
      return std::make_unique<AssignExpr>(id->name, std::move(value),
                                          AssignKind::Copy);
    }
    if (auto *ai = dynamic_cast<ArrayIndexExpr *>(left.get())) {
      return std::make_unique<ArrayIndexAssignExpr>(
          ai->array, std::move(ai->index), std::move(value));
    }
    throw ParseError(
        peek().getLine(), peek().getColumn(),
        "Left-hand side of assignment must be an identifier or array index");
  }

  if (match(TokenKind::TOK_MOVE)) {
    auto value = parseAssignment();
    if (auto *id = dynamic_cast<IdentExpr *>(left.get())) {
      return std::make_unique<AssignExpr>(id->name, std::move(value),
                                          AssignKind::Move);
    }
    throw ParseError(peek().getLine(), peek().getColumn(),
                     "Left-hand side of move assignment must be an identifier");
  }

  if (match(TokenKind::TOK_BORROW)) {
    auto value = parseAssignment();
    if (auto *id = dynamic_cast<IdentExpr *>(left.get())) {
      return std::make_unique<AssignExpr>(id->name, std::move(value),
                                          AssignKind::Borrow);
    }
    throw ParseError(
        peek().getLine(), peek().getColumn(),
        "Left-hand side of borrow assignment must be an identifier");
  }

  return left;
}

std::unique_ptr<Expression> Parser::parseEquality() {
  auto expr = this->parseComparison();

  while (true) {
    if (this->match(TokenKind::TOK_EQ)) {
      auto right = this->parseComparison();
      expr = std::make_unique<BinaryExpr>(BinaryOp::Eq, std::move(expr),
                                          std::move(right));
    } else if (this->match(TokenKind::TOK_NE)) {
      auto right = this->parseComparison();
      expr = std::make_unique<BinaryExpr>(BinaryOp::Ne, std::move(expr),
                                          std::move(right));
    } else {
      break;
    }
  }
  return expr;
}

std::unique_ptr<Expression> Parser::parseComparison() {
  auto expr = this->parseAdditive();

  while (true) {
    if (this->match(TokenKind::TOK_LT)) {
      auto right = this->parseAdditive();
      expr = std::make_unique<BinaryExpr>(BinaryOp::Lt, std::move(expr),
                                          std::move(right));
    } else if (this->match(TokenKind::TOK_GT)) {
      auto right = this->parseAdditive();
      expr = std::make_unique<BinaryExpr>(BinaryOp::Gt, std::move(expr),
                                          std::move(right));
    } else if (this->match(TokenKind::TOK_LE)) {
      auto right = this->parseAdditive();
      expr = std::make_unique<BinaryExpr>(BinaryOp::Le, std::move(expr),
                                          std::move(right));
    } else if (this->match(TokenKind::TOK_GE)) {
      auto right = this->parseAdditive();
      expr = std::make_unique<BinaryExpr>(BinaryOp::Ge, std::move(expr),
                                          std::move(right));
    } else {
      break;
    }
  }

  return expr;
}

std::unique_ptr<Expression> Parser::parseAdditive() {
  auto expr = parseMultiplicative();

  while (true) {
    if (match(TokenKind::TOK_ADD)) {
      auto right = parseMultiplicative();
      // Emit as BinaryExpr with Add; CodeGen will detect string + anything
      expr = std::make_unique<BinaryExpr>(BinaryOp::Add, std::move(expr),
                                          std::move(right));
    } else if (match(TokenKind::TOK_SUB)) {
      auto right = parseMultiplicative();
      expr = std::make_unique<BinaryExpr>(BinaryOp::Sub, std::move(expr),
                                          std::move(right));
    } else {
      break;
    }
  }

  return expr;
}

std::unique_ptr<Expression> Parser::parseMultiplicative() {
  auto expr = parseUnary();

  while (true) {
    if (match(TokenKind::TOK_PROD)) {
      auto right = parseUnary();
      expr = std::make_unique<BinaryExpr>(BinaryOp::Mul, std::move(expr),
                                          std::move(right));
    } else if (match(TokenKind::TOK_DIV_FLOOR)) {
      auto right = parseUnary();
      expr = std::make_unique<BinaryExpr>(BinaryOp::DivFloor, std::move(expr),
                                          std::move(right));
    } else if (match(TokenKind::TOK_DIV)) {
      auto right = parseUnary();
      expr = std::make_unique<BinaryExpr>(BinaryOp::Div, std::move(expr),
                                          std::move(right));
    } else if (match(TokenKind::TOK_MOD)) {
      auto right = parseUnary();
      expr = std::make_unique<BinaryExpr>(BinaryOp::Mod, std::move(expr),
                                          std::move(right));
    } else {
      break;
    }
  }

  return expr;
}

std::unique_ptr<Expression> Parser::parseUnary() {
  if (match(TokenKind::TOK_SUB)) {
    auto operand = parseUnary();
    return std::make_unique<UnaryExpr>(UnaryOp::Negate, std::move(operand));
  }
  return parsePostfix();
}

std::unique_ptr<Expression> Parser::parsePostfix() {
  auto expr = parsePrimary();

  if (match(TokenKind::TOK_LBRACKET)) {
    auto index = parseExpression();
    this->expect(TokenKind::TOK_RBRACKET, "Expected ']' after index");

    if (auto *id = dynamic_cast<IdentExpr *>(expr.get())) {
      return std::make_unique<ArrayIndexExpr>(id->name, std::move(index));
    }
    throw ParseError(peek().getLine(), peek().getColumn(),
                     "Array indexing requires an identifier");
  }

  if (match(TokenKind::TOK_DOT)) {
    Token prop =
        this->expect(TokenKind::TOK_IDENTIFIER, "Expected property name");
    if (prop.getWord() == "length") {
      if (auto *id = dynamic_cast<IdentExpr *>(expr.get())) {
        return std::make_unique<ArrayLengthExpr>(id->name);
      }
      throw ParseError(peek().getLine(), peek().getColumn(),
                       "Property access requires an identifier");
    }
    throw ParseError(peek().getLine(), peek().getColumn(),
                     "Unknown property: " + prop.getWord());
  }

  if (match(TokenKind::TOK_INCREMENT)) {
    if (auto *id = dynamic_cast<IdentExpr *>(expr.get())) {
      return std::make_unique<Increment>(id->name);
    }
    throw ParseError(peek().getLine(), peek().getColumn(),
                     "++ can only follow an identifier");
  }
  if (match(TokenKind::TOK_DECREMENT)) {
    if (auto *id = dynamic_cast<IdentExpr *>(expr.get())) {
      return std::make_unique<Decrement>(id->name);
    }
    throw ParseError(peek().getLine(), peek().getColumn(),
                     "-- can only follow an identifier");
  }

  return expr;
}
