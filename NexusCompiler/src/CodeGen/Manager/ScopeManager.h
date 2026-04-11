#ifndef SCOPE_MANAGER_H
#define SCOPE_MANAGER_H

#include "../VarInfo.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include <map>
#include <string>
#include <vector>

class ScopeManager {
public:
  ScopeManager(llvm::IRBuilder<> &B, llvm::LLVMContext &ctx, llvm::Module *M,
               std::map<std::string, VarInfo> &namedValues);

  void reset();
  void pushScope();
  void declare(const std::string &name);

  void declareTmp(llvm::AllocaInst *alloca, llvm::Type *ty);

  bool isTmp(llvm::AllocaInst *alloca) const;

  std::vector<std::string> popScope();
  void popAll();

  void emitAllDestructors();

  size_t depth() const { return stack_.size(); }
  bool isLocal(const std::string &name) const;

private:
  llvm::IRBuilder<> &B_;
  llvm::LLVMContext &ctx_;
  llvm::Module *M_;
  std::map<std::string, VarInfo> &namedValues_;

  std::vector<std::vector<std::string>> stack_;
  std::vector<std::vector<VarInfo>> tmpStack_;

  void emitDestructorsFor(const std::vector<std::string> &names);
  void emitDestructor(VarInfo &vi);
  void emitArrayFree(llvm::Value *arrPtr, llvm::StructType *arrSt, int depth);

  static bool isValidPointer(llvm::Value *ptr);
  llvm::Function *getFree();
};

#endif // SCOPE_MANAGER_H
