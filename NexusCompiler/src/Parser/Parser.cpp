#include "Parser.h"
#include "../Token/TokenType.h"
#include "ParserError.h"
#include <iostream>
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

bool Parser::isIdentWord(std::string_view word) const {
  return peek().getKind() == TokenKind::IDENTIFIER && peek().getWord() == word;
}

// ----------------------------------------- //
// Type-keyword helpers                       //
// ----------------------------------------- //
static bool isScalarTypeName(const std::string &w) {
  return w == "i32" || w == "i64" || w == "i16" || w == "i8" || w == "f32" ||
         w == "f64" || w == "bool" || w == "void" || w == "int" ||
         w == "integer" || w == "long" || w == "short" || w == "float" ||
         w == "double" || w == "string" || w == "str" || w == "char" ||
         w == "ptr";
}

static bool looksLikeType(const Token &tok) {
  return tok.getKind() == TokenKind::IDENTIFIER;
}

std::unique_ptr<Program> Parser::parse() {
  auto prog = std::make_unique<Program>();

  while (!isAtEnd()) {
    try {
      if (check(TokenKind::IMPORT)) {
        prog->imports.push_back(parseImportDecl());
        continue;
      }

      {
        size_t offset = 0;
        if (check(TokenKind::PUBLIC) || check(TokenKind::PRIVATE))
          offset = 1;
        if (peekAt(offset).getKind() == TokenKind::IDENTIFIER &&
            peekAt(offset).getWord() == "extern") {
          if (offset == 1)
            consume();
          prog->externBlocks.push_back(parseExternBlock());
          continue;
        }
      }

      bool isPublic = false;
      if (check(TokenKind::PUBLIC)) {
        consume();
        isPublic = true;
      } else if (check(TokenKind::PRIVATE)) {
        consume();
      }

      if (isIdentWord("struct")) {
        auto s = parseStructDecl();
        s->isPublic = isPublic;
        prog->structs.push_back(std::move(s));
        continue;
      }

      bool nextIsConst = check(TokenKind::CONST);
      const Token &typeCheck = nextIsConst ? peekAt(1) : peek();
      if (looksLikeType(typeCheck)) {
        size_t typeOff = nextIsConst ? 1 : 0;
        if (peekAt(typeOff + 1).getKind() == TokenKind::IDENTIFIER &&
            peekAt(typeOff + 2).getKind() != TokenKind::LPAREN) {
          auto gv = parseGlobalVarDecl();
          gv->isPublic = isPublic;
          prog->globals.push_back(std::move(gv));
          continue;
        }
      }

      auto fn = parseFunctionDecl();
      fn->isPublic = isPublic;
      prog->functions.push_back(std::move(fn));

    } catch (const ParseError &e) {
      std::cerr << e.what() << "\n";
      synchronize();
    }
  }

  return prog;
}

ExternBlock Parser::parseExternBlock() {
  consume();

  if (peek().getKind() != TokenKind::LIT_STRING || peek().getWord() != "C") {
    throw ParseError(peek().getLine(), peek().getColumn(),
                     "Expected \"C\" after extern");
  }
  consume();

  expect(TokenKind::LBRACE, "Expected '{' after extern \"C\"");

  ExternBlock block;

  while (!check(TokenKind::RBRACE) && !isAtEnd()) {
    bool declPrivate = false;

    if (check(TokenKind::PRIVATE)) {
      consume();
      declPrivate = true;
    } else if (check(TokenKind::PUBLIC)) {
      consume();
    }

    Token nameTok =
        expect(TokenKind::IDENTIFIER, "Expected function name in extern block");
    expect(TokenKind::LPAREN, "Expected '('");

    std::vector<TypeDesc> paramTypes;
    if (!check(TokenKind::RPAREN)) {
      do {
        Token typeTok =
            expect(TokenKind::IDENTIFIER, "Expected parameter type");
        if (check(TokenKind::IDENTIFIER))
          consume();
        paramTypes.emplace_back(Identifier{typeTok});
      } while (match(TokenKind::COMMA));
    }
    expect(TokenKind::RPAREN, "Expected ')'");

    Token voidTok{TokenKind::IDENTIFIER, "void", nameTok.getLine(), 0};
    TypeDesc retType{Identifier{voidTok}};
    if (match(TokenKind::RETURN_TYPE)) {
      Token retTok = expect(TokenKind::IDENTIFIER, "Expected return type");
      retType = TypeDesc{Identifier{retTok}};
    }

    expect(TokenKind::SEMI, "Expected ';'");
    block.decls.emplace_back(nameTok.getWord(), std::move(paramTypes),
                             std::move(retType), declPrivate);
  }

  expect(TokenKind::RBRACE, "Expected '}' to close extern block");
  return block;
}

