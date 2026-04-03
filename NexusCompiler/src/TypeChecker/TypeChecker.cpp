#include "TypeChecker.h"

#include <cassert>

// ------------------------- //
//  Scope / symbol helpers   //
// ------------------------- //

void TypeChecker::declareVar(const std::string &name, const NexusType &type) {
  if (scopes_.empty()) {
    error("Internal: no active scope when declaring '" + name + "'");
    return;
  }
  auto &top = scopes_.back();
  if (top.count(name)) {
    error("Redeclaration of variable '" + name + "' in the same scope");
    return;
  }
  top[name] = type;
}

std::optional<NexusType> TypeChecker::lookupVar(const std::string &name) const {
  for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
    auto found = it->find(name);
    if (found != it->end())
      return found->second;
  }
  return std::nullopt;
}

bool TypeChecker::typeExists(const std::string &name) const {
  static const std::vector<std::string> primitives = {
      "int",  "integer", "long",   "short",  "i8",   "i16",
      "i32",  "i64",     "float",  "double", "f32",  "f64",
      "bool", "str",     "string", "char",   "void", "ptr"};
  for (auto &p : primitives)
    if (p == name)
      return true;
  return structs_.count(name) > 0;
}

bool TypeChecker::isAssignable(const NexusType &from,
                               const NexusType &to) const {
  if (from == to)
    return true;
  if (from.base == "null" && to.isPtr)
    return true;
  if (from.base == "int" && to.base == "float" && from.dims == 0 &&
      to.dims == 0)
    return true;
  return false;
}

// --------------------------------- //
//  First pass register declarations //
// --------------------------------- //

void TypeChecker::registerStructs(const Program &prog) {
  for (auto &sd : prog.structs) {
    if (structs_.count(sd->name)) {
      error("Duplicate struct declaration: '" + sd->name + "'");
      continue;
    }
    std::vector<std::pair<std::string, NexusType>> fields;
    for (auto &f : sd->fields) {
      NexusType ft = NexusType::fromTypeDesc(f.type);
      if (!typeExists(ft.base))
        error("Struct '" + sd->name + "' field '" + f.name +
              "' has unknown type '" + ft.base + "'");
      fields.push_back({f.name, ft});
    }
    structs_[sd->name] = std::move(fields);
  }
}

void TypeChecker::registerFunctions(const Program &prog) {
  for (auto &fn : prog.functions) {
    const std::string &nm = fn->name.token.getWord();
    if (funcs_.count(nm)) {
      error("Duplicate function declaration: '" + nm + "'");
      continue;
    }
    FuncSig sig;
    sig.ret = NexusType::fromTypeDesc(fn->returnType);
    for (auto &p : fn->params)
      sig.params.push_back(NexusType::fromTypeDesc(p.type));
    funcs_[nm] = std::move(sig);
  }
}

void TypeChecker::registerExterns(const Program &prog) {
  for (auto &eb : prog.externBlocks) {
    for (auto &decl : eb.decls) {
      if (funcs_.count(decl.name))
        continue; // allow shadowing by user fn
      FuncSig sig;
      sig.ret = NexusType::fromTypeDesc(decl.returnType);
      for (auto &pt : decl.paramTypes)
        sig.params.push_back(NexusType::fromTypeDesc(pt));
      funcs_[decl.name] = std::move(sig);
    }
  }
}

void TypeChecker::registerGlobals(const Program &prog) {
  pushScope(); // global scope lives at the bottom of the stack
  for (auto &gv : prog.globals) {
    NexusType gt = NexusType::fromTypeDesc(gv->type);
    if (!typeExists(gt.base))
      error("Global '" + gv->name + "' has unknown type '" + gt.base + "'");
    if (gv->init) {
      NexusType it = inferExpr(*gv->init);
      if (!isAssignable(it, gt))
        error("Global '" + gv->name + "': initialiser type '" + it.str() +
              "' is not assignable to declared type '" + gt.str() + "'");
    }
    declareVar(gv->name, gt);
  }
}

