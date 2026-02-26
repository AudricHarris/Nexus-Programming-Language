#include "CodeGen.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include <memory>
#include <regex>
#include <unordered_map>

using namespace llvm;

//------------------------------------------------------------------------------
// Helper Functions
//------------------------------------------------------------------------------

static std::string normalizeFunctionName(const std::string &name) {
  static const std::unordered_map<std::string, std::string> mappings = {
      {"Main", "main"},
      {"Printf", "printf"},
      {"Print", "print_literal"},
      {"printf", "printf"}};

  auto it = mappings.find(name);
  return (it != mappings.end()) ? it->second : name;
}

static std::string unescapeString(const std::string &s) {
  std::string result;
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '\\' && i + 1 < s.size()) {
      switch (s[i + 1]) {
      case 'n':
        result += '\n';
        ++i;
        break;
      case 't':
        result += '\t';
        ++i;
        break;
      case 'r':
        result += '\r';
        ++i;
        break;
      case '\\':
        result += '\\';
        ++i;
        break;
      case '"':
        result += '"';
        ++i;
        break;
      case '0':
        result += '\0';
        ++i;
        break;
      default:
        result += s[i];
        break;
      }
    } else {
      result += s[i];
    }
  }
  return result;
}

std::string CodeGenerator::hexToAnsi(const std::string &hex) {
  if (hex.size() != 7 || hex[0] != '#')
    return hex;

  int r = std::stoi(hex.substr(1, 2), nullptr, 16);
  int g = std::stoi(hex.substr(3, 2), nullptr, 16);
  int b = std::stoi(hex.substr(5, 2), nullptr, 16);

  return "\033[38;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" +
         std::to_string(b) + "m";
}

std::string CodeGenerator::replaceHexColors(const std::string &input) {
  std::regex hexColor(R"(#([0-9a-fA-F]{6}))");
  std::string result;
  std::sregex_iterator begin(input.begin(), input.end(), hexColor);
  std::sregex_iterator end;

  size_t lastPos = 0;
  for (auto i = begin; i != end; ++i) {
    auto match = *i;
    result += input.substr(lastPos, match.position() - lastPos);
    result += hexToAnsi(match.str());
    lastPos = match.position() + match.length();
  }
  result += input.substr(lastPos);
  return result;
}

//------------------------------------------------------------------------------
// Type System
//------------------------------------------------------------------------------

class TypeResolver {
public:
  static Type *getLLVMType(LLVMContext &context, const Identifier &typeId) {
    const std::string &t = typeId.token.getWord();

    if (t == "i32" || t == "int" || t == "integer")
      return Type::getInt32Ty(context);
    if (t == "i64" || t == "long")
      return Type::getInt64Ty(context);
    if (t == "f32" || t == "float")
      return Type::getFloatTy(context);
    if (t == "f64" || t == "double")
      return Type::getDoubleTy(context);
    if (t == "bool")
      return Type::getInt1Ty(context);
    if (t == "void")
      return Type::getVoidTy(context);
    if (t == "str" || t == "string")
      return PointerType::get(context, 0);

    return nullptr;
  }

  static std::string getFormatSpecifier(Type *ty) {
    if (!ty)
      return "%d";
    if (ty->isIntegerTy(64))
      return "%lld";
    if (ty->isIntegerTy())
      return "%d";
    if (ty->isFloatTy() || ty->isDoubleTy())
      return "%f";
    return "%s";
  }

  static bool isNumeric(Type *ty) {
    return ty->isIntegerTy() || ty->isFloatingPointTy();
  }

  static bool isFloat(Type *ty) { return ty->isFloatingPointTy(); }

  static bool isBool(Type *ty) { return ty->isIntegerTy(1); }
};

//------------------------------------------------------------------------------
// Expression Parser for String Interpolation
//------------------------------------------------------------------------------

namespace {
enum class MiniTokenKind {
  Int,
  Float,
  Bool,
  Ident,
  Plus,
  Minus,
  Star,
  Slash,
  SlashSlash,
  Percent,
  LParen,
  RParen,
  End,
  Unknown
};

struct MiniToken {
  MiniTokenKind kind;
  std::string text;
};

class MiniParser {
public:
  MiniParser(const std::vector<MiniToken> &tokens_, LLVMContext &ctx_,
             IRBuilder<> &builder_, const std::map<std::string, VarInfo> &vars_)
      : tokens(tokens_), ctx(ctx_), builder(builder_), vars(vars_), pos(0) {}

