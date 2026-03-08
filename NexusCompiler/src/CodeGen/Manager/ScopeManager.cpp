#include "ScopeManager.h"
#include "../TypeResolver.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"

using namespace llvm;

// ------------ //
// Construction //
// ------------ //

ScopeManager::ScopeManager(IRBuilder<> &B, LLVMContext &ctx, Module *M,
                           std::map<std::string, VarInfo> &namedValues)
    : B_(B), ctx_(ctx), M_(M), namedValues_(namedValues) {}

// --------------- //
// Scope lifecycle //
// --------------- //

void ScopeManager::pushScope() { stack_.emplace_back(); }

void ScopeManager::declare(const std::string &name) {
  if (!stack_.empty())
    stack_.back().push_back(name);
}

std::vector<std::string> ScopeManager::popScope() {
  if (stack_.empty())
    return {};
  auto names = stack_.back();
  emitDestructorsFor(names);
  stack_.pop_back();
  return names;
}

void ScopeManager::popAll() {
  while (!stack_.empty())
    popScope();
}

// ----------- //
// Depth query //
// ----------- //

bool ScopeManager::isLocal(const std::string &name) const {
  if (stack_.empty())
    return false;
  for (const auto &n : stack_.back())
    if (n == name)
      return true;
  return false;
}

// --------------------------- //
// Private destructor emission //
// --------------------------- //

void ScopeManager::emitDestructorsFor(const std::vector<std::string> &names) {
  for (auto it = names.rbegin(); it != names.rend(); ++it) {
    auto nv = namedValues_.find(*it);
    if (nv == namedValues_.end())
      continue;

    VarInfo &vi = nv->second;

    if (!TypeResolver::isArray(vi.type) && !TypeResolver::isString(vi.type) &&
        !vi.ownsHeap)
      continue;
    if (vi.isMoved || vi.isBorrowed || vi.isReference)
      continue;

    emitDestructor(vi);
  }
}

void ScopeManager::emitDestructor(VarInfo &vi) {
  Type *ty = vi.type;

  if (TypeResolver::isString(ty)) {
    StructType *st = cast<StructType>(ty);
    Value *load = B_.CreateLoad(st, vi.allocaInst);
    Value *data = B_.CreateExtractValue(load, {0});
    B_.CreateCall(getFree(), {data});
    return;
  }

  if (TypeResolver::isArray(ty)) {
    emitArrayFree(vi.allocaInst, cast<StructType>(ty), 0);
    return;
  }
}

void ScopeManager::emitArrayFree(Value *arrPtr, StructType *arrSt, int depth) {
  Type *i64 = Type::getInt64Ty(ctx_);
  llvm::Function *fn = B_.GetInsertBlock()->getParent();

  Value *loaded = B_.CreateLoad(arrSt, arrPtr);
  Value *dataPtr = B_.CreateExtractValue(loaded, {1}, "data");
  Value *len = B_.CreateExtractValue(loaded, {0}, "len");

  Type *elemTy = TypeResolver::elemType(ctx_, arrSt);

  if (elemTy && TypeResolver::isArray(elemTy)) {
    StructType *elemSt = cast<StructType>(elemTy);
    std::string sfx = std::to_string(depth);

    BasicBlock *loopBB = BasicBlock::Create(ctx_, "free.loop" + sfx, fn);
    BasicBlock *bodyBB = BasicBlock::Create(ctx_, "free.body" + sfx, fn);
    BasicBlock *afterBB = BasicBlock::Create(ctx_, "free.after" + sfx, fn);

    AllocaInst *idx = B_.CreateAlloca(i64, nullptr, "fi" + sfx);
    B_.CreateStore(ConstantInt::get(i64, 0), idx);
    B_.CreateBr(loopBB);

    B_.SetInsertPoint(loopBB);
    Value *cur = B_.CreateLoad(i64, idx);
    Value *cond = B_.CreateICmpULT(cur, len);
    B_.CreateCondBr(cond, bodyBB, afterBB);

    B_.SetInsertPoint(bodyBB);
    Value *slotPtr = B_.CreateGEP(elemSt, dataPtr, cur, "slot" + sfx);
    emitArrayFree(slotPtr, elemSt, depth + 1);
    Value *next = B_.CreateAdd(cur, ConstantInt::get(i64, 1));
    B_.CreateStore(next, idx);
    B_.CreateBr(loopBB);

    B_.SetInsertPoint(afterBB);
  }

  B_.CreateCall(getFree(), {dataPtr});
}

llvm::Function *ScopeManager::getFree() {
  llvm::Function *f = M_->getFunction("free");
  if (!f) {
    auto *ft = FunctionType::get(Type::getVoidTy(ctx_),
                                 {PointerType::get(ctx_, 0)}, false);
    f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "free", M_);
  }
  return f;
}