// ------------------ //
//  Entry point       //
// ------------------ //

bool TypeChecker::check(const Program &prog) {
  errors_.clear();
  scopes_.clear();

  registerStructs(prog);
  registerFunctions(prog);
  registerExterns(prog);
  registerGlobals(prog);

  for (auto &fn : prog.functions)
    checkFunction(*fn);

  return errors_.empty();
}

// ---------------------- //
//  Function checking     //
// ---------------------- //

void TypeChecker::checkFunction(const Function &fn) {
  currentReturnType_ = NexusType::fromTypeDesc(fn.returnType);

  pushScope();
  for (auto &p : fn.params) {
    NexusType pt = NexusType::fromTypeDesc(p.type);
    if (!typeExists(pt.base))
      error("Function '" + fn.name.token.getWord() + "' parameter '" +
            p.name.token.getWord() + "' has unknown type '" + pt.base + "'");
    declareVar(p.name.token.getWord(), pt);
  }

  if (fn.body)
    checkBlock(*fn.body);

  popScope();
}

// ------------------ //
//  Block / Statement //
// ------------------ //

void TypeChecker::checkBlock(const Block &block) {
  pushScope();
  for (auto &stmt : block.statements)
    checkStatement(*stmt);
  popScope();
}

void TypeChecker::checkStatement(const Statement &stmt) {
  if (auto *s = dynamic_cast<const VarDecl *>(&stmt))
    return checkVarDecl(*s);
  if (auto *s = dynamic_cast<const ExprStmt *>(&stmt))
    return checkExprStmt(*s);
  if (auto *s = dynamic_cast<const IfStmt *>(&stmt))
    return checkIfStmt(*s);
  if (auto *s = dynamic_cast<const WhileStmt *>(&stmt))
    return checkWhileStmt(*s);
  if (auto *s = dynamic_cast<const ForRangeStmt *>(&stmt))
    return checkForRange(*s);
  if (auto *s = dynamic_cast<const Return *>(&stmt))
    return checkReturn(*s);
  // Break / Continue have no types to check.
}

void TypeChecker::checkVarDecl(const VarDecl &s) {
  NexusType declared = NexusType::fromTypeDesc(s.type);
  if (!typeExists(declared.base))
    error("Variable '" + s.name.token.getWord() + "' has unknown type '" +
          declared.base + "'");

  if (s.initializer) {
    NexusType init = inferExpr(*s.initializer);
    if (!isAssignable(init, declared))
      error("Variable '" + s.name.token.getWord() + "': initialiser type '" +
            init.str() + "' is not assignable to declared type '" +
            declared.str() + "'");
  }
  declareVar(s.name.token.getWord(), declared);
}

void TypeChecker::checkExprStmt(const ExprStmt &s) {
  if (s.expr)
    inferExpr(*s.expr);
}

void TypeChecker::checkIfStmt(const IfStmt &s) {
  NexusType cond = inferExpr(*s.condition);
  if (cond.base != "bool" || cond.dims != 0)
    error("If-condition must be 'bool', got '" + cond.str() + "'");
  if (s.thenBranch)
    checkBlock(*s.thenBranch);
  if (s.elseBranch)
    checkBlock(*s.elseBranch);
}

void TypeChecker::checkWhileStmt(const WhileStmt &s) {
  NexusType cond = inferExpr(*s.condition);
  if (cond.base != "bool" || cond.dims != 0)
    error("While-condition must be 'bool', got '" + cond.str() + "'");
  if (s.doBranch)
    checkBlock(*s.doBranch);
}