// ------------------- //
// Struct declaration  //
// ------------------- //
std::unique_ptr<StructDecl> Parser::parseStructDecl() {
  consume();
  Token nameTok = expect(TokenKind::IDENTIFIER, "Expected struct name");
  expect(TokenKind::LBRACE, "Expected '{'");

  auto decl = std::make_unique<StructDecl>();
  decl->name = nameTok.getWord();

  while (!check(TokenKind::RBRACE) && !isAtEnd()) {
    Token typeTok = expect(TokenKind::IDENTIFIER, "Expected field type");

    int dims = 0;
    while (peek().getKind() == TokenKind::LBRACKET &&
           peekAt(1).getKind() == TokenKind::RBRACKET) {
      consume();
      consume();
      ++dims;
    }

    Token fieldTok = expect(TokenKind::IDENTIFIER, "Expected field name");
    expect(TokenKind::SEMI, "Expected ';' after field");
    decl->fields.emplace_back(TypeDesc{Identifier{typeTok}, dims},
                              fieldTok.getWord());
  }

  expect(TokenKind::RBRACE, "Expected '}'");
  return decl;
}

// ------------------- //
// Import declaration  //
// ------------------- //
std::unique_ptr<ImportDecl> Parser::parseImportDecl() {
  expect(TokenKind::IMPORT, "Expected 'import'");

  auto decl = std::make_unique<ImportDecl>();
  decl->selective = false;

  Token first = expect(TokenKind::IDENTIFIER, "Expected module name");
  decl->path.segments.push_back(first.getWord());
  decl->path.isStdLib =
      (first.getWord() == "Nexus" || first.getWord() == "Std");

  while (check(TokenKind::COLON_COLON)) {
    consume();
    if (check(TokenKind::LBRACE)) {
      consume();
      decl->selective = true;
      if (!check(TokenKind::RBRACE)) {
        do {
          Token sym = expect(TokenKind::IDENTIFIER, "Expected symbol name");
          decl->symbols.push_back(sym.getWord());
        } while (match(TokenKind::COMMA));
      }
      expect(TokenKind::RBRACE, "Expected '}'");
      break;
    }
    Token seg = expect(TokenKind::IDENTIFIER, "Expected module path segment");
    decl->path.segments.push_back(seg.getWord());
  }

  expect(TokenKind::SEMI, "Expected ';' after import");
  return decl;
}
// ----------------------- //
// Global variable decl    //
// ----------------------- //
std::unique_ptr<GlobalVarDecl> Parser::parseGlobalVarDecl() {
  bool isConst = false;
  if (check(TokenKind::CONST)) {
    consume();
    isConst = true;
  }

  Token typeTok = expect(TokenKind::IDENTIFIER, "Expected type");
  Token nameTok = expect(TokenKind::IDENTIFIER, "Expected variable name");
  expect(TokenKind::ASSIGN, "Global variables must be initialized");
  auto init = parseExpression();
  expect(TokenKind::SEMI, "Expected ';'");

  TypeDesc td(Identifier{typeTok}, 0, isConst);
  return std::make_unique<GlobalVarDecl>(std::move(td), nameTok.getWord(),
                                         std::move(init), isConst);
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
      if (check(TokenKind::AND)) {
        consume();
        isBorrowRef = true;
      }
      if (check(TokenKind::CONST)) {
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

// ------- //
// Block   //
// ------- //
std::unique_ptr<Block> Parser::parseBlock(bool allowSingleStmt) {
  auto block = std::make_unique<Block>();
  if (check(TokenKind::LBRACE)) {
    expect(TokenKind::LBRACE, "Expected '{'");
    while (!check(TokenKind::RBRACE) && !isAtEnd()) {
      try {
        auto s = parseStatement();
        if (s)
          block->statements.push_back(std::move(s));
      } catch (const ParseError &e) {
        std::cerr << e.what() << "\n";
        synchronize();
      }
    }
    expect(TokenKind::RBRACE, "Expected '}'");
  } else if (allowSingleStmt) {
    try {
      auto s = parseStatement();
      if (s)
        block->statements.push_back(std::move(s));
    } catch (const ParseError &e) {
      std::cerr << e.what() << "\n";
      synchronize();
    }
  }
  return block;
}

// ------------------------------------------------------------------ //
// Statement dispatch                                                   //
// ------------------------------------------------------------------ //
std::unique_ptr<Statement> Parser::parseStatement() {
  if (match(TokenKind::RETURN))
    return parseReturnStatement();
  if (match(TokenKind::IF))
    return parseIfStatement();
  if (match(TokenKind::WHILE))
    return parseWhileLoop();
  if (check(TokenKind::CONTINUE) || check(TokenKind::BREAK))
    return parseLoopBreak();
  if (check(TokenKind::CONST))
    return parseVarDeclStatement(AssignKind::Copy);

  if (looksLikeType(peekAt(0))) {
    size_t offset = 1;
    while (peekAt(offset).getKind() == TokenKind::LBRACKET &&
           peekAt(offset + 1).getKind() == TokenKind::RBRACKET) {
      offset += 2;
    }

    if (peekAt(offset).getKind() == TokenKind::IDENTIFIER) {
      TokenKind op = peekAt(offset + 1).getKind();
      if (op == TokenKind::ASSIGN)
        return parseVarDeclStatement(AssignKind::Copy);
      if (op == TokenKind::MOVE)
        return parseVarDeclStatement(AssignKind::Move);
      if (op == TokenKind::BORROW)
        return parseVarDeclStatement(AssignKind::Borrow);
      if (op == TokenKind::SEMI)
        return parseVarDeclNoInit();
    }
  }

  auto expr = parseExpression();
  expect(TokenKind::SEMI, "Expected ';'");
  return std::make_unique<ExprStmt>(std::move(expr));
}

// ---------------------------------------- //
// Uninitialised var decl:  TypeName name;  //
// ---------------------------------------- //
std::unique_ptr<VarDecl> Parser::parseVarDeclNoInit() {
  Token typeTok = consume();
  int dims = 0;
  while (peek().getKind() == TokenKind::LBRACKET &&
         peekAt(1).getKind() == TokenKind::RBRACKET) {
    consume();
    consume();
    ++dims;
  }
  Token nameTok = expect(TokenKind::IDENTIFIER, "Expected variable name");
  expect(TokenKind::SEMI, "Expected ';'");

  TypeDesc td(Identifier{Token{TokenKind::IDENTIFIER, typeTok.getWord(),
                               typeTok.getLine(), typeTok.getColumn()}},
              dims);
  return std::make_unique<VarDecl>(std::move(td), Identifier{nameTok}, nullptr,
                                   AssignKind::Copy, false);
}

// ------------------------ //
// Array assignment helper  //
// ------------------------ //
std::unique_ptr<Statement> Parser::parseArrayAssign() {
  Token arrTok = consume();
  std::vector<ExprPtr> indices;

  while (true) {
    expect(TokenKind::LBRACKET, "Expected '['");
    indices.push_back(parseExpression());
    expect(TokenKind::RBRACKET, "Expected ']'");
    if (!check(TokenKind::LBRACKET))
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
  if (check(TokenKind::CONST)) {
    consume();
    isConst = true;
  }

  Token typeTok = consume();
  int dims = 0;
  while (check(TokenKind::LBRACKET)) {
    consume();
    expect(TokenKind::RBRACKET, "Expected ']'");
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

  // If the initializer starts with '{', treat it as a positional struct
  // literal.
  std::unique_ptr<Expression> init;
  if (check(TokenKind::LBRACE)) {
    consume(); // '{'
    std::vector<ExprPtr> vals;
    if (!check(TokenKind::RBRACE)) {
      do {
        vals.push_back(parseExpression());
      } while (match(TokenKind::COMMA));
    }
    expect(TokenKind::RBRACE, "Expected '}' to close struct literal");
    init = std::make_unique<StructLitExpr>(typeTok.getWord(), std::move(vals));
  } else {
    init = parseExpression();
  }
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
    if (auto *fa = dynamic_cast<FieldAccessExpr *>(left.get())) {
      auto val = parseAssignment();
      return std::make_unique<FieldAssignExpr>(std::move(fa->object), fa->field,
                                               std::move(val));
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
    if (match(TokenKind::EQ))
      expr = std::make_unique<BinaryExpr>(BinaryOp::Eq, std::move(expr),
                                          parseComparison());
    else if (match(TokenKind::NE))
      expr = std::make_unique<BinaryExpr>(BinaryOp::Ne, std::move(expr),
                                          parseComparison());
    else
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
  if (match(TokenKind::SUB))
    return std::make_unique<UnaryExpr>(UnaryOp::Negate, parseUnary());
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

      if (auto *id = dynamic_cast<IdentExpr *>(expr.get()))
        expr = std::make_unique<ArrayIndexExpr>(id->name, std::move(indices));
      else
        throw ParseError(peek().getLine(), peek().getColumn(),
                         "Indexing requires an identifier");
      continue;
    }

    if (match(TokenKind::DOT)) {
      Token prop = expect(TokenKind::IDENTIFIER, "Expected property name");

      if (prop.getWord() == "length") {
        if (auto *id = dynamic_cast<IdentExpr *>(expr.get()))
          return std::make_unique<LengthPropertyExpr>(id->name);
        if (auto *arr = dynamic_cast<ArrayIndexExpr *>(expr.get()))
          return std::make_unique<IndexedLengthExpr>(arr->array,
                                                     std::move(arr->indices));
        throw ParseError(prop.getLine(), prop.getColumn(),
                         "'.length' requires an array identifier");
      }

      expr = std::make_unique<FieldAccessExpr>(std::move(expr), prop.getWord());
      continue;
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
  // null literal
  if (isIdentWord("null")) {
    consume();
    return std::make_unique<NullLitExpr>();
  }

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
    if (!check(TokenKind::LBRACKET))
      break;
    consume();
  } while (true);

  TypeDesc td(Identifier{Token{TokenKind::IDENTIFIER, elemTok.getWord(),
                               elemTok.getLine(), elemTok.getColumn()}},
              sizes.size());
  return std::make_unique<NewArrayExpr>(std::move(td), std::move(sizes));
}
