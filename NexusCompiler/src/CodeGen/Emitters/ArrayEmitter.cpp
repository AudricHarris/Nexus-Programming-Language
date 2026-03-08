#include "ArrayEmitter.h"
#include "../TypeResolver.h"

using namespace llvm;

namespace ArrayEmitter {

// ---------------- //
// Internal helpers //
// ---------------- //

static Value *buildLevel(IRBuilder<> &B, LLVMContext &C, Module &M,
                         Type *elementType, ArrayRef<Value *> dims,
                         unsigned depth);

// ---------- //
// Public API //
// ---------- //

StructType *getArrayStructTy(LLVMContext &C) {
  return StructType::get(Type::getInt64Ty(C), PointerType::get(C, 0));
}

Value *emitMalloc(IRBuilder<> &B, LLVMContext &C, Module &M, Type *allocTy,
                  Value *count) {
  const DataLayout &DL = M.getDataLayout();
  uint64_t size = DL.getTypeAllocSize(allocTy);

  Type *i64Ty = Type::getInt64Ty(C);
  Value *elemSize = ConstantInt::get(i64Ty, size);
  Value *count64 = B.CreateZExt(count, i64Ty);
  Value *total = B.CreateMul(elemSize, count64);

  FunctionCallee mallocFn = M.getOrInsertFunction(
      "malloc", FunctionType::get(PointerType::get(C, 0), {i64Ty}, false));

  return B.CreateCall(mallocFn, {total});
}

Value *makeND(IRBuilder<> &B, LLVMContext &C, Module &M, Type *elementType,
              ArrayRef<Value *> dims) {
  return buildLevel(B, C, M, elementType, dims, 0);
}

void emitArrayFree(IRBuilder<> &B, LLVMContext &C, Module &M, Value *arrPtr,
                   StructType *arrSt, int depth) {
  Type *i64 = Type::getInt64Ty(C);

  // Declare free() if not already present
  llvm::Function *freeF = M.getFunction("free");
  if (!freeF) {
    auto *ft =
        FunctionType::get(Type::getVoidTy(C), {PointerType::get(C, 0)}, false);
    freeF =
        llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "free", M);
  }

  llvm::Function *fn = B.GetInsertBlock()->getParent();

  Value *loaded = B.CreateLoad(arrSt, arrPtr);
  Value *dataPtr = B.CreateExtractValue(loaded, {1}, "data");
  Value *len = B.CreateExtractValue(loaded, {0}, "len");

  Type *elemTy = TypeResolver::elemType(C, arrSt);

  if (elemTy && TypeResolver::isArray(elemTy)) {
    StructType *elemSt = cast<StructType>(elemTy);
    std::string sfx = std::to_string(depth);

    BasicBlock *loopBB = BasicBlock::Create(C, "free.loop" + sfx, fn);
    BasicBlock *bodyBB = BasicBlock::Create(C, "free.body" + sfx, fn);
    BasicBlock *afterBB = BasicBlock::Create(C, "free.after" + sfx, fn);

    AllocaInst *idx = B.CreateAlloca(i64, nullptr, "fi" + sfx);
    B.CreateStore(ConstantInt::get(i64, 0), idx);
    B.CreateBr(loopBB);

    B.SetInsertPoint(loopBB);
    Value *cur = B.CreateLoad(i64, idx);
    Value *cond = B.CreateICmpULT(cur, len);
    B.CreateCondBr(cond, bodyBB, afterBB);

    B.SetInsertPoint(bodyBB);
    Value *slotPtr = B.CreateGEP(elemSt, dataPtr, cur, "slot" + sfx);
    emitArrayFree(B, C, M, slotPtr, elemSt, depth + 1); // recurse
    Value *next = B.CreateAdd(cur, ConstantInt::get(i64, 1));
    B.CreateStore(next, idx);
    B.CreateBr(loopBB);

    B.SetInsertPoint(afterBB);
  }

  B.CreateCall(freeF, {dataPtr});
}

// ------------------------------------------//
// Private — recursive N-dimensional builder //
// ----------------------------------------- //

static Value *buildLevel(IRBuilder<> &B, LLVMContext &C, Module &M,
                         Type *elementType, ArrayRef<Value *> dims,
                         unsigned depth) {
  Type *i64Ty = Type::getInt64Ty(C);
  StructType *arrTy = getArrayStructTy(C);
  Value *len = dims[depth];

  Value *descriptor = B.CreateAlloca(arrTy);

  Value *lenPtr = B.CreateStructGEP(arrTy, descriptor, 0);
  B.CreateStore(B.CreateZExt(len, i64Ty), lenPtr);

  Value *dataPtr = B.CreateStructGEP(arrTy, descriptor, 1);
  bool isLeaf = (depth == dims.size() - 1);

  if (isLeaf) {
    Value *buffer = emitMalloc(B, C, M, elementType, len);
    B.CreateStore(buffer, dataPtr);
  } else {
    StructType *childTy = getArrayStructTy(C);
    Value *childArray = emitMalloc(B, C, M, childTy, len);
    B.CreateStore(childArray, dataPtr);

    llvm::Function *fn = B.GetInsertBlock()->getParent();
    BasicBlock *loopBB = BasicBlock::Create(C, "nd.loop", fn);
    BasicBlock *bodyBB = BasicBlock::Create(C, "nd.body", fn);
    BasicBlock *afterBB = BasicBlock::Create(C, "nd.after", fn);

    Value *index = B.CreateAlloca(i64Ty);
    B.CreateStore(ConstantInt::get(i64Ty, 0), index);
    B.CreateBr(loopBB);

    B.SetInsertPoint(loopBB);
    Value *iVal = B.CreateLoad(i64Ty, index);
    Value *cond = B.CreateICmpULT(iVal, B.CreateZExt(len, i64Ty));
    B.CreateCondBr(cond, bodyBB, afterBB);

    B.SetInsertPoint(bodyBB);
    Value *slot = B.CreateGEP(childTy, childArray, iVal);
    Value *childDesc = buildLevel(B, C, M, elementType, dims, depth + 1);
    Value *childVal = B.CreateLoad(childTy, childDesc);
    B.CreateStore(childVal, slot);
    Value *next = B.CreateAdd(iVal, ConstantInt::get(i64Ty, 1));
    B.CreateStore(next, index);
    B.CreateBr(loopBB);

    B.SetInsertPoint(afterBB);
  }

  return descriptor;
}

} // namespace ArrayEmitter