void TypeChecker::checkForRange(const ForRangeStmt &s) {
  pushScope();

  NexusType varTy = NexusType::fromTypeDesc(s.varType);
  if (!typeExists(varTy.base))
    error("For-range variable '" + s.varName.token.getWord() +
          "' has unknown type '" + varTy.base + "'");

  if (s.start) {
    NexusType st = inferExpr(*s.start);
    if (!st.isIntegral())
      error("For-range start must be 'int', got '" + st.str() + "'");
  }
  if (s.end) {
    NexusType en = inferExpr(*s.end);
    if (!en.isIntegral())
      error("For-range end must be 'int', got '" + en.str() + "'");
  }
  if (s.step) {
    NexusType sp = inferExpr(*s.step);
    if (!sp.isIntegral())
      error("For-range step must be 'int', got '" + sp.str() + "'");
  }

  declareVar(s.varName.token.getWord(), varTy);
  if (s.body)
    checkBlock(*s.body);

  popScope();
}

void TypeChecker::checkReturn(const Return &s) {
  if (!s.value) {
    if (!currentReturnType_.isVoid())
      error("Return without value in non-void function (expected '" +
            currentReturnType_.str() + "')");
    return;
  }
  NexusType val = inferExpr(**s.value);
  if (!isAssignable(val, currentReturnType_))
    error("Return type mismatch: got '" + val.str() + "', expected '" +
          currentReturnType_.str() + "'");
}

// --------------------------- //
//  Expression inference       //
// --------------------------- //

NexusType TypeChecker::inferExpr(const Expression &expr) {
  if (auto *e = dynamic_cast<const IntLitExpr *>(&expr))
    return inferIntLit(*e);
  if (auto *e = dynamic_cast<const FloatLitExpr *>(&expr))
    return inferFloatLit(*e);
  if (auto *e = dynamic_cast<const StrLitExpr *>(&expr))
    return inferStrLit(*e);
  if (auto *e = dynamic_cast<const BoolLitExpr *>(&expr))
    return inferBoolLit(*e);
  if (auto *e = dynamic_cast<const CharLitExpr *>(&expr))
    return inferCharLit(*e);
  if (auto *e = dynamic_cast<const NullLitExpr *>(&expr))
    return inferNullLit(*e);
  if (auto *e = dynamic_cast<const IdentExpr *>(&expr))
    return inferIdent(*e);
  if (auto *e = dynamic_cast<const BinaryExpr *>(&expr))
    return inferBinary(*e);
  if (auto *e = dynamic_cast<const UnaryExpr *>(&expr))
    return inferUnary(*e);
  if (auto *e = dynamic_cast<const CallExpr *>(&expr))
    return inferCall(*e);
  if (auto *e = dynamic_cast<const AssignExpr *>(&expr))
    return inferAssign(*e);
  if (auto *e = dynamic_cast<const Increment *>(&expr))
    return inferIncDec(e->target.token.getWord());
  if (auto *e = dynamic_cast<const Decrement *>(&expr))
    return inferIncDec(e->target.token.getWord());
  if (auto *e = dynamic_cast<const NewArrayExpr *>(&expr))
    return inferNewArray(*e);
  if (auto *e = dynamic_cast<const ArrayIndexExpr *>(&expr))
    return inferArrayIndex(*e);
  if (auto *e = dynamic_cast<const ArrayIndexAssignExpr *>(&expr))
    return inferArrayIndexAssign(*e);
  if (auto *e = dynamic_cast<const LengthPropertyExpr *>(&expr))
    return inferLengthProp(*e);
  if (auto *e = dynamic_cast<const IndexedLengthExpr *>(&expr))
    return inferIndexedLength(*e);
  if (auto *e = dynamic_cast<const FieldAccessExpr *>(&expr))
    return inferFieldAccess(*e);
  if (auto *e = dynamic_cast<const FieldAssignExpr *>(&expr))
    return inferFieldAssign(*e);
  if (auto *e = dynamic_cast<const StructLitExpr *>(&expr))
    return inferStructLit(*e);

  error("Unknown expression kind encountered");
  return NexusType::make("error");
}

// ------------------- //
//  Literal inference  //
// ------------------- //

