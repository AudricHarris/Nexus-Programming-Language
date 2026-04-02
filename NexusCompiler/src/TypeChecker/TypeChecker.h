#ifndef TYPE_CHECKER_H
#define TYPE_CHECKER_H

#include "../AST/AST.h"

#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

// ---------------------------------------------------------- //
//  NexusType semantic type carried through the type-checker  //
// ---------------------------------------------------------- //

struct NexusType {
  std::string base;
  int dims = 0;
  bool isPtr = false;

  static NexusType make(std::string b, int d = 0, bool ptr = false) {
    NexusType t;
    t.base = std::move(b);
    t.dims = d;
    t.isPtr = ptr;
    return t;
  }
  static NexusType fromTypeDesc(const TypeDesc &td) {
    return make(td.isPtr ? "ptr" : td.base.token.getWord(), td.dimensions,
                td.isPtr);
  }

  bool isNumeric() const {
    if (dims != 0)
      return false;
    return base == "int" || base == "integer" || base == "long" ||
           base == "short" || base == "i8" || base == "i16" || base == "i32" ||
           base == "i64" || base == "float" || base == "double" ||
           base == "f32" || base == "f64";
  }

  bool isIntegral() const {
    if (dims != 0)
      return false;
    return base == "int" || base == "integer" || base == "long" ||
           base == "short" || base == "i8" || base == "i16" || base == "i32" ||
           base == "i64";
  }

  bool isArray() const { return dims > 0; }
  bool isVoid() const { return base == "void" && dims == 0; }

  bool operator==(const NexusType &o) const {
    return base == o.base && dims == o.dims && isPtr == o.isPtr;
  }
  bool operator!=(const NexusType &o) const { return !(*this == o); }

  std::string str() const {
    if (isPtr)
      return "ptr";
    std::string s = base;
    for (int i = 0; i < dims; ++i)
      s = "array." + s;
    return s;
  }
};

// ----------- //
//  TypeError  //
// ----------- //

struct TypeError : std::runtime_error {
  explicit TypeError(const std::string &msg) : std::runtime_error(msg) {}
};

// ------------- //
//  TypeChecker  //
// ------------- //

class TypeChecker {
public:
  bool check(const Program &prog);

  const std::vector<std::string> &errors() const { return errors_; }
  bool hasErrors() const { return !errors_.empty(); }

private:
  struct FuncSig {
    std::vector<NexusType> params;
    NexusType ret;
  };

  std::unordered_map<std::string, FuncSig> funcs_;
  std::unordered_map<std::string,
                     std::vector<std::pair<std::string, NexusType>>>
      structs_;

  using Scope = std::unordered_map<std::string, NexusType>;
  std::vector<Scope> scopes_;

  NexusType currentReturnType_;

  std::vector<std::string> errors_;

  void pushScope() { scopes_.emplace_back(); }
  void popScope() { scopes_.pop_back(); }

  void declareVar(const std::string &name, const NexusType &type);
  std::optional<NexusType> lookupVar(const std::string &name) const;

  void error(const std::string &msg) { errors_.push_back(msg); }
  bool typeExists(const std::string &name) const;
  bool isAssignable(const NexusType &from, const NexusType &to) const;

  void registerStructs(const Program &prog);
  void registerFunctions(const Program &prog);
  void registerExterns(const Program &prog);
  void registerGlobals(const Program &prog);

  void checkFunction(const Function &fn);
  void checkBlock(const Block &block);
  void checkStatement(const Statement &stmt);

  void checkVarDecl(const VarDecl &s);
  void checkExprStmt(const ExprStmt &s);
  void checkIfStmt(const IfStmt &s);
  void checkWhileStmt(const WhileStmt &s);
  void checkForRange(const ForRangeStmt &s);
  void checkReturn(const Return &s);

  NexusType inferExpr(const Expression &expr);

  NexusType inferIntLit(const IntLitExpr &e);
  NexusType inferFloatLit(const FloatLitExpr &e);
  NexusType inferStrLit(const StrLitExpr &e);
  NexusType inferBoolLit(const BoolLitExpr &e);
  NexusType inferCharLit(const CharLitExpr &e);
  NexusType inferNullLit(const NullLitExpr &e);
  NexusType inferIdent(const IdentExpr &e);
  NexusType inferBinary(const BinaryExpr &e);
  NexusType inferUnary(const UnaryExpr &e);
  NexusType inferCall(const CallExpr &e);
  NexusType inferAssign(const AssignExpr &e);
  NexusType inferIncDec(const std::string &varName);
  NexusType inferNewArray(const NewArrayExpr &e);
  NexusType inferArrayIndex(const ArrayIndexExpr &e);
  NexusType inferArrayIndexAssign(const ArrayIndexAssignExpr &e);
  NexusType inferLengthProp(const LengthPropertyExpr &e);
  NexusType inferIndexedLength(const IndexedLengthExpr &e);
  NexusType inferFieldAccess(const FieldAccessExpr &e);
  NexusType inferFieldAssign(const FieldAssignExpr &e);
  NexusType inferStructLit(const StructLitExpr &e);

  struct ExprInferVisitor;
};

#endif // TYPE_CHECKER_H
