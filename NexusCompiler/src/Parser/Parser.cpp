#include "Parser.h"
#include "../Token/TokenType.h"
#include "ParserError.h"
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// ---------------- //
// Token navigation //
// ---------------- //
const Token &Parser::peek() const {
  if (currentIndex >= tokens.size()) {
    static const Token EOF_TOKEN{TokenKind::END_OF_FILE, "", 0, 0};
    return EOF_TOKEN;
  }
  return tokens[currentIndex];
}

const Token &Parser::peekAt(size_t offset) const {
  size_t idx = currentIndex + offset;
  if (idx >= tokens.size()) {
    static const Token EOF_TOKEN{TokenKind::END_OF_FILE, "", 0, 0};
    return EOF_TOKEN;
  }
  return tokens[idx];
}

const Token Parser::consume() {
  if (currentIndex >= tokens.size()) {
    static const Token EOF_TOKEN{TokenKind::END_OF_FILE, "", 0, 0};
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
         peek().getKind() == TokenKind::END_OF_FILE;
}

void Parser::synchronize() {
  consume();
  while (!isAtEnd()) {
    if (peek().getKind() == TokenKind::SEMI) {
      consume();
      return;
    }
    switch (peek().getKind()) {
    case TokenKind::RETURN:
    case TokenKind::LBRACE:
      return;
    default:
      consume();
    }
  }
}

// ---------------------- //
// Type-keyword detection //
// ---------------------- //
static bool isScalarTypeName(const std::string &w) {
  return w == "i32" || w == "i64" || w == "i16" || w == "i8" || w == "f32" ||
         w == "f64" || w == "bool" || w == "void" || w == "int" ||
         w == "integer" || w == "long" || w == "short" || w == "float" ||
         w == "double" || w == "string" || w == "str" || w == "char";
}

static bool looksLikeType(const Token &tok) {
  return tok.getKind() == TokenKind::IDENTIFIER &&
         isScalarTypeName(tok.getWord());
}

// --------------- //
// Top-level parse //
// --------------- //
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

// -------------------- //
// Function declaration //
// -------------------- //
std::unique_ptr<Function> Parser::parseFunctionDecl() {
  Token nameToken = expect(TokenKind::IDENTIFIER, "Expected function name");
  expect(TokenKind::LPAREN, "Expected '(' after function name");

  std::vector<Parameter> params;
  if (!match(TokenKind::RPAREN)) {
    do {
      bool isBorrowRef = false, isConst = false;
      if (peek().getKind() == TokenKind::AND) {
        consume();
        isBorrowRef = true;
      }
      if (peek().getKind() == TokenKind::CONST) {
        consume();
        isConst = true;
      }
      Token typeTok = expect(TokenKind::IDENTIFIER, "Expected parameter type");

      int dims = 0;
      while (peek().getKind() == TokenKind::LBRACKET &&
             peekAt(1).getKind() == TokenKind::RBRACKET) {
        consume();
        consume();
        ++dims;
      }

      Token nameTok = expect(TokenKind::IDENTIFIER, "Expected parameter name");
      params.emplace_back(TypeDesc(Identifier{typeTok}, dims, isConst),
                          Identifier{nameTok}, isBorrowRef, isConst);
    } while (match(TokenKind::COMMA));

    expect(TokenKind::RPAREN, "Expected ')'");
  }

  if (match(TokenKind::RETURN_TYPE)) {
    Token retTok = expect(TokenKind::IDENTIFIER, "Expected return type");

    int retDims = 0;
    while (peek().getKind() == TokenKind::LBRACKET &&
           peekAt(1).getKind() == TokenKind::RBRACKET) {
      consume();
      consume();
      ++retDims;
    }

    auto body = parseBlock();
    TypeDesc retTd(Identifier{retTok}, retDims, false);
    return std::make_unique<Function>(Identifier{nameToken}, std::move(params),
                                      std::move(body), std::move(retTd));
  }

  Token voidTok{TokenKind::IDENTIFIER, "void", nameToken.getLine(), 0};
  auto body = parseBlock();
  return std::make_unique<Function>(Identifier{nameToken}, std::move(params),
                                    std::move(body),
                                    TypeDesc(Identifier{voidTok}));
}

// --------- //
// Block     //
// --------- //
std::unique_ptr<Block> Parser::parseBlock(bool allowSingleStmt) {
  auto block = std::make_unique<Block>();
  if (check(TokenKind::LBRACE)) {
    expect(TokenKind::LBRACE, "Expected '{'");
    while (!check(TokenKind::RBRACE) && !isAtEnd()) {
      try {
        auto s = parseStatement();
        if (s)
          block->statements.push_back(std::move(s));
      } catch (const ParseError &) {
        synchronize();
      }
    }
    expect(TokenKind::RBRACE, "Expected '}'");
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

// ------------------ //
// Statement dispatch //
// ------------------ //
std::unique_ptr<Statement> Parser::parseStatement() {
  if (match(TokenKind::RETURN))
    return parseReturnStatement();
  if (match(TokenKind::IF))
    return parseIfStatement();
  if (match(TokenKind::WHILE))
    return parseWhileLoop();
  if (peek().getKind() == TokenKind::CONTINUE ||
      peek().getKind() == TokenKind::BREAK)
    return parseLoopBreak();

  // const var decl
  if (peek().getKind() == TokenKind::CONST)
    return parseVarDeclStatement(AssignKind::Copy);

  if (looksLikeType(peekAt(0))) {
    size_t offset = 1;
    while (peekAt(offset).getKind() == TokenKind::LBRACKET &&
           peekAt(offset + 1).getKind() == TokenKind::RBRACKET) {
      offset += 2;
    }
    if (peekAt(offset).getKind() == TokenKind::IDENTIFIER) {
      size_t opOff = offset + 1;
      TokenKind op = peekAt(opOff).getKind();
      if (op == TokenKind::ASSIGN)
        return parseVarDeclStatement(AssignKind::Copy);
      if (op == TokenKind::MOVE)
        return parseVarDeclStatement(AssignKind::Move);
      if (op == TokenKind::BORROW)
        return parseVarDeclStatement(AssignKind::Borrow);
      if (op == TokenKind::SEMI)
        throw ParseError(peek().getLine(), peek().getColumn(),
                         "Variables must be initialised at declaration");
    }
  }

  auto expr = parseExpression();
  expect(TokenKind::SEMI, "Expected ';'");
  return std::make_unique<ExprStmt>(std::move(expr));
}

// ------------------------ //
// Array assignment helpers //
// ------------------------ //
std::unique_ptr<Statement> Parser::parseArrayAssign() {
  Token arrTok = consume();
  std::vector<ExprPtr> indices;

  while (true) {
    expect(TokenKind::LBRACKET, "Expected '['");
    indices.push_back(parseExpression());
    expect(TokenKind::RBRACKET, "Expected ']'");
    if (peek().getKind() != TokenKind::LBRACKET)
      break;
  }

  consume();
  auto value = parseExpression();
  expect(TokenKind::SEMI, "Expected ';'");

  auto e = std::make_unique<ArrayIndexAssignExpr>(
      Identifier{arrTok}, std::move(indices), std::move(value));
  return std::make_unique<ExprStmt>(std::move(e));
}

// ------------ //
// Control flow //
// ------------ //
std::unique_ptr<IfStmt> Parser::parseIfStatement() {
  expect(TokenKind::LPAREN, "Expected '('");
  auto cond = parseExpression();
  expect(TokenKind::RPAREN, "Expected ')'");
  auto thenBlock = parseBlock(true);
  std::unique_ptr<Block> elseBlock;
  if (match(TokenKind::ElSE)) {
    if (match(TokenKind::IF)) {
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
  expect(TokenKind::LPAREN, "Expected '('");
  auto cond = parseExpression();
  expect(TokenKind::RPAREN, "Expected ')'");
  auto body = parseBlock();
  return std::make_unique<WhileStmt>(std::move(cond), std::move(body));
}

std::unique_ptr<Return> Parser::parseReturnStatement() {
  auto ret = std::make_unique<Return>();
  if (match(TokenKind::SEMI))
    return ret;
  ret->value = parseExpression();
  expect(TokenKind::SEMI, "Expected ';'");
  return ret;
}

std::unique_ptr<Statement> Parser::parseLoopBreak() {
  if (match(TokenKind::CONTINUE)) {
    expect(TokenKind::SEMI, "Expected ';' after continue");
    return std::make_unique<Continue>();
  }

  expect(TokenKind::BREAK, "Expected 'break' or 'continue'");
  expect(TokenKind::SEMI, "Expected ';' after break");
  return std::make_unique<Break>();
}

// -------------------- //
// Variable declaration //
// -------------------- //
std::unique_ptr<VarDecl> Parser::parseVarDeclStatement(AssignKind kind) {
  bool isConst = false;
  if (peek().getKind() == TokenKind::CONST) {
    consume();
    isConst = true;
  }

  Token typeTok = consume();
  int dims = 0;
  while (peek().getKind() == TokenKind::LBRACKET) {
    expect(TokenKind::LBRACKET, "");
    expect(TokenKind::RBRACKET, "");
    ++dims;
  }
  Token nameTok = expect(TokenKind::IDENTIFIER, "Expected variable name");

  if (match(TokenKind::ASSIGN))
    kind = AssignKind::Copy;
  else if (match(TokenKind::MOVE))
    kind = AssignKind::Move;
  else if (match(TokenKind::BORROW))
    kind = AssignKind::Borrow;
  else
    throw ParseError(peek().getLine(), peek().getColumn(),
                     "Expected '=', '<-', or '&='");

  auto init = parseExpression();
  expect(TokenKind::SEMI, "Expected ';'");

  TypeDesc td(Identifier{Token{TokenKind::IDENTIFIER, typeTok.getWord(),
                               typeTok.getLine(), typeTok.getColumn()}},
              dims, isConst);
  return std::make_unique<VarDecl>(std::move(td), Identifier{nameTok},
                                   std::move(init), kind, isConst);
}

// ---------- //
// Expression //
// ---------- //
std::unique_ptr<Expression> Parser::parseExpression() {
  return parseAssignment();
}

std::unique_ptr<Expression> Parser::parseAssignment() {
  auto left = parseOr();
  if (match(TokenKind::ASSIGN)) {
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
  if (match(TokenKind::MOVE)) {
    if (auto *id = dynamic_cast<IdentExpr *>(left.get())) {
      auto val = parseAssignment();
      return std::make_unique<AssignExpr>(id->name, std::move(val),
                                          AssignKind::Move);
    }
    throw ParseError(peek().getLine(), peek().getColumn(),
                     "'<-' requires an identifier");
  }
  if (match(TokenKind::BORROW)) {
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

std::unique_ptr<Expression> Parser::parseOr() {
  auto expr = parseAnd();
  while (match(TokenKind::OR))
    expr =
        std::make_unique<BinaryExpr>(BinaryOp::Or, std::move(expr), parseAnd());
  return expr;
}

std::unique_ptr<Expression> Parser::parseAnd() {
  auto expr = parseEquality();
  while (true) {
    if (match(TokenKind::DOUBLE_AND))
      expr = std::make_unique<BinaryExpr>(BinaryOp::And, std::move(expr),
                                          parseEquality());
    else if (match(TokenKind::AND))
      expr = std::make_unique<BinaryExpr>(BinaryOp::BitAnd, std::move(expr),
                                          parseEquality());
    else
      break;
  }
  return expr;
}

std::unique_ptr<Expression> Parser::parseEquality() {
  auto expr = parseComparison();
  while (true) {
    if (match(TokenKind::EQ)) {
      expr = std::make_unique<BinaryExpr>(BinaryOp::Eq, std::move(expr),
                                          parseComparison());
    } else if (match(TokenKind::NE)) {
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
    if (match(TokenKind::LT))
      op = BinaryOp::Lt;
    else if (match(TokenKind::GT))
      op = BinaryOp::Gt;
    else if (match(TokenKind::LE))
      op = BinaryOp::Le;
    else if (match(TokenKind::GE))
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
    if (match(TokenKind::ADD))
      expr = std::make_unique<BinaryExpr>(BinaryOp::Add, std::move(expr),
                                          parseMultiplicative());
    else if (match(TokenKind::SUB))
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
    if (match(TokenKind::PROD))
      op = BinaryOp::Mul;
    else if (match(TokenKind::DIV_FLOOR))
      op = BinaryOp::DivFloor;
    else if (match(TokenKind::DIV))
      op = BinaryOp::Div;
    else if (match(TokenKind::MOD))
      op = BinaryOp::Mod;
    else
      break;
    expr = std::make_unique<BinaryExpr>(op, std::move(expr), parseUnary());
  }
  return expr;
}

std::unique_ptr<Expression> Parser::parseUnary() {
  if (match(TokenKind::NOT))
    return std::make_unique<UnaryExpr>(UnaryOp::Not, parseUnary());

  if (match(TokenKind::SUB)) {
    return std::make_unique<UnaryExpr>(UnaryOp::Negate, parseUnary());
  }
  return parsePostfix();
}

std::unique_ptr<Expression> Parser::parsePostfix() {
  auto expr = parsePrimary();

  while (true) {
    if (match(TokenKind::LBRACKET)) {
      std::vector<ExprPtr> indices;
      do {
        indices.push_back(parseExpression());
        expect(TokenKind::RBRACKET, "Expected ']'");
      } while (match(TokenKind::LBRACKET));

      if (auto *id = dynamic_cast<IdentExpr *>(expr.get())) {
        expr = std::make_unique<ArrayIndexExpr>(id->name, std::move(indices));
      } else {
        throw ParseError(peek().getLine(), peek().getColumn(),
                         "Indexing requires an identifier");
      }
      continue; // loop back — now we can see the .length
    }

    if (match(TokenKind::DOT)) {
      Token prop = expect(TokenKind::IDENTIFIER, "Expected property name");
      if (prop.getWord() == "length") {
        if (auto *id = dynamic_cast<IdentExpr *>(expr.get()))
          return std::make_unique<LengthPropertyExpr>(id->name);
        if (auto *arr = dynamic_cast<ArrayIndexExpr *>(expr.get()))
          return std::make_unique<IndexedLengthExpr>(arr->array,
                                                     std::move(arr->indices));
      }
      throw ParseError(peek().getLine(), peek().getColumn(),
                       "Unknown property: " + prop.getWord());
    }

    if (match(TokenKind::INCREMENT)) {
      if (auto *id = dynamic_cast<IdentExpr *>(expr.get()))
        return std::make_unique<Increment>(id->name);
      throw ParseError(peek().getLine(), peek().getColumn(),
                       "'++' requires an identifier");
    }
    if (match(TokenKind::DECREMENT)) {
      if (auto *id = dynamic_cast<IdentExpr *>(expr.get()))
        return std::make_unique<Decrement>(id->name);
      throw ParseError(peek().getLine(), peek().getColumn(),
                       "'--' requires an identifier");
    }
    break;
  }
  return expr;
}

std::unique_ptr<Expression> Parser::parsePrimary() {
  Token tok = consume();
  switch (tok.getKind()) {
  case TokenKind::LIT_INT:
    return std::make_unique<IntLitExpr>(tok);
  case TokenKind::LIT_FLOAT:
    return std::make_unique<FloatLitExpr>(tok);
  case TokenKind::LIT_STRING:
    return std::make_unique<StrLitExpr>(tok);
  case TokenKind::LIT_CHAR:
    return std::make_unique<CharLitExpr>(tok);
  case TokenKind::LIT_BOOL:
    return std::make_unique<BoolLitExpr>(tok);
  case TokenKind::NEW:
    --currentIndex;
    return parseNewArray();
  case TokenKind::IDENTIFIER: {
    Identifier id{tok};
    if (match(TokenKind::LPAREN)) {
      std::vector<ExprPtr> args;
      if (!match(TokenKind::RPAREN)) {
        do {
          args.push_back(parseExpression());
        } while (match(TokenKind::COMMA));
        expect(TokenKind::RPAREN, "Expected ')'");
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
  expect(TokenKind::NEW, "Expected 'new'");
  Token elemTok = expect(TokenKind::IDENTIFIER, "Expected element type");

  std::vector<ExprPtr> sizes;
  expect(TokenKind::LBRACKET, "Expected '['");

  do {
    sizes.push_back(parseExpression());
    expect(TokenKind::RBRACKET, "Expected ']'");
    if (peek().getKind() != TokenKind::LBRACKET)
      break;
    consume();
  } while (true);

  const std::string &baseName = elemTok.getWord();
  TypeDesc td(Identifier{Token{TokenKind::IDENTIFIER, baseName,
                               elemTok.getLine(), elemTok.getColumn()}},
              sizes.size());

  return std::make_unique<NewArrayExpr>(std::move(td), std::move(sizes));
}