  Value *parse() { return parseAddSub(); }

private:
  const std::vector<MiniToken> &tokens;
  LLVMContext &ctx;
  IRBuilder<> &builder;
  const std::map<std::string, VarInfo> &vars;
  size_t pos;

  const MiniToken &peek() const { return tokens[pos]; }
  MiniToken consume() { return tokens[pos++]; }
  bool check(MiniTokenKind k) const { return tokens[pos].kind == k; }

  Value *parseAddSub() {
    Value *lhs = parseMulDiv();
    if (!lhs)
      return nullptr;

    while (check(MiniTokenKind::Plus) || check(MiniTokenKind::Minus)) {
      auto op = consume();
      Value *rhs = parseMulDiv();
      if (!rhs)
        return nullptr;

      lhs = promoteToMatch(lhs, rhs);
      rhs = promoteToMatch(rhs, lhs);

      if (lhs->getType()->isFloatingPointTy()) {
        lhs = (op.kind == MiniTokenKind::Plus)
                  ? builder.CreateFAdd(lhs, rhs, "fadd")
                  : builder.CreateFSub(lhs, rhs, "fsub");
      } else {
        lhs = (op.kind == MiniTokenKind::Plus)
                  ? builder.CreateAdd(lhs, rhs, "add")
                  : builder.CreateSub(lhs, rhs, "sub");
      }
    }
    return lhs;
  }

  Value *parseMulDiv() {
    Value *lhs = parseUnary();
    if (!lhs)
      return nullptr;

    while (check(MiniTokenKind::Star) || check(MiniTokenKind::Slash) ||
           check(MiniTokenKind::SlashSlash) || check(MiniTokenKind::Percent)) {
      auto op = consume();
      Value *rhs = parseUnary();
      if (!rhs)
        return nullptr;

      switch (op.kind) {
      case MiniTokenKind::SlashSlash: // Floor division
        lhs = toInt(lhs);
        rhs = toInt(rhs);
        lhs = builder.CreateSDiv(lhs, rhs, "divfloor");
        break;

      case MiniTokenKind::Percent: // Modulo
        lhs = toInt(lhs);
        rhs = toInt(rhs);
        lhs = builder.CreateSRem(lhs, rhs, "mod");
        break;

      case MiniTokenKind::Slash: // True division
        lhs = toDouble(lhs);
        rhs = toDouble(rhs);
        lhs = builder.CreateFDiv(lhs, rhs, "divtrue");
        break;

      case MiniTokenKind::Star: // Multiplication
        lhs = promoteToMatch(lhs, rhs);
        rhs = promoteToMatch(rhs, lhs);
        if (lhs->getType()->isFloatingPointTy())
          lhs = builder.CreateFMul(lhs, rhs, "fmul");
        else
          lhs = builder.CreateMul(lhs, rhs, "mul");
        break;

      default:
        break;
      }
    }
    return lhs;
  }

  Value *parseUnary() {
    if (check(MiniTokenKind::Minus)) {
      consume();
      Value *v = parseAtom();
      if (!v)
        return nullptr;

      if (v->getType()->isFloatingPointTy())
        return builder.CreateFNeg(v, "fneg");
      return builder.CreateNeg(v, "neg");
    }
    return parseAtom();
  }

  Value *parseAtom() {
    auto tok = consume();

    switch (tok.kind) {
    case MiniTokenKind::Int:
      return ConstantInt::get(Type::getInt32Ty(ctx), std::stoll(tok.text));

    case MiniTokenKind::Float:
      return ConstantFP::get(Type::getDoubleTy(ctx), std::stod(tok.text));

    case MiniTokenKind::Bool:
      return ConstantInt::get(Type::getInt1Ty(ctx), tok.text == "true" ? 1 : 0);

    case MiniTokenKind::Ident: {
      auto it = vars.find(tok.text);
      if (it == vars.end() || it->second.isMoved)
        return nullptr;

      return builder.CreateLoad(it->second.type, it->second.alloca,
                                tok.text + "_load");
    }

    case MiniTokenKind::LParen: {
      Value *v = parse();
      if (check(MiniTokenKind::RParen))
        consume();
      return v;
    }

    default:
      return nullptr;
    }
  }

  Value *toDouble(Value *v) {
    if (v->getType()->isDoubleTy())
      return v;
    if (v->getType()->isFloatTy())
      return builder.CreateFPExt(v, Type::getDoubleTy(ctx), "f2d");
    if (v->getType()->isIntegerTy())
      return builder.CreateSIToFP(v, Type::getDoubleTy(ctx), "i2d");
    return v;
  }

