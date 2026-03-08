#include "ArithmeticManager.h"

using namespace llvm;

// ----------------- //
// Promotion helpers //
// ----------------- //

Value *ArithmeticManager::promoteToDouble(IRBuilder<> &B, Value *v) {
  if (v->getType()->isDoubleTy())
    return v;
  if (v->getType()->isFloatTy())
    return B.CreateFPExt(v, Type::getDoubleTy(v->getContext()), "f2d");
  return B.CreateSIToFP(v, Type::getDoubleTy(v->getContext()), "i2d");
}

Value *ArithmeticManager::promoteToInt(IRBuilder<> &B, LLVMContext &ctx,
                                       Value *v) {
  if (v->getType()->isIntegerTy())
    return v;
  return B.CreateFPToSI(v, Type::getInt32Ty(ctx), "f2i");
}

// ---------- //
// Comparison //
// ---------- //

Value *ArithmeticManager::generateComparison(IRBuilder<> &B, BinaryOp op,
                                             Value *l, Value *r, bool isFloat) {
  if (isFloat) {
    l = promoteToDouble(B, l);
    r = promoteToDouble(B, r);
    switch (op) {
    case BinaryOp::Lt:
      return B.CreateFCmpOLT(l, r, "flt");
    case BinaryOp::Le:
      return B.CreateFCmpOLE(l, r, "fle");
    case BinaryOp::Gt:
      return B.CreateFCmpOGT(l, r, "fgt");
    case BinaryOp::Ge:
      return B.CreateFCmpOGE(l, r, "fge");
    case BinaryOp::Eq:
      return B.CreateFCmpOEQ(l, r, "feq");
    case BinaryOp::Ne:
      return B.CreateFCmpONE(l, r, "fne");
    default:
      return nullptr;
    }
  } else {
    switch (op) {
    case BinaryOp::Lt:
      return B.CreateICmpSLT(l, r, "ilt");
    case BinaryOp::Le:
      return B.CreateICmpSLE(l, r, "ile");
    case BinaryOp::Gt:
      return B.CreateICmpSGT(l, r, "igt");
    case BinaryOp::Ge:
      return B.CreateICmpSGE(l, r, "ige");
    case BinaryOp::Eq:
      return B.CreateICmpEQ(l, r, "ieq");
    case BinaryOp::Ne:
      return B.CreateICmpNE(l, r, "ine");
    default:
      return nullptr;
    }
  }
}

// ------------------------------------------------- //
// Full binary expression dispatch (non-string path) //
// ------------------------------------------------- //

Value *ArithmeticManager::emitBinaryOp(IRBuilder<> &B, LLVMContext &ctx,
                                       BinaryOp op, Value *l, Value *r) {
  if (l->getType()->isIntegerTy() && r->getType()->isIntegerTy()) {
    unsigned lb = l->getType()->getIntegerBitWidth();
    unsigned rb = r->getType()->getIntegerBitWidth();
    if (lb < rb)
      l = B.CreateSExt(l, r->getType());
    else if (rb < lb)
      r = B.CreateSExt(r, l->getType());
  }

  bool lf = l->getType()->isFloatingPointTy();
  bool rf = r->getType()->isFloatingPointTy();
  if (lf && r->getType()->isIntegerTy())
    r = B.CreateSIToFP(r, Type::getDoubleTy(ctx));
  else if (rf && l->getType()->isIntegerTy())
    l = B.CreateSIToFP(l, Type::getDoubleTy(ctx));

  switch (op) {
  case BinaryOp::Lt:
  case BinaryOp::Le:
  case BinaryOp::Gt:
  case BinaryOp::Ge:
  case BinaryOp::Eq:
  case BinaryOp::Ne:
    return generateComparison(B, op, l, r, lf || rf);

  case BinaryOp::Add:
    return (lf || rf) ? B.CreateFAdd(promoteToDouble(B, l),
                                     promoteToDouble(B, r), "fadd")
                      : B.CreateAdd(l, r, "add");

  case BinaryOp::Sub:
    return (lf || rf) ? B.CreateFSub(promoteToDouble(B, l),
                                     promoteToDouble(B, r), "fsub")
                      : B.CreateSub(l, r, "sub");

  case BinaryOp::Mul:
    return (lf || rf) ? B.CreateFMul(promoteToDouble(B, l),
                                     promoteToDouble(B, r), "fmul")
                      : B.CreateMul(l, r, "mul");

  case BinaryOp::Div:
    return B.CreateFDiv(promoteToDouble(B, l), promoteToDouble(B, r), "fdiv");

  case BinaryOp::DivFloor:
    return B.CreateSDiv(promoteToInt(B, ctx, l), promoteToInt(B, ctx, r),
                        "sdiv");

  case BinaryOp::Mod:
    return B.CreateSRem(promoteToInt(B, ctx, l), promoteToInt(B, ctx, r),
                        "srem");

  default:
    return nullptr;
  }
}
