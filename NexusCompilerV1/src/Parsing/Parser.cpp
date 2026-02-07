#include "Parser.h"
#include <stdexcept>

Parser::Parser(const std::vector<Token> &tokens)
    : tokens(tokens), currentPos(0) {}

Token Parser::peek() const {
  if (isAtEnd())
    return tokens.back(); // return EOF token
  return tokens[currentPos];
}

Token Parser::advance() {
  if (!isAtEnd())
    currentPos++;
  return tokens[currentPos - 1];
}

bool Parser::match(TokenKind kind) {
  if (check(kind)) {
    advance();
    return true;
  }
  return false;
}

bool Parser::check(TokenKind kind) const {
  if (isAtEnd())
    return false;
  return peek().getKind() == kind;
}

Token Parser::consume(TokenKind kind, const std::string &errorMsg) {
  if (check(kind))
    return advance();
  throw ParseError(errorMsg + " at " + peek().toString());
}

bool Parser::isAtEnd() const {
  return currentPos >= tokens.size() || peek().getKind() == TokenKind::eof;
}

std::unique_ptr<Program> Parser::parse() {
  auto program = std::make_unique<Program>();

  try {
    while (!isAtEnd()) {
      auto decl = parseDeclaration();
      if (decl) {
        program->declarations.push_back(std::move(decl));
      }
    }
  } catch (const ParseError &e) {
    throw;
  }

  return program;
}

std::unique_ptr<Decl> Parser::parseDeclaration() {
  // Skip any semicolons
  while (match(TokenKind::delim_semicolon)) {
  }

  if (isAtEnd())
    return nullptr;

  // Check for class declaration
  if (match(TokenKind::kw_class)) {
    return parseClassDecl();
  }

  // Check for sum type (Options)
  if (match(TokenKind::kw_Options)) {
    return parseSumTypeDecl();
  }

  // Otherwise, assume variable declaration or error
  throw ParseError("Expected declaration at " + peek().toString());
}

std::unique_ptr<ClassDecl> Parser::parseClassDecl() {
  auto classDecl = std::make_unique<ClassDecl>();

  // Class name
  Token nameToken = consume(TokenKind::identifier, "Expected class name");
  classDecl->name = nameToken.getLexeme();

  consume(TokenKind::delim_lbrace, "Expected '{' after class name");

  // Parse fields and methods
  while (!check(TokenKind::delim_rbrace) && !isAtEnd()) {
    // Check for visibility modifiers
    bool isPublic = true;
    if (match(TokenKind::kw_private)) {
      isPublic = false;
    } else if (match(TokenKind::kw_public)) {
      isPublic = true;
    }

    // Check for constructor or method
    if (check(TokenKind::kw_Constructor)) {
      auto method = parseMethodDecl();
      method->isPublic = isPublic;
      method->isConstructor = true;
      classDecl->methods.push_back(std::move(method));
    } else if (check(TokenKind::identifier)) {
      // Could be field or method - look ahead
      size_t saved = currentPos;
      advance(); // skip identifier

      if (check(TokenKind::delim_lparen)) {
        // It's a method
        currentPos = saved;
        auto method = parseMethodDecl();
        method->isPublic = isPublic;
        classDecl->methods.push_back(std::move(method));
      } else {
        // It's a field
        currentPos = saved;
        auto field = parseVarDecl();
        classDecl->fields.push_back(std::move(field));
      }
    } else {
      throw ParseError("Expected field or method declaration");
    }
  }

  consume(TokenKind::delim_rbrace, "Expected '}' after class body");

  return classDecl;
}

std::unique_ptr<MethodDecl> Parser::parseMethodDecl() {
  auto method = std::make_unique<MethodDecl>();

  // Method name or Constructor
  if (match(TokenKind::kw_Constructor)) {
    method->name = "Constructor";
    method->isConstructor = true;
  } else {
    Token nameToken = consume(TokenKind::identifier, "Expected method name");
    method->name = nameToken.getLexeme();
  }

  // Parameters
  consume(TokenKind::delim_lparen, "Expected '(' after method name");

  while (!check(TokenKind::delim_rparen) && !isAtEnd()) {
    Token paramName = consume(TokenKind::identifier, "Expected parameter name");
    consume(TokenKind::delim_colon, "Expected ':' after parameter name");
    Type paramType = parseType();

    method->params.push_back({paramName.getLexeme(), paramType});

    if (!check(TokenKind::delim_rparen)) {
      consume(TokenKind::delim_comma, "Expected ',' between parameters");
    }
  }

  consume(TokenKind::delim_rparen, "Expected ')' after parameters");

  // Return type (if not constructor)
  if (!method->isConstructor) {
    consume(TokenKind::op_arrow, "Expected '->' before return type");
    method->returnType = parseType();
  }

  // Method body
  method->body = parseBlockStmt();

  return method;
}