  Value *toInt(Value *v) {
    if (v->getType()->isFloatingPointTy())
      return builder.CreateFPToSI(v, Type::getInt32Ty(ctx), "f2i");
    return v;
  }

  Value *promoteToMatch(Value *v, Value *other) {
    if (other->getType()->isFloatingPointTy() && v->getType()->isIntegerTy())
      return toDouble(v);
    return v;
  }
};

static std::vector<MiniToken> tokenizeExpression(const std::string &src) {
  std::vector<MiniToken> tokens;
  size_t i = 0;

  while (i < src.size()) {
    char c = src[i];

    // Skip whitespace
    if (std::isspace(static_cast<unsigned char>(c))) {
      ++i;
      continue;
    }

    // Numbers
    if (std::isdigit(static_cast<unsigned char>(c))) {
      std::string num;
      bool isFloat = false;
      while (
          i < src.size() &&
          (std::isdigit(static_cast<unsigned char>(src[i])) || src[i] == '.')) {
        if (src[i] == '.')
          isFloat = true;
        num += src[i++];
      }
      tokens.push_back(
          {isFloat ? MiniTokenKind::Float : MiniTokenKind::Int, num});
      continue;
    }

    // Identifiers and booleans
    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
      std::string id;
      while (
          i < src.size() &&
          (std::isalnum(static_cast<unsigned char>(src[i])) || src[i] == '_'))
        id += src[i++];

      if (id == "true" || id == "false") {
        tokens.push_back({MiniTokenKind::Bool, id});
      } else {
        tokens.push_back({MiniTokenKind::Ident, id});
      }
      continue;
    }

    // Operators and punctuation
    switch (c) {
    case '+':
      tokens.push_back({MiniTokenKind::Plus, "+"});
      ++i;
      break;
    case '-':
      tokens.push_back({MiniTokenKind::Minus, "-"});
      ++i;
      break;
    case '*':
      tokens.push_back({MiniTokenKind::Star, "*"});
      ++i;
      break;
    case '/':
      if (i + 1 < src.size() && src[i + 1] == '/') {
        tokens.push_back({MiniTokenKind::SlashSlash, "//"});
        i += 2;
      } else {
        tokens.push_back({MiniTokenKind::Slash, "/"});
        ++i;
      }
      break;
    case '%':
      tokens.push_back({MiniTokenKind::Percent, "%"});
      ++i;
      break;
    case '(':
      tokens.push_back({MiniTokenKind::LParen, "("});
      ++i;
      break;
    case ')':
      tokens.push_back({MiniTokenKind::RParen, ")"});
      ++i;
      break;
    default:
      tokens.push_back({MiniTokenKind::Unknown, std::string(1, c)});
      ++i;
      break;
    }
  }

  tokens.push_back({MiniTokenKind::End, ""});
  return tokens;
}
} // namespace

//------------------------------------------------------------------------------
// Printf String Processor
//------------------------------------------------------------------------------

static std::string
processPrintfString(const std::string &raw, LLVMContext &ctx,
                    IRBuilder<> &builder,
                    const std::map<std::string, VarInfo> &vars,
                    std::vector<Value *> &outExprs) {

  std::string result;
  size_t i = 0;

  while (i < raw.size()) {
    if (raw[i] != '{') {
      result += raw[i++];
      continue;
    }

    // Find matching closing brace
    size_t start = i + 1;
    size_t depth = 1;
    size_t j = start;

    while (j < raw.size() && depth > 0) {
      if (raw[j] == '{')
        ++depth;
      else if (raw[j] == '}')
        --depth;
      if (depth > 0)
        ++j;
    }

    std::string inner = raw.substr(start, j - start);
    auto tokens = tokenizeExpression(inner);
    MiniParser parser(tokens, ctx, builder, vars);

    if (Value *val = parser.parse()) {
      result += TypeResolver::getFormatSpecifier(val->getType());
      outExprs.push_back(val);
    } else {
      // If parsing fails, keep the original braces
      result += '{' + inner + '}';
    }

    i = j + 1;
  }

  return result;
}

//------------------------------------------------------------------------------
// CodeGenerator Implementation
//------------------------------------------------------------------------------

CodeGenerator::CodeGenerator()
    : module(std::make_unique<Module>("nexus", context)), builder(context) {
  module->setTargetTriple(Triple(LLVM_HOST_TRIPLE));
}

Value *CodeGenerator::logError(const char *msg) {
  errs() << "\033[31mCode compiling error: " << msg << "\033[0m\n";
  return nullptr;
}

//------------------------------------------------------------------------------
// Expression Code Generation
//------------------------------------------------------------------------------

