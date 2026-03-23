#include "BuiltinEmitter.h"
#include "../RTDecl.h"
#include "StringEmitter.h"

using namespace llvm;

Value *BuiltinEmitter::handleRandom(IRBuilder<> &B, LLVMContext &ctx,
                                    Module *M) {
  llvm::Function *randF = M->getFunction("rand");
  if (!randF) {
    FunctionType *randTy = FunctionType::get(Type::getInt32Ty(ctx), false);
    randF = llvm::Function::Create(randTy, llvm::Function::ExternalLinkage,
                                   "rand", M);
  }

  Value *randVal = B.CreateCall(randF, {}, "rand.val");
  Value *randFloat = B.CreateSIToFP(randVal, Type::getDoubleTy(ctx), "rand.f");
  Value *randMax = ConstantFP::get(Type::getDoubleTy(ctx), (double)RAND_MAX);
  return B.CreateFDiv(randFloat, randMax, "random.f");
}

void BuiltinEmitter::emitRuntimeInit(IRBuilder<> &B, LLVMContext &ctx,
                                     Module *M) {
  llvm::Function *timeF = M->getFunction("time");
  if (!timeF) {
    FunctionType *timeTy = FunctionType::get(
        Type::getInt64Ty(ctx), {PointerType::getUnqual(ctx)}, false);
    timeF = llvm::Function::Create(timeTy, llvm::Function::ExternalLinkage,
                                   "time", M);
  }
  llvm::Function *srandF = M->getFunction("srand");
  if (!srandF) {
    FunctionType *srandTy =
        FunctionType::get(Type::getVoidTy(ctx), {Type::getInt32Ty(ctx)}, false);
    srandF = llvm::Function::Create(srandTy, llvm::Function::ExternalLinkage,
                                    "srand", M);
  }

  Value *nullPtr = ConstantPointerNull::get(PointerType::getUnqual(ctx));
  Value *timeval = B.CreateCall(timeF, {nullPtr}, "time.val");
  Value *seed = B.CreateTrunc(timeval, Type::getInt32Ty(ctx), "seed");
  B.CreateCall(srandF, {seed});
}

Value *BuiltinEmitter::handleRead(IRBuilder<> &B, LLVMContext &ctx, Module *M) {
  const int BUF_SIZE = 1024;
  Type *i8Ty = Type::getInt8Ty(ctx);
  Type *i32Ty = Type::getInt32Ty(ctx);
  Type *ptrTy = PointerType::getUnqual(ctx);

  Value *buf =
      B.CreateAlloca(i8Ty, ConstantInt::get(i32Ty, BUF_SIZE), "read.buf");

  llvm::Function *fgetsF = M->getFunction("fgets");
  if (!fgetsF) {
    FunctionType *fgetsTy =
        FunctionType::get(ptrTy, {ptrTy, i32Ty, ptrTy}, false);
    fgetsF = llvm::Function::Create(fgetsTy, llvm::Function::ExternalLinkage,
                                    "fgets", M);
  }

  GlobalVariable *stdinVar = M->getGlobalVariable("stdin");
  if (!stdinVar) {
    stdinVar = new GlobalVariable(
        *M, ptrTy, false, GlobalValue::ExternalLinkage, nullptr, "stdin");
  }
  Value *stdinVal = B.CreateLoad(ptrTy, stdinVar, "stdin.val");

  B.CreateCall(fgetsF, {buf, ConstantInt::get(i32Ty, BUF_SIZE), stdinVal});

  Value *len = B.CreateCall(RTDecl::strlen_(M, ctx), {buf}, "read.len");

  Type *i64Ty = Type::getInt64Ty(ctx);
  Value *len64 = B.CreateZExt(len, i64Ty);
  Value *lastIdx = B.CreateSub(len64, ConstantInt::get(i64Ty, 1), "last.idx");
  Value *lastPtr = B.CreateGEP(i8Ty, buf, lastIdx, "last.ptr");
  Value *lastCh = B.CreateLoad(i8Ty, lastPtr, "last.ch");
  Value *isNewline = B.CreateICmpEQ(lastCh, ConstantInt::get(i8Ty, '\n'));
  Value *nullCh = ConstantInt::get(i8Ty, 0);
  B.CreateStore(B.CreateSelect(isNewline, nullCh, lastCh), lastPtr);
  Value *trimmed =
      B.CreateSelect(isNewline, B.CreateSub(len64, ConstantInt::get(i64Ty, 1)),
                     len64, "trimmed.len");

  return StringOps::fromParts(B, ctx, M, buf, trimmed);
}
