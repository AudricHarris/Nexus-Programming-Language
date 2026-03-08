#include "StringEmitter.h"
#include "../RTDecl.h"
#include "../TypeResolver.h"
#include "llvm/IR/Verifier.h"

// Public
llvm::Value *StringOps::fromLiteral(llvm::IRBuilder<> &B,
                                    llvm::LLVMContext &ctx, llvm::Module *M,
                                    const std::string &literal) {
  llvm::Value *ptr = B.CreateGlobalString(literal, "strl");
  llvm::Value *len =
      llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), literal.size());
  return fromParts(B, ctx, M, ptr, len);
}

llvm::Value *StringOps::fromParts(llvm::IRBuilder<> &B, llvm::LLVMContext &ctx,
                                  llvm::Module *M, llvm::Value *dataPtr,
                                  llvm::Value *length) {
  llvm::Type *i64 = llvm::Type::getInt64Ty(ctx);
  llvm::Value *cap = B.CreateAdd(length, llvm::ConstantInt::get(i64, 1), "cap");
  llvm::Value *mem = B.CreateCall(RTDecl::malloc_(M, ctx), {cap}, "str.alloc");

  B.CreateCall(RTDecl::memcpy_(M, ctx),
               {mem, dataPtr, length, llvm::ConstantInt::getFalse(ctx)});
  llvm::Value *nullPos = B.CreateGEP(llvm::Type::getInt8Ty(ctx), mem, length);
  B.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt8Ty(ctx), 0), nullPos);

  llvm::StructType *st = TypeResolver::getStringType(ctx);
  llvm::AllocaInst *s = B.CreateAlloca(st, nullptr, "string");
  B.CreateStore(mem, B.CreateStructGEP(st, s, 0));
  B.CreateStore(length, B.CreateStructGEP(st, s, 1));
  B.CreateStore(cap, B.CreateStructGEP(st, s, 2));
  return s;
}

llvm::Value *StringOps::concat(llvm::IRBuilder<> &B, llvm::LLVMContext &ctx,
                               llvm::Module *M, llvm::Value *leftStruct,
                               llvm::Value *rightStruct) {
  llvm::StructType *st = TypeResolver::getStringType(ctx);
  llvm::Value *lv = B.CreateLoad(st, leftStruct, "ls");
  llvm::Value *rv = B.CreateLoad(st, rightStruct, "rs");
  llvm::Value *ld = B.CreateExtractValue(lv, {0}, "ld");
  llvm::Value *ll = B.CreateExtractValue(lv, {1}, "ll");
  llvm::Value *rd = B.CreateExtractValue(rv, {0}, "rd");
  llvm::Value *rl = B.CreateExtractValue(rv, {1}, "rl");

  llvm::Type *i64 = llvm::Type::getInt64Ty(ctx);
  llvm::Value *total = B.CreateAdd(ll, rl, "tlen");
  llvm::Value *cap = B.CreateAdd(total, llvm::ConstantInt::get(i64, 1), "tcap");
  llvm::Value *mem = B.CreateCall(RTDecl::malloc_(M, ctx), {cap}, "cat.alloc");

  llvm::Function *mc = RTDecl::memcpy_(M, ctx);
  B.CreateCall(mc, {mem, ld, ll, llvm::ConstantInt::getFalse(ctx)});
  llvm::Value *mid = B.CreateGEP(llvm::Type::getInt8Ty(ctx), mem, ll);
  B.CreateCall(mc, {mid, rd, rl, llvm::ConstantInt::getFalse(ctx)});
  llvm::Value *null = B.CreateGEP(llvm::Type::getInt8Ty(ctx), mem, total);
  B.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt8Ty(ctx), 0), null);

  llvm::AllocaInst *res = B.CreateAlloca(st, nullptr, "cat.res");
  B.CreateStore(mem, B.CreateStructGEP(st, res, 0));
  B.CreateStore(total, B.CreateStructGEP(st, res, 1));
  B.CreateStore(cap, B.CreateStructGEP(st, res, 2));
  return res;
}

llvm::Value *StringOps::fromValue(llvm::IRBuilder<> &B, llvm::LLVMContext &ctx,
                                  llvm::Module *M, llvm::Value *val) {
  llvm::Type *ty = val->getType();
  if (TypeResolver::isString(ty))
    return val;
  if (ty->isIntegerTy())
    return intToStr(B, ctx, M, val);
  if (ty->isFloatingPointTy())
    return floatToStr(B, ctx, M, val);
  if (ty->isPointerTy()) {
    llvm::Value *len = B.CreateCall(RTDecl::strlen_(M, ctx), {val}, "sl");
    return fromParts(B, ctx, M, val, len);
  }
  return fromLiteral(B, ctx, M, "[unprintable]");
}

// Private

llvm::Value *StringOps::intToStr(llvm::IRBuilder<> &B, llvm::LLVMContext &ctx,
                                 llvm::Module *M, llvm::Value *v) {
  if (v->getType()->getIntegerBitWidth() < 64)
    v = B.CreateSExt(v, llvm::Type::getInt64Ty(ctx));
  llvm::Value *fmt = B.CreateGlobalString("%lld", "ifmt");
  auto *buf = B.CreateAlloca(
      llvm::ArrayType::get(llvm::Type::getInt8Ty(ctx), 32), nullptr, "ibuf");
  B.CreateCall(RTDecl::sprintf_(M, ctx), {buf, fmt, v});
  llvm::Value *len = B.CreateCall(RTDecl::strlen_(M, ctx), {buf}, "il");
  return fromParts(B, ctx, M, buf, len);
}

llvm::Value *StringOps::floatToStr(llvm::IRBuilder<> &B, llvm::LLVMContext &ctx,
                                   llvm::Module *M, llvm::Value *v) {
  if (v->getType()->isFloatTy())
    v = B.CreateFPExt(v, llvm::Type::getDoubleTy(ctx));
  llvm::Value *fmt = B.CreateGlobalString("%g", "ffmt");
  auto *buf = B.CreateAlloca(
      llvm::ArrayType::get(llvm::Type::getInt8Ty(ctx), 64), nullptr, "fbuf");
  B.CreateCall(RTDecl::sprintf_(M, ctx), {buf, fmt, v});
  llvm::Value *len = B.CreateCall(RTDecl::strlen_(M, ctx), {buf}, "fl");
  return fromParts(B, ctx, M, buf, len);
}