NexusType TypeChecker::inferIntLit(const IntLitExpr &) {
  return NexusType::make("int");
}
NexusType TypeChecker::inferFloatLit(const FloatLitExpr &) {
  return NexusType::make("float");
}
NexusType TypeChecker::inferStrLit(const StrLitExpr &) {
  return NexusType::make("str");
}
NexusType TypeChecker::inferBoolLit(const BoolLitExpr &) {
  return NexusType::make("bool");
}
NexusType TypeChecker::inferCharLit(const CharLitExpr &) {
  return NexusType::make("char");
}
NexusType TypeChecker::inferNullLit(const NullLitExpr &) {
  return NexusType::make("null", 0, true);
}

// ---------------- //
//  Identifier      //
// ---------------- //

NexusType TypeChecker::inferIdent(const IdentExpr &e) {
  const std::string &nm = e.name.token.getWord();
  auto opt = lookupVar(nm);
  if (!opt) {
    error("Use of undeclared variable '" + nm + "'");
    return NexusType::make("error");
  }
  return *opt;
}

// ------------ //
//  Binary      //
// ------------ //

NexusType TypeChecker::inferBinary(const BinaryExpr &e) {
  NexusType L = inferExpr(*e.left);
  NexusType R = inferExpr(*e.right);

  switch (e.op) {
  // Arithmetic
  case BinaryOp::Add:
  case BinaryOp::Sub:
  case BinaryOp::Mul:
  case BinaryOp::Div:
  case BinaryOp::DivFloor:
  case BinaryOp::Mod: {
    if (e.op == BinaryOp::Add && L.base == "str" && R.base == "str")
      return NexusType::make("str"); // string concatenation
    if (!L.isNumeric() || !R.isNumeric()) {
      error("Arithmetic operator '" + toString(e.op) +
            "' requires numeric operands, got '" + L.str() + "' and '" +
            R.str() + "'");
      return NexusType::make("error");
    }
    return (L.base == "float" || R.base == "float") ? NexusType::make("float")
                                                    : NexusType::make("int");
  }

  case BinaryOp::Eq:
  case BinaryOp::Ne:
    if (L != R && !(L.isNumeric() && R.isNumeric()))
      error("Equality operator applied to incompatible types '" + L.str() +
            "' and '" + R.str() + "'");
    return NexusType::make("bool");

  case BinaryOp::Lt:
  case BinaryOp::Gt:
  case BinaryOp::Le:
  case BinaryOp::Ge:
    if (!L.isNumeric() || !R.isNumeric())
      error("Comparison operator '" + toString(e.op) +
            "' requires numeric operands, got '" + L.str() + "' and '" +
            R.str() + "'");
    return NexusType::make("bool");

  case BinaryOp::And:
  case BinaryOp::Or:
    if (L.base != "bool" || R.base != "bool")
      error("Logical operator '" + toString(e.op) +
            "' requires bool operands, got '" + L.str() + "' and '" + R.str() +
            "'");
    return NexusType::make("bool");

  case BinaryOp::BitAnd:
    if (!L.isIntegral() || !R.isIntegral())
      error("BitAnd requires 'int' operands, got '" + L.str() + "' and '" +
            R.str() + "'");
    return NexusType::make("int");
  }
  return NexusType::make("error");
}

// ----------- //
//  Unary      //
// ----------- //

NexusType TypeChecker::inferUnary(const UnaryExpr &e) {
  NexusType op = inferExpr(*e.operand);
  switch (e.op) {
  case UnaryOp::Negate:
    if (!op.isNumeric()) {
      error("Unary negate requires numeric operand, got '" + op.str() + "'");
      return NexusType::make("error");
    }
    return op;
  case UnaryOp::Not:
    if (op.base != "bool" || op.dims != 0) {
      error("Unary not requires 'bool' operand, got '" + op.str() + "'");
      return NexusType::make("error");
    }
    return NexusType::make("bool");
  }
  return NexusType::make("error");
}