Value *CodeGenerator::visitIdentifier(const IdentExpr &expr) {
  std::string name = expr.name.token.getWord();
  auto it = namedValues.find(name);

  if (it == namedValues.end())
    return logError(("Unknown variable: " + name).c_str());

  if (it->second.isMoved)
    return logError(("Use of moved value: " + name).c_str());

  return builder.CreateLoad(it->second.type, it->second.alloca, name + "_load");
}

Value *CodeGenerator::visitLiteral(const IntLitExpr &expr) {
  return ConstantInt::get(Type::getInt32Ty(context),
                          std::stoll(expr.lit.token.getWord()));
}

Value *CodeGenerator::visitLiteral(const FloatLitExpr &expr) {
  return ConstantFP::get(Type::getDoubleTy(context),
                         std::stod(expr.lit.token.getWord()));
}

Value *CodeGenerator::visitLiteral(const StrLitExpr &expr) {
  return builder.CreateGlobalString(expr.lit.token.getWord());
}

Value *CodeGenerator::visitLiteral(const BoolLitExpr &expr) {
  bool value = (expr.lit.token.getWord() == "true");
  return ConstantInt::get(Type::getInt1Ty(context), value);
}

Value *CodeGenerator::promoteToDouble(Value *val) {
  if (val->getType()->isDoubleTy())
    return val;
  if (val->getType()->isFloatTy())
    return builder.CreateFPExt(val, Type::getDoubleTy(context), "f2d");
  return builder.CreateSIToFP(val, Type::getDoubleTy(context), "i2d");
}

Value *CodeGenerator::promoteToInt(Value *val) {
  if (val->getType()->isIntegerTy())
    return val;
  return builder.CreateFPToSI(val, Type::getInt32Ty(context), "f2i");
}

Value *CodeGenerator::visitBinary(const BinaryExpr &expr) {
  Value *left = codegen(*expr.left);
  Value *right = codegen(*expr.right);

  if (!left || !right)
    return nullptr;

  bool leftFloat = left->getType()->isFloatingPointTy();
  bool rightFloat = right->getType()->isFloatingPointTy();
  // bool leftBool = TypeResolver::isBool(left->getType());
  // bool rightBool = TypeResolver::isBool(right->getType());

  // Handle comparison operators separately (they return bool)
  switch (expr.op) {
  case BinaryOp::Lt:
  case BinaryOp::Le:
  case BinaryOp::Gt:
  case BinaryOp::Ge:
  case BinaryOp::Eq:
  case BinaryOp::Ne:
    return generateComparison(expr.op, left, right, leftFloat || rightFloat);
  default:
    break;
  }

  // Handle arithmetic operators
  switch (expr.op) {
  case BinaryOp::Add:
    if (leftFloat || rightFloat) {
      left = promoteToDouble(left);
      right = promoteToDouble(right);
      return builder.CreateFAdd(left, right, "fadd");
    }
    return builder.CreateAdd(left, right, "add");

  case BinaryOp::Sub:
    if (leftFloat || rightFloat) {
      left = promoteToDouble(left);
      right = promoteToDouble(right);
      return builder.CreateFSub(left, right, "fsub");
    }
    return builder.CreateSub(left, right, "sub");

  case BinaryOp::Mul:
    if (leftFloat || rightFloat) {
      left = promoteToDouble(left);
      right = promoteToDouble(right);
      return builder.CreateFMul(left, right, "fmul");
    }
    return builder.CreateMul(left, right, "mul");

  case BinaryOp::Div:
    left = promoteToDouble(left);
    right = promoteToDouble(right);
    return builder.CreateFDiv(left, right, "fdiv");

  case BinaryOp::DivFloor:
    left = promoteToInt(left);
    right = promoteToInt(right);
    return builder.CreateSDiv(left, right, "divfloor");

  case BinaryOp::Mod:
    left = promoteToInt(left);
    right = promoteToInt(right);
    return builder.CreateSRem(left, right, "mod");

  default:
    return logError("Unknown binary operator");
  }
}

