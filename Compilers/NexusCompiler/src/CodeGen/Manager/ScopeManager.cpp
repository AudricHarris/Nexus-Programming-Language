#include "ScopeManager.h"
#include "../TypeResolver.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include <llvm/IR/BasicBlock.h>
#include <string>

using namespace llvm;

static bool blockHasTerminator(llvm::IRBuilder<> &B) {
  llvm::BasicBlock *bb = B.GetInsertBlock();
  if (!bb || bb->empty())
    return false;
  return bb->back().isTerminator();
}

// ------------ //
// Construction //
// ------------ //

ScopeManager::ScopeManager(IRBuilder<> &B, LLVMContext &ctx, Module *M,
                           std::map<std::string, VarInfo> &namedValues)
    : B_(B), ctx_(ctx), M_(M), namedValues_(namedValues) {}

// --------------- //
// Scope lifecycle //
// --------------- //

void ScopeManager::reset() {
  stack_.clear();
  tmpStack_.clear();
  destructorsEmitted_ = false;
  bbCounter_ = 0;
}

void ScopeManager::pushScope() {
  stack_.emplace_back();
  tmpStack_.emplace_back();
}

void ScopeManager::declare(const std::string &name) {
  if (!stack_.empty())
    stack_.back().push_back(name);
}

void ScopeManager::declareTmp(llvm::AllocaInst *alloca, llvm::Type *ty) {
  if (tmpStack_.empty())
    return;
  VarInfo vi(alloca, ty, false, false, false, false);
  vi.ownsHeap = true;
  tmpStack_.back().push_back(vi);
}

bool ScopeManager::isTmp(llvm::AllocaInst *alloca) const {
  for (const auto &level : tmpStack_)
    for (const auto &vi : level)
      if (vi.allocaInst == alloca)
        return true;
  return false;
}

std::vector<std::string> ScopeManager::popScope() {
  if (stack_.empty())
    return {};

  if (destructorsEmitted_) {
    if (!tmpStack_.empty())
      tmpStack_.pop_back();
    auto names = stack_.back();
    stack_.pop_back();
    return names;
  }

  bool canEmit = !blockHasTerminator(B_);

  if (!tmpStack_.empty()) {
    if (canEmit)
      for (auto &vi : tmpStack_.back())
        emitDestructor(vi);
    tmpStack_.pop_back();
  }
  auto names = stack_.back();
  if (canEmit)
    emitDestructorsFor(names);
  stack_.pop_back();
  return names;
}

void ScopeManager::popAll() {
  if (destructorsEmitted_) {
    stack_.clear();
    tmpStack_.clear();
    return;
  }
  while (!stack_.empty())
    popScope();
}

void ScopeManager::emitAllDestructors() {
  if (destructorsEmitted_)
    return;
  destructorsEmitted_ = true;

  for (int i = static_cast<int>(stack_.size()) - 1; i >= 0; --i) {
    if (i < static_cast<int>(tmpStack_.size()))
      for (auto &vi : tmpStack_[i])
        emitDestructor(vi);
    emitDestructorsFor(stack_[i]);
  }
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
    if (blockHasTerminator(B_))
      return;

    auto nv = namedValues_.find(*it);
    if (nv == namedValues_.end())
      continue;

    VarInfo &vi = nv->second;

    if (!vi.ownsHeap)
      continue;
    if (vi.isMoved || vi.isBorrowed || vi.isReference)
      continue;

    emitDestructor(vi);
  }
}