// --------- //
//  Call     //
// --------- //

NexusType TypeChecker::inferCall(const CallExpr &e) {
  const std::string &nm = e.callee.token.getWord();
  auto it = funcs_.find(nm);
  if (it == funcs_.end()) {
    error("Call to undeclared function '" + nm + "'");
    return NexusType::make("error");
  }

  const FuncSig &sig = it->second;

  if (!sig.params.empty() && e.arguments.size() != sig.params.size()) {
    error("Function '" + nm + "' expects " + std::to_string(sig.params.size()) +
          " argument(s), got " + std::to_string(e.arguments.size()));
  } else {
    for (size_t i = 0; i < e.arguments.size() && i < sig.params.size(); ++i) {
      NexusType at = inferExpr(*e.arguments[i]);
      if (!isAssignable(at, sig.params[i]))
        error("Function '" + nm + "' argument " + std::to_string(i + 1) +
              ": expected '" + sig.params[i].str() + "', got '" + at.str() +
              "'");
    }
  }

  return sig.ret;
}

// --------------- //
//  Assignment     //
// --------------- //

NexusType TypeChecker::inferAssign(const AssignExpr &e) {
  const std::string &nm = e.target.token.getWord();
  auto opt = lookupVar(nm);
  if (!opt) {
    error("Assignment to undeclared variable '" + nm + "'");
    return NexusType::make("error");
  }
  NexusType val = inferExpr(*e.value);
  if (!isAssignable(val, *opt))
    error("Cannot assign '" + val.str() + "' to '" + opt->str() + "' ('" + nm +
          "')");
  return *opt;
}

// ----------------------- //
//  Increment / Decrement  //
// ----------------------- //

NexusType TypeChecker::inferIncDec(const std::string &varName) {
  auto opt = lookupVar(varName);
  if (!opt) {
    error("++/-- on undeclared variable '" + varName + "'");
    return NexusType::make("error");
  }
  if (!opt->isNumeric())
    error("++/-- requires numeric variable, got '" + opt->str() + "'");
  return *opt;
}

// ----------- //
//  Arrays     //
// ----------- //

NexusType TypeChecker::inferNewArray(const NewArrayExpr &e) {
  for (auto &sz : e.sizes) {
    NexusType st = inferExpr(*sz);
    if (!st.isIntegral())
      error("Array size must be 'int', got '" + st.str() + "'");
  }
  return NexusType::fromTypeDesc(e.arrayType);
}

NexusType TypeChecker::inferArrayIndex(const ArrayIndexExpr &e) {
  const std::string &nm = e.array.token.getWord();
  auto opt = lookupVar(nm);
  if (!opt) {
    error("Array index on undeclared variable '" + nm + "'");
    return NexusType::make("error");
  }
  if (!opt->isArray()) {
    error("Cannot index non-array variable '" + nm + "' (type '" + opt->str() +
          "')");
    return NexusType::make("error");
  }
  for (auto &idx : e.indices) {
    NexusType it = inferExpr(*idx);
    if (!it.isIntegral())
      error("Array index must be 'int', got '" + it.str() + "'");
  }
  int remaining = opt->dims - static_cast<int>(e.indices.size());
  if (remaining < 0) {
    error("Too many indices for array '" + nm + "'");
    return NexusType::make("error");
  }
  return NexusType::make(opt->base, remaining, opt->isPtr);
}

NexusType TypeChecker::inferArrayIndexAssign(const ArrayIndexAssignExpr &e) {
  const std::string &nm = e.array.token.getWord();
  auto opt = lookupVar(nm);
  if (!opt) {
    error("Array index assign on undeclared variable '" + nm + "'");
    return NexusType::make("error");
  }
  if (!opt->isArray())
    error("Cannot index non-array variable '" + nm + "'");

  for (auto &idx : e.indices) {
    NexusType it = inferExpr(*idx);
    if (!it.isIntegral())
      error("Array index must be 'int', got '" + it.str() + "'");
  }

  NexusType elemTy = NexusType::make(
      opt->base, opt->dims - static_cast<int>(e.indices.size()));
  NexusType val = inferExpr(*e.value);
  if (!isAssignable(val, elemTy))
    error("Array element assign: expected '" + elemTy.str() + "', got '" +
          val.str() + "'");
  return elemTy;
}