Value *CodeGenerator::generateComparison(BinaryOp op, Value *left, Value *right,
                                         bool isFloat) {
  if (isFloat) {
    left = promoteToDouble(left);
    right = promoteToDouble(right);

    switch (op) {
    case BinaryOp::Lt:
      return builder.CreateFCmpOLT(left, right, "flt");
    case BinaryOp::Le:
      return builder.CreateFCmpOLE(left, right, "fle");
    case BinaryOp::Gt:
      return builder.CreateFCmpOGT(left, right, "fgt");
    case BinaryOp::Ge:
      return builder.CreateFCmpOGE(left, right, "fge");
    case BinaryOp::Eq:
      return builder.CreateFCmpOEQ(left, right, "feq");
    case BinaryOp::Ne:
      return builder.CreateFCmpONE(left, right, "fne");
    default:
      return nullptr;
    }
  } else {
    switch (op) {
    case BinaryOp::Lt:
      return builder.CreateICmpSLT(left, right, "ilt");
    case BinaryOp::Le:
      return builder.CreateICmpSLE(left, right, "ile");
    case BinaryOp::Gt:
      return builder.CreateICmpSGT(left, right, "igt");
    case BinaryOp::Ge:
      return builder.CreateICmpSGE(left, right, "ige");
    case BinaryOp::Eq:
      return builder.CreateICmpEQ(left, right, "ieq");
    case BinaryOp::Ne:
      return builder.CreateICmpNE(left, right, "ine");
    default:
      return nullptr;
    }
  }
}

Value *CodeGenerator::visitUnary(const UnaryExpr &expr) {
  Value *operand = codegen(*expr.operand);
  if (!operand)
    return nullptr;

  switch (expr.op) {
  case UnaryOp::Negate:
    if (operand->getType()->isFloatingPointTy())
      return builder.CreateFNeg(operand, "fneg");
    return builder.CreateNeg(operand, "neg");
    // case UnaryOp::Not:
    // return builder.CreateNot(operand, "not");
  }

  return logError("Unknown unary operator");
}

Value *CodeGenerator::visitAssignment(const AssignExpr &expr) {
  std::string targetName = expr.target.token.getWord();
  Value *val = codegen(*expr.value);
  if (!val)
    return nullptr;

  auto it = namedValues.find(targetName);
  if (it == namedValues.end())
    return logError(("Undeclared variable: " + targetName).c_str());

  if (it->second.isBorrowed)
    return logError(("Cannot modify borrowed variable: " + targetName).c_str());

  switch (expr.kind) {
  case AssignKind::Copy:
    builder.CreateStore(val, it->second.alloca);
    break;

  case AssignKind::Move: {
    auto *sourceIdent = dynamic_cast<const IdentExpr *>(expr.value.get());
    if (!sourceIdent)
      return logError("Move requires variable on right-hand side");

    std::string sourceName = sourceIdent->name.token.getWord();
    auto srcIt = namedValues.find(sourceName);

    if (srcIt == namedValues.end())
      return logError(("Unknown variable: " + sourceName).c_str());
    if (srcIt->second.isMoved)
      return logError(("Already moved: " + sourceName).c_str());
    if (srcIt->second.isBorrowed)
      return logError(("Cannot move borrowed value: " + sourceName).c_str());

    builder.CreateStore(val, it->second.alloca);
    srcIt->second.isMoved = true;
    break;
  }

  case AssignKind::Borrow: {
    auto *sourceIdent = dynamic_cast<const IdentExpr *>(expr.value.get());
    if (!sourceIdent)
      return logError("Borrow requires variable on right-hand side");

    std::string sourceName = sourceIdent->name.token.getWord();
    auto srcIt = namedValues.find(sourceName);

    if (srcIt == namedValues.end())
      return logError(("Unknown variable: " + sourceName).c_str());
    if (srcIt->second.isMoved)
      return logError(("Cannot borrow moved value: " + sourceName).c_str());

    // Create borrow reference (read-only)
    namedValues[targetName] = {
        srcIt->second.alloca, srcIt->second.type,
        true, // isBorrowed
        false // isMoved
    };
    break;
  }
  }

  return val;
}

Value *CodeGenerator::visitIncrement(const Increment &expr) {
  return generateIncrementDecrement(expr.target.token.getWord(), true);
}

Value *CodeGenerator::visitDecrement(const Decrement &expr) {
  return generateIncrementDecrement(expr.target.token.getWord(), false);
}

Value *CodeGenerator::generateIncrementDecrement(const std::string &varName,
                                                 bool isInc) {
  auto it = namedValues.find(varName);
  if (it == namedValues.end())
    return logError(("Unknown variable: " + varName).c_str());

  if (it->second.isBorrowed || it->second.isMoved)
    return logError(("Cannot modify " + varName).c_str());

  Value *cur =
      builder.CreateLoad(it->second.type, it->second.alloca, "load_" + varName);
  Value *one = ConstantInt::get(it->second.type, 1);
  Value *result = isInc ? builder.CreateAdd(cur, one, "inc")
                        : builder.CreateSub(cur, one, "dec");

  builder.CreateStore(result, it->second.alloca);
  return result;
}