void ScopeManager::emitDestructor(VarInfo &vi) {
  if (blockHasTerminator(B_))
    return;
  Type *ty = vi.type;
  if (!ty)
    return;

  // Plain struct (not a string or array wrapper): recursively walk all fields.
  // This handles arbitrarily deep nesting, e.g. Option<Animal> → Animal → str.
  if (ty->isStructTy() && !TypeResolver::isString(ty) &&
      !TypeResolver::isArray(ty)) {
    emitStructFieldDestructors(cast<StructType>(ty), vi.allocaInst);
    return;
  }

  if (TypeResolver::isString(ty)) {
    StructType *st = cast<StructType>(ty);
    Value *load = B_.CreateLoad(st, vi.allocaInst);
    Value *data = B_.CreateExtractValue(load, {0});

    if (!isValidPointer(data))
      return;

    llvm::Function *fn = B_.GetInsertBlock()->getParent();
    std::string uid = std::to_string(bbCounter_++);
    BasicBlock *freeBB = BasicBlock::Create(ctx_, "str.free" + uid, fn);
    BasicBlock *skipBB = BasicBlock::Create(ctx_, "str.skip" + uid, fn);
    Value *isNull = B_.CreateICmpEQ(
        data, llvm::ConstantPointerNull::get(llvm::PointerType::get(ctx_, 0)),
        "is.null");
    B_.CreateCondBr(isNull, skipBB, freeBB);

    B_.SetInsertPoint(freeBB);
    B_.CreateCall(getFree(), {data});
    B_.CreateBr(skipBB);

    B_.SetInsertPoint(skipBB);
    return;
  }

  if (TypeResolver::isArray(ty)) {
    if (vi.allocaInst)
      emitArrayFree(vi.allocaInst, cast<StructType>(ty), 0);
    return;
  }
}