// ----------- //
//  Length     //
// ----------- //

NexusType TypeChecker::inferLengthProp(const LengthPropertyExpr &e) {
  const std::string &nm = e.name.token.getWord();
  auto opt = lookupVar(nm);
  if (!opt) {
    error("'.length' on undeclared variable '" + nm + "'");
    return NexusType::make("int");
  }
  if (!opt->isArray() && opt->base != "str")
    error("'.length' is only valid on arrays or strings, got '" + opt->str() +
          "'");
  return NexusType::make("int");
}

NexusType TypeChecker::inferIndexedLength(const IndexedLengthExpr &e) {
  const std::string &nm = e.arrayName.token.getWord();
  auto opt = lookupVar(nm);
  if (!opt) {
    error("Indexed '.length' on undeclared variable '" + nm + "'");
    return NexusType::make("int");
  }
  for (auto &idx : e.indices) {
    NexusType it = inferExpr(*idx);
    if (!it.isIntegral())
      error("Indexed length index must be 'int', got '" + it.str() + "'");
  }
  return NexusType::make("int");
}

// --------------------------- //
//  Struct field access/assign //
// --------------------------- //

NexusType TypeChecker::inferFieldAccess(const FieldAccessExpr &e) {
  NexusType objTy = inferExpr(*e.object);
  auto sit = structs_.find(objTy.base);
  if (sit == structs_.end()) {
    error("Field access on non-struct type '" + objTy.str() + "'");
    return NexusType::make("error");
  }
  for (auto &[fname, ftype] : sit->second) {
    if (fname == e.field)
      return ftype;
  }
  error("Struct '" + objTy.base + "' has no field '" + e.field + "'");
  return NexusType::make("error");
}

NexusType TypeChecker::inferFieldAssign(const FieldAssignExpr &e) {
  NexusType objTy = inferExpr(*e.object);
  NexusType valTy = inferExpr(*e.value);
  auto sit = structs_.find(objTy.base);
  if (sit == structs_.end()) {
    error("Field assign on non-struct type '" + objTy.str() + "'");
    return NexusType::make("error");
  }
  for (auto &[fname, ftype] : sit->second) {
    if (fname == e.field) {
      if (!isAssignable(valTy, ftype))
        error("Field '" + e.field + "' of struct '" + objTy.base +
              "': expected '" + ftype.str() + "', got '" + valTy.str() + "'");
      return ftype;
    }
  }
  error("Struct '" + objTy.base + "' has no field '" + e.field + "'");
  return NexusType::make("error");
}

// ------------------ //
//  Struct literal    //
// ------------------ //

NexusType TypeChecker::inferStructLit(const StructLitExpr &e) {
  auto sit = structs_.find(e.typeName);
  if (sit == structs_.end()) {
    error("Unknown struct type '" + e.typeName + "' in struct literal");
    return NexusType::make("error");
  }
  const auto &fields = sit->second;
  if (e.values.size() != fields.size())
    error("Struct '" + e.typeName + "' has " + std::to_string(fields.size()) +
          " field(s), literal provides " + std::to_string(e.values.size()));

  for (size_t i = 0; i < e.values.size() && i < fields.size(); ++i) {
    NexusType vt = inferExpr(*e.values[i]);
    if (!isAssignable(vt, fields[i].second))
      error("Struct '" + e.typeName + "' field '" + fields[i].first +
            "': expected '" + fields[i].second.str() + "', got '" + vt.str() +
            "'");
  }
  return NexusType::make(e.typeName);
}