std::unique_ptr<VarDecl> Parser::parseVarDecl() {
  auto varDecl = std::make_unique<VarDecl>();

  Token nameToken = consume(TokenKind::identifier, "Expected variable name");
  varDecl->name = nameToken.getLexeme();

  consume(TokenKind::delim_colon, "Expected ':' after variable name");
  varDecl->type = parseType();

  // Optional initializer
  if (match(TokenKind::op_assign)) {
    varDecl->initializer = parseExpression();
  }

  consume(TokenKind::delim_semicolon,
          "Expected ';' after variable declaration");

  return varDecl;
}

std::unique_ptr<SumTypeDecl> Parser::parseSumTypeDecl() {
  auto sumType = std::make_unique<SumTypeDecl>();

  Token nameToken = consume(TokenKind::identifier, "Expected sum type name");
  sumType->name = nameToken.getLexeme();

  consume(TokenKind::delim_lbrace, "Expected '{' after sum type name");

  // Parse variants
  while (!check(TokenKind::delim_rbrace) && !isAtEnd()) {
    Token variantName = consume(TokenKind::identifier, "Expected variant name");

    SumTypeDecl::Variant variant;
    variant.name = variantName.getLexeme();

    // Check for payload
    if (match(TokenKind::delim_lparen)) {
      variant.payload = parseType();
      consume(TokenKind::delim_rparen, "Expected ')' after variant payload");
    }

    sumType->variants.push_back(variant);

    if (!check(TokenKind::delim_rbrace)) {
      match(TokenKind::delim_comma); // Optional comma
    }
  }

  consume(TokenKind::delim_rbrace, "Expected '}' after sum type variants");

  return sumType;
}

std::unique_ptr<Stmt> Parser::parseStatement() {
  if (match(TokenKind::kw_while)) {
    return parseWhileStmt();
  }
  if (check(TokenKind::delim_lbrace)) {
    return parseBlockStmt();
  }

  return parseExprStmt();
}

std::unique_ptr<Stmt> Parser::parseBlockStmt() {
  auto block = std::make_unique<BlockStmt>();

  consume(TokenKind::delim_lbrace, "Expected '{'");

  while (!check(TokenKind::delim_rbrace) && !isAtEnd()) {
    block->statements.push_back(parseStatement());
  }

  consume(TokenKind::delim_rbrace, "Expected '}'");

  return block;
}

std::unique_ptr<Stmt> Parser::parseWhileStmt() {
  auto whileStmt = std::make_unique<WhileStmt>();

  consume(TokenKind::delim_lparen, "Expected '(' after 'while'");
  whileStmt->condition = parseExpression();
  consume(TokenKind::delim_rparen, "Expected ')' after condition");

  whileStmt->body = parseStatement();

  return whileStmt;
}

std::unique_ptr<Stmt> Parser::parseIfStmt() {
  // TODO: Implement when kw_if is added to TokenKind
  throw ParseError("If statements not yet implemented");
}

std::unique_ptr<Stmt> Parser::parseReturnStmt() {
  // TODO: Implement when kw_return is added to TokenKind
  throw ParseError("Return statements not yet implemented");
}

std::unique_ptr<Stmt> Parser::parseExprStmt() {
  auto stmt = std::make_unique<ExprStmt>();
  stmt->expr = parseExpression();
  consume(TokenKind::delim_semicolon, "Expected ';' after expression");
  return stmt;
}

std::unique_ptr<Expr> Parser::parseExpression() { return parseAssignment(); }

std::unique_ptr<Expr> Parser::parseAssignment() {
  auto expr = parseLogical();

  if (match(TokenKind::op_assign)) {
    Token op = tokens[currentPos - 1];
    auto value = parseAssignment();
    auto binary =
        std::make_unique<BinaryExpr>(std::move(expr), op, std::move(value));
    return binary;
  }

  return expr;
}