Value *CodeGenerator::visitCall(const CallExpr &expr) {
  std::string rawName = expr.callee.token.getWord();
  std::string calleeName = normalizeFunctionName(rawName);

  // Handle special print functions
  if ((calleeName == "printf" || rawName == "Printf") &&
      expr.arguments.size() == 1) {
    return handlePrintf(expr);
  }

  if (rawName == "Print" && expr.arguments.size() == 1) {
    return handlePrint(expr);
  }

  // Regular function call
  llvm::Function *callee = module->getFunction(calleeName);
  if (!callee)
    return logError(("Unknown function: " + calleeName).c_str());

  if (!callee->isVarArg() && callee->arg_size() != expr.arguments.size())
    return logError("Incorrect number of arguments");

  std::vector<Value *> args;
  for (auto &arg : expr.arguments) {
    Value *v = codegen(*arg);
    if (!v)
      return nullptr;
    args.push_back(v);
  }

  return builder.CreateCall(callee, args, "calltmp");
}

Value *CodeGenerator::handlePrintf(const CallExpr &expr) {
  auto *strArg = dynamic_cast<const StrLitExpr *>(expr.arguments[0].get());
  if (!strArg)
    return logError("Printf requires string literal");

  std::vector<Value *> interpolated;
  std::string fmt =
      processPrintfString(unescapeString(strArg->lit.token.getWord()), context,
                          builder, namedValues, interpolated);

  fmt = replaceHexColors(fmt) + "\033[0m";

  Value *fmtPtr = builder.CreateGlobalString(fmt, ".fmt");
  llvm::Function *printf = module->getFunction("printf");
  if (!printf)
    return logError("printf not declared");

  std::vector<Value *> args = {fmtPtr};
  for (Value *v : interpolated) {
    if (v->getType()->isFloatTy())
      v = builder.CreateFPExt(v, Type::getDoubleTy(context), "f2d_arg");
    args.push_back(v);
  }

  return builder.CreateCall(printf, args, "printf_ret");
}

Value *CodeGenerator::handlePrint(const CallExpr &expr) {
  auto *strArg = dynamic_cast<const StrLitExpr *>(expr.arguments[0].get());
  if (!strArg)
    return logError("Print requires string literal");

  std::string literal = unescapeString(strArg->lit.token.getWord());
  literal = replaceHexColors(literal) + "\033[0m\n";

  Value *fmtPtr = builder.CreateGlobalString(literal, ".lit");
  Value *strSpec = builder.CreateGlobalString("%s", ".pfmt");
  llvm::Function *printf = module->getFunction("printf");

  if (!printf)
    return logError("printf not declared");
  return builder.CreateCall(printf, {strSpec, fmtPtr}, "print_ret");
}

Value *CodeGenerator::codegen(const Expression &expr) {
  // Dispatch to appropriate visitor method
  if (auto *ie = dynamic_cast<const IdentExpr *>(&expr))
    return visitIdentifier(*ie);
  if (auto *ile = dynamic_cast<const IntLitExpr *>(&expr))
    return visitLiteral(*ile);
  if (auto *fle = dynamic_cast<const FloatLitExpr *>(&expr))
    return visitLiteral(*fle);
  if (auto *sle = dynamic_cast<const StrLitExpr *>(&expr))
    return visitLiteral(*sle);
  if (auto *ble = dynamic_cast<const BoolLitExpr *>(&expr))
    return visitLiteral(*ble);
  if (auto *bin = dynamic_cast<const BinaryExpr *>(&expr))
    return visitBinary(*bin);
  if (auto *un = dynamic_cast<const UnaryExpr *>(&expr))
    return visitUnary(*un);
  if (auto *ae = dynamic_cast<const AssignExpr *>(&expr))
    return visitAssignment(*ae);
  if (auto *inc = dynamic_cast<const Increment *>(&expr))
    return visitIncrement(*inc);
  if (auto *dec = dynamic_cast<const Decrement *>(&expr))
    return visitDecrement(*dec);
  if (auto *ce = dynamic_cast<const CallExpr *>(&expr))
    return visitCall(*ce);

  return logError("Unknown expression type");
}

//------------------------------------------------------------------------------
// Statement Code Generation
//------------------------------------------------------------------------------