// Recursively emits null-guarded free() calls for all heap-owning fields of a
// struct, descending into nested structs so that deeply-nested strings and
// arrays (e.g. Option<Animal> → Animal → str name) are always cleaned up.
// Called both from emitDestructor (for named variables) and from emitArrayFree
// (for struct elements stored inside an array).
void ScopeManager::emitStructFieldDestructors(llvm::StructType *st,
                                              llvm::Value *ptr) {
  // Detect whether `st` itself is an enum/tagged-union layout: field 0 is an
  // i32 tag, and at least one *payload* field (index >= 1) owns heap memory
  // (string or array, possibly nested). Such payload fields are only valid
  // when the active variant is the "has payload" one (tag == 1, e.g. Some).
  // For other variants (e.g. None) those bytes are uninitialized/leftover
  // stack garbage and must never be read as a pointer and freed.
  bool selfIsEnum = false;
  if (st->getNumElements() >= 1 && st->getElementType(0)->isIntegerTy(32)) {
    std::function<bool(llvm::Type *)> ownsHeap = [&](llvm::Type *t) -> bool {
      if (TypeResolver::isString(t) || TypeResolver::isArray(t))
        return true;
      if (auto *nested = llvm::dyn_cast<llvm::StructType>(t)) {
        for (unsigned j = 0; j < nested->getNumElements(); ++j)
          if (ownsHeap(nested->getElementType(j)))
            return true;
      }
      return false;
    };
    for (unsigned i = 1; i < st->getNumElements(); ++i) {
      if (ownsHeap(st->getElementType(i))) {
        selfIsEnum = true;
        break;
      }
    }
  }

  llvm::BasicBlock *enumSkipBB = nullptr;
  if (selfIsEnum) {
    Value *tagPtr = B_.CreateStructGEP(st, ptr, 0, "self.tag.ptr");
    Value *tag = B_.CreateLoad(Type::getInt32Ty(ctx_), tagPtr, "self.tag");
    Value *isSome = B_.CreateICmpEQ(
        tag, ConstantInt::get(Type::getInt32Ty(ctx_), 1), "self.is.some");

    llvm::Function *fn = B_.GetInsertBlock()->getParent();
    std::string uid = std::to_string(bbCounter_++);
    llvm::BasicBlock *someBB = BasicBlock::Create(ctx_, "self.some" + uid, fn);
    enumSkipBB = BasicBlock::Create(ctx_, "self.skip" + uid, fn);
    B_.CreateCondBr(isSome, someBB, enumSkipBB);
    B_.SetInsertPoint(someBB);
  }

  for (unsigned i = selfIsEnum ? 1 : 0; i < st->getNumElements(); ++i) {
    if (blockHasTerminator(B_))
      break;

    Type *fieldTy = st->getElementType(i);
    Value *fieldPtr =
        B_.CreateStructGEP(st, ptr, i, "field.dtor." + std::to_string(i));

    if (TypeResolver::isString(fieldTy)) {
      Value *load = B_.CreateLoad(cast<StructType>(fieldTy), fieldPtr);
      Value *data = B_.CreateExtractValue(load, {0});
      llvm::Function *fn = B_.GetInsertBlock()->getParent();
      std::string uid = std::to_string(bbCounter_++);
      BasicBlock *freeBB = BasicBlock::Create(ctx_, "str.free" + uid, fn);
      BasicBlock *skipBB = BasicBlock::Create(ctx_, "str.skip" + uid, fn);
      Value *isNull = B_.CreateICmpEQ(
          data, ConstantPointerNull::get(PointerType::get(ctx_, 0)), "is.null");
      B_.CreateCondBr(isNull, skipBB, freeBB);
      B_.SetInsertPoint(freeBB);
      B_.CreateCall(getFree(), {data});
      B_.CreateBr(skipBB);
      B_.SetInsertPoint(skipBB);

    } else if (TypeResolver::isArray(fieldTy)) {
      emitArrayFree(fieldPtr, cast<StructType>(fieldTy), 0);

    } else if (auto *nestedSt = llvm::dyn_cast<llvm::StructType>(fieldTy)) {
      if (nestedSt->getNumElements() >= 1 &&
          nestedSt->getElementType(0)->isIntegerTy(32)) {
        Value *tagPtr = B_.CreateStructGEP(nestedSt, fieldPtr, 0, "tag.ptr");
        Value *tag = B_.CreateLoad(Type::getInt32Ty(ctx_), tagPtr, "tag");
        Value *isSome = B_.CreateICmpEQ(
            tag, ConstantInt::get(Type::getInt32Ty(ctx_), 1), "is.some");

        llvm::Function *fn = B_.GetInsertBlock()->getParent();
        std::string uid = std::to_string(bbCounter_++);
        BasicBlock *someBB = BasicBlock::Create(ctx_, "some" + uid, fn);
        BasicBlock *skipBB = BasicBlock::Create(ctx_, "skip" + uid, fn);
        B_.CreateCondBr(isSome, someBB, skipBB);

        B_.SetInsertPoint(someBB);
        emitStructFieldDestructors(nestedSt, fieldPtr);
        B_.CreateBr(skipBB);

        B_.SetInsertPoint(skipBB);
      } else {
        emitStructFieldDestructors(nestedSt, fieldPtr);
      }
    }
  }

  if (selfIsEnum) {
    if (!blockHasTerminator(B_))
      B_.CreateBr(enumSkipBB);
    B_.SetInsertPoint(enumSkipBB);
  }
}