std::unique_ptr<Expr> Parser::parseLogical() { return parseEquality(); }

std::unique_ptr<Expr> Parser::parseEquality() { return parseComparison(); }

std::unique_ptr<Expr> Parser::parseComparison() {
  auto expr = parseTerm();

  while (match(TokenKind::op_lt) || match(TokenKind::op_gt)) {
    Token op = tokens[currentPos - 1];
    auto right = parseTerm();
    expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
  }

  return expr;
}

std::unique_ptr<Expr> Parser::parseTerm() {
  auto expr = parseFactor();

  while (match(TokenKind::op_plus) || match(TokenKind::op_minus)) {
    Token op = tokens[currentPos - 1];
    auto right = parseFactor();
    expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
  }

  return expr;
}

std::unique_ptr<Expr> Parser::parseFactor() {
  auto expr = parseUnary();

  while (match(TokenKind::op_mult) || match(TokenKind::op_div) ||
         match(TokenKind::op_mod)) {
    Token op = tokens[currentPos - 1];
    auto right = parseUnary();
    expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
  }

  return expr;
}

std::unique_ptr<Expr> Parser::parseUnary() {
  if (match(TokenKind::op_minus)) {
    Token op = tokens[currentPos - 1];
    auto right = parseUnary();
    return std::make_unique<BinaryExpr>(nullptr, op,
                                        std::move(right)); // Unary minus
  }

  return parseCall();
}

std::unique_ptr<Expr> Parser::parseCall() {
  auto expr = parsePrimary();

  while (true) {
    if (match(TokenKind::delim_lparen)) {
      std::vector<ExprPtr> args;

      while (!check(TokenKind::delim_rparen) && !isAtEnd()) {
        args.push_back(parseExpression());
        if (!check(TokenKind::delim_rparen)) {
          consume(TokenKind::delim_comma, "Expected ',' between arguments");
        }
      }

      consume(TokenKind::delim_rparen, "Expected ')' after arguments");
      expr = std::make_unique<CallExpr>(std::move(expr), std::move(args));
    } else if (match(TokenKind::op_dot)) {
      Token member =
          consume(TokenKind::identifier, "Expected member name after '.'");
      expr = std::make_unique<MemberAccessExpr>(std::move(expr),
                                                member.getLexeme());
    } else {
      break;
    }
  }

  return expr;
}

std::unique_ptr<Expr> Parser::parsePrimary() {
  if (match(TokenKind::kw_true)) {
    return std::make_unique<LiteralExpr>(tokens[currentPos - 1]);
  }

  if (match(TokenKind::lit_integer) || match(TokenKind::lit_float) ||
      match(TokenKind::lit_string)) {
    return std::make_unique<LiteralExpr>(tokens[currentPos - 1]);
  }

  if (match(TokenKind::identifier)) {
    return std::make_unique<IdentifierExpr>(tokens[currentPos - 1].getLexeme(),
                                            tokens[currentPos - 1]);
  }

  if (match(TokenKind::kw_new)) {
    Type type = parseType();
    consume(TokenKind::delim_lparen,
            "Expected '(' after type in 'new' expression");

    std::vector<ExprPtr> args;
    while (!check(TokenKind::delim_rparen) && !isAtEnd()) {
      args.push_back(parseExpression());
      if (!check(TokenKind::delim_rparen)) {
        consume(TokenKind::delim_comma, "Expected ',' between arguments");
      }
    }

    consume(TokenKind::delim_rparen, "Expected ')' after arguments");
    return std::make_unique<NewExpr>(type, std::move(args));
  }

  if (match(TokenKind::delim_lparen)) {
    auto expr = parseExpression();
    consume(TokenKind::delim_rparen, "Expected ')' after expression");
    return expr;
  }

  throw ParseError("Expected expression at " + peek().toString());
}

std::unique_ptr<Expr> Parser::parseMatchExpr() {
  // TODO: Implement match expressions
  throw ParseError("Match expressions not yet implemented");
}

Type Parser::parseType() {
  Type type;

  if (match(TokenKind::type_i32)) {
    type.name = "i32";
  } else if (match(TokenKind::type_f64)) {
    type.name = "f64";
  } else if (match(TokenKind::identifier)) {
    type.name = tokens[currentPos - 1].getLexeme();
  } else {
    throw ParseError("Expected type at " + peek().toString());
  }

  return type;
}