Value *CodeGenerator::codegen(const Statement &stmt) {
  if (auto *vd = dynamic_cast<const VarDecl *>(&stmt))
    return visitVarDecl(*vd);
  if (auto *es = dynamic_cast<const ExprStmt *>(&stmt))
    return codegen(*es->expr);
  if (auto *ifs = dynamic_cast<const IfStmt *>(&stmt))
    return visitIfStmt(*ifs);
  if (auto *loop = dynamic_cast<const WhileStmt *>(&stmt))
    return visitWhileStmt(*loop);
  if (auto *ret = dynamic_cast<const Return *>(&stmt))
    return visitReturn(*ret);

  return logError("Unknown statement type");
}

Value *CodeGenerator::visitVarDecl(const VarDecl &decl) {
  std::string varName = decl.name.token.getWord();

  Type *ty = TypeResolver::getLLVMType(context, decl.type);
  if (!ty)
    return logError(("Unknown type: " + varName).c_str());

  AllocaInst *alloc = builder.CreateAlloca(ty, nullptr, varName);

  if (decl.initializer) {
    if (decl.isMove) {
      return handleMoveInitialization(decl, varName, ty, alloc);
    } else {
      return handleCopyInitialization(decl, varName, ty, alloc);
    }
  }

  namedValues[varName] = {alloc, ty, false, false};
  return alloc;
}

Value *CodeGenerator::handleMoveInitialization(const VarDecl &decl,
                                               const std::string &varName,
                                               Type *ty, AllocaInst *alloc) {
  auto *identExpr = dynamic_cast<const IdentExpr *>(decl.initializer.get());
  if (!identExpr)
    return logError("Move requires variable on right-hand side");

  std::string sourceName = identExpr->name.token.getWord();
  auto srcIt = namedValues.find(sourceName);

  if (srcIt == namedValues.end())
    return logError(("Unknown variable: " + sourceName).c_str());
  if (srcIt->second.isMoved)
    return logError(("Already moved: " + sourceName).c_str());
  if (srcIt->second.isBorrowed)
    return logError(("Cannot move borrowed value: " + sourceName).c_str());

  Value *srcVal = builder.CreateLoad(srcIt->second.type, srcIt->second.alloca,
                                     sourceName + "_load");
  builder.CreateStore(srcVal, alloc);
  srcIt->second.isMoved = true;

  namedValues[varName] = {alloc, ty, false, false};
  return alloc;
}

Value *CodeGenerator::handleCopyInitialization(const VarDecl &decl,
                                               const std::string &varName,
                                               Type *ty, AllocaInst *alloc) {
  Value *initVal = codegen(*decl.initializer);
  if (!initVal)
    return logError(("Failed to initialize: " + varName).c_str());

  // Type conversions
  if (ty->isFloatTy() && initVal->getType()->isDoubleTy())
    initVal = builder.CreateFPTrunc(initVal, ty, "d2f");
  else if (ty->isDoubleTy() && initVal->getType()->isFloatTy())
    initVal = builder.CreateFPExt(initVal, ty, "f2d");
  else if (ty->isIntegerTy() && initVal->getType()->isFloatingPointTy())
    initVal = builder.CreateFPToSI(initVal, ty, "f2i");
  else if (ty->isFloatingPointTy() && initVal->getType()->isIntegerTy())
    initVal = builder.CreateSIToFP(initVal, ty, "i2f");

  builder.CreateStore(initVal, alloc);
  namedValues[varName] = {alloc, ty, false, false};
  return alloc;
}

Value *CodeGenerator::visitIfStmt(const IfStmt &stmt) {
  Value *cond = codegen(*stmt.condition);
  if (!cond)
    return nullptr;

  // Convert to boolean if needed
  if (!cond->getType()->isIntegerTy(1)) {
    cond = builder.CreateICmpNE(cond, ConstantInt::get(cond->getType(), 0),
                                "ifcond");
  }

  llvm::Function *func = builder.GetInsertBlock()->getParent();
  BasicBlock *thenBB = BasicBlock::Create(context, "then", func);
  BasicBlock *elseBB = BasicBlock::Create(context, "else");
  BasicBlock *mergeBB = BasicBlock::Create(context, "ifcont");

  builder.CreateCondBr(cond, thenBB, elseBB);

  // Then branch
  builder.SetInsertPoint(thenBB);
  codegen(*stmt.thenBranch);
  if (!builder.GetInsertBlock()->getTerminator())
    builder.CreateBr(mergeBB);

  // Else branch
  func->insert(func->end(), elseBB);
  builder.SetInsertPoint(elseBB);
  if (stmt.elseBranch)
    codegen(*stmt.elseBranch);
  if (!builder.GetInsertBlock()->getTerminator())
    builder.CreateBr(mergeBB);

  // Merge point
  func->insert(func->end(), mergeBB);
  builder.SetInsertPoint(mergeBB);

  return nullptr;
}

