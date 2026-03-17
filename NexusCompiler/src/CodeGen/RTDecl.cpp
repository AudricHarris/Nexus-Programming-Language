#include "RTDecl.h"

llvm::Function *RTDecl::malloc_(llvm::Module *M, llvm::LLVMContext &ctx) {
  llvm::Function *f = M->getFunction("malloc");
  if (!f) {
    auto *ft = llvm::FunctionType::get(llvm::PointerType::get(ctx, 0),
                                       {llvm::Type::getInt64Ty(ctx)}, false);
    f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "malloc",
                               M);
  }
  return f;
}

llvm::Function *RTDecl::memcpy_(llvm::Module *M, llvm::LLVMContext &ctx) {
  const std::string name = "llvm.memcpy.p0.p0.i64";
  llvm::Function *f = M->getFunction(name);
  if (!f) {
    llvm::Type *ptrTy = llvm::PointerType::get(ctx, 0);
    auto *ft = llvm::FunctionType::get(
        llvm::Type::getVoidTy(ctx),
        {ptrTy, ptrTy, llvm::Type::getInt64Ty(ctx), llvm::Type::getInt1Ty(ctx)},
        false);
    f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, name, M);
    f->addFnAttr(llvm::Attribute::NoUnwind);
  }
  return f;
}

llvm::Function *RTDecl::sprintf_(llvm::Module *M, llvm::LLVMContext &ctx) {
  llvm::Function *f = M->getFunction("sprintf");
  if (!f) {
    llvm::Type *ptrTy = llvm::PointerType::get(ctx, 0);
    auto *ft = llvm::FunctionType::get(llvm::Type::getInt32Ty(ctx),
                                       {ptrTy, ptrTy}, true);
    f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "sprintf",
                               M);
  }
  return f;
}
llvm::Function *RTDecl::strcmp_(llvm::Module *M, llvm::LLVMContext &ctx) {
  llvm::Function *f = M->getFunction("strcmp");
  if (!f) {
    llvm::Type *i8ptr =
        llvm::PointerType::get(llvm::Type::getInt8Ty(ctx)->getContext(), 0);

    llvm::FunctionType *ft = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(ctx), {i8ptr, i8ptr}, false);

    f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "strcmp",
                               *M);
  }
  return f;
}
llvm::Function *RTDecl::strlen_(llvm::Module *M, llvm::LLVMContext &ctx) {
  llvm::Function *f = M->getFunction("strlen");
  if (!f) {
    auto *ft = llvm::FunctionType::get(llvm::Type::getInt64Ty(ctx),
                                       {llvm::PointerType::get(ctx, 0)}, false);
    f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "strlen",
                               M);
  }
  return f;
}