void ScopeManager::emitArrayFree(llvm::Value *arrPtr, llvm::StructType *arrSt,
                                 int depth) {
  if (!arrPtr || !arrSt)
    return;

  Type *i64 = Type::getInt64Ty(ctx_);
  llvm::Function *fn = B_.GetInsertBlock()->getParent();

  Value *loaded = B_.CreateLoad(arrSt, arrPtr);
  Value *dataPtr = B_.CreateExtractValue(loaded, {1}, "data");
  Value *len = B_.CreateExtractValue(loaded, {0}, "len");

  std::string sfx = std::to_string(depth) + "_" + std::to_string(bbCounter_++);
  BasicBlock *nullCheckPassBB =
      BasicBlock::Create(ctx_, "arr.notnull" + sfx, fn);
  BasicBlock *nullCheckDoneBB =
      BasicBlock::Create(ctx_, "arr.nulldone" + sfx, fn);

  Value *isNull = B_.CreateICmpEQ(
      dataPtr, ConstantPointerNull::get(PointerType::get(ctx_, 0)),
      "arr.isnull");
  B_.CreateCondBr(isNull, nullCheckDoneBB, nullCheckPassBB);
  B_.SetInsertPoint(nullCheckPassBB);

  Type *elemTy = TypeResolver::elemType(ctx_, arrSt);

  if (elemTy && TypeResolver::isArray(elemTy)) {
    StructType *elemSt = cast<StructType>(elemTy);

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

  } else if (elemTy && elemTy->isStructTy() &&
             !TypeResolver::isString(elemTy)) {
    StructType *elemSt = cast<StructType>(elemTy);

    // Recursively check whether any field at any nesting depth owns heap
    // memory. The old flat scan only looked one level deep and missed cases
    // like Option<Animal> where strings live two levels down (Option$Animal ->
    // Animal
    // -> str name/type).
    std::function<bool(llvm::StructType *)> structHasHeap =
        [&](llvm::StructType *s) -> bool {
      for (unsigned i = 0; i < s->getNumElements(); ++i) {
        Type *ft = s->getElementType(i);
        if (TypeResolver::isString(ft) || TypeResolver::isArray(ft))
          return true;
        if (auto *nested = llvm::dyn_cast<llvm::StructType>(ft))
          if (structHasHeap(nested))
            return true;
      }
      return false;
    };

    if (structHasHeap(elemSt)) {
      BasicBlock *loopBB = BasicBlock::Create(ctx_, "sfree.loop" + sfx, fn);
      BasicBlock *bodyBB = BasicBlock::Create(ctx_, "sfree.body" + sfx, fn);
      BasicBlock *afterBB = BasicBlock::Create(ctx_, "sfree.after" + sfx, fn);

      AllocaInst *idx = B_.CreateAlloca(i64, nullptr, "sfi" + sfx);
      B_.CreateStore(ConstantInt::get(i64, 0), idx);
      B_.CreateBr(loopBB);

      B_.SetInsertPoint(loopBB);
      Value *cur = B_.CreateLoad(i64, idx);
      Value *cond = B_.CreateICmpULT(cur, len);
      B_.CreateCondBr(cond, bodyBB, afterBB);

      B_.SetInsertPoint(bodyBB);
      Value *elemPtr = B_.CreateGEP(elemSt, dataPtr, cur, "sslot" + sfx);
      if (elemSt->getNumElements() >= 1 &&
          elemSt->getElementType(0)->isIntegerTy(32)) {
        Value *tagPtr = B_.CreateStructGEP(elemSt, elemPtr, 0, "tag.ptr");
        Value *tag = B_.CreateLoad(Type::getInt32Ty(ctx_), tagPtr, "tag");
        Value *isSome = B_.CreateICmpEQ(
            tag, ConstantInt::get(Type::getInt32Ty(ctx_), 1), "is.some");

        llvm::Function *fn2 = B_.GetInsertBlock()->getParent();
        std::string uid = std::to_string(bbCounter_++);
        BasicBlock *someBB = BasicBlock::Create(ctx_, "some" + uid, fn2);
        BasicBlock *skipBB = BasicBlock::Create(ctx_, "skip" + uid, fn2);
        B_.CreateCondBr(isSome, someBB, skipBB);

        B_.SetInsertPoint(someBB);
        emitStructFieldDestructors(elemSt, elemPtr);
        B_.CreateBr(skipBB);

        B_.SetInsertPoint(skipBB);
      } else {
        emitStructFieldDestructors(elemSt, elemPtr);
      }

      Value *next = B_.CreateAdd(cur, ConstantInt::get(i64, 1));
      B_.CreateStore(next, idx);
      B_.CreateBr(loopBB);

      B_.SetInsertPoint(afterBB);
    }
  }

  B_.CreateCall(getFree(), {dataPtr});

  B_.CreateBr(nullCheckDoneBB);
  B_.SetInsertPoint(nullCheckDoneBB);
}

bool ScopeManager::isValidPointer(llvm::Value *ptr) {
  return ptr != nullptr && !llvm::isa<llvm::ConstantPointerNull>(ptr) &&
         !llvm::isa<llvm::UndefValue>(ptr) &&
         !llvm::isa<llvm::PoisonValue>(ptr);
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