Value *CodeGenerator::visitWhileStmt(const WhileStmt &stmt) {
  llvm::Function *func = builder.GetInsertBlock()->getParent();

  BasicBlock *condBB = BasicBlock::Create(context, "while.cond", func);
  BasicBlock *bodyBB = BasicBlock::Create(context, "while.body");
  BasicBlock *exitBB = BasicBlock::Create(context, "while.end");

  builder.CreateBr(condBB);

  // Condition block
  builder.SetInsertPoint(condBB);
  Value *cond = codegen(*stmt.condition);
  if (!cond)
    return nullptr;

  if (!cond->getType()->isIntegerTy(1)) {
    cond = builder.CreateICmpNE(cond, ConstantInt::get(cond->getType(), 0),
                                "cond");
  }
  builder.CreateCondBr(cond, bodyBB, exitBB);

  // Body block
  func->insert(func->end(), bodyBB);
  builder.SetInsertPoint(bodyBB);
  codegen(*stmt.doBranch);
  if (!builder.GetInsertBlock()->getTerminator())
    builder.CreateBr(condBB);

  // Exit block
  func->insert(func->end(), exitBB);
  builder.SetInsertPoint(exitBB);

  return nullptr;
}

Value *CodeGenerator::visitReturn(const Return &stmt) {
  if (stmt.value) {
    Value *v = codegen(**stmt.value);
    if (!v)
      return nullptr;
    builder.CreateRet(v);
  } else {
    builder.CreateRetVoid();
  }
  return nullptr;
}

void CodeGenerator::codegen(const Block &block) {
  for (const auto &stmt : block.statements) {
    if (builder.GetInsertBlock()->getTerminator())
      break;
    codegen(*stmt);
  }
}

//------------------------------------------------------------------------------
// Function Code Generation
//------------------------------------------------------------------------------

llvm::Function *CodeGenerator::codegen(const AST_H::Function &func) {
  std::string fname = normalizeFunctionName(func.name.token.getWord());
  Type *retTy = Type::getInt32Ty(context); // Default return type

  // Build parameter types
  std::vector<Type *> paramTypes;
  for (const auto &p : func.params) {
    Type *t = TypeResolver::getLLVMType(context, p.type);
    if (!t)
      return nullptr;
    paramTypes.push_back(t);
  }

  // Create or get function
  llvm::Function *f = module->getFunction(fname);
  if (!f) {
    auto *ft = FunctionType::get(retTy, paramTypes, false);
    f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, fname,
                               *module);
  }

  // Create entry block
  BasicBlock *bb = BasicBlock::Create(context, "entry", f);
  builder.SetInsertPoint(bb);

  // Store parameters
  namedValues.clear();
  size_t idx = 0;
  for (auto &arg : f->args()) {
    const auto &param = func.params[idx++];
    std::string name = param.name.token.getWord();

    AllocaInst *alloc = builder.CreateAlloca(arg.getType(), nullptr, name);
    builder.CreateStore(&arg, alloc);
    namedValues[name] = {alloc, arg.getType(), false, false};
  }

  // Generate body
  codegen(*func.body);

  // Add default return if missing
  if (!builder.GetInsertBlock()->getTerminator()) {
    if (retTy->isVoidTy())
      builder.CreateRetVoid();
    else
      builder.CreateRet(ConstantInt::get(retTy, 0));
  }

  // Verify function
  if (verifyFunction(*f, &errs())) {
    f->eraseFromParent();
    return nullptr;
  }

  return f;
}

//------------------------------------------------------------------------------
// Program Generation
//------------------------------------------------------------------------------

bool CodeGenerator::generate(const Program &program,
                             const std::string &outputFilename) {
  namedValues.clear();

  // Declare printf
  auto *i8PtrTy = PointerType::get(context, 0);
  auto *printfTy =
      FunctionType::get(Type::getInt32Ty(context), {i8PtrTy}, true);
  llvm::Function::Create(printfTy, llvm::Function::ExternalLinkage, "printf",
                         *module);

  // Generate all functions
  for (const auto &f : program.functions) {
    if (!codegen(*f))
      return false;
  }

  // Write output
  std::error_code ec;
  raw_fd_ostream out(outputFilename + ".ll", ec, sys::fs::OF_None);
  if (ec) {
    errs() << "Could not open output: " << ec.message() << "\n";
    return false;
  }

  module->print(out, nullptr);
  return true;
}
