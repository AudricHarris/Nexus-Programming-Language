#pragma once
#include "llvm/IR/Attributes.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"

namespace RTDecl {
llvm::Function *malloc_(llvm::Module *M, llvm::LLVMContext &ctx);
llvm::Function *memcpy_(llvm::Module *M, llvm::LLVMContext &ctx);
llvm::Function *sprintf_(llvm::Module *M, llvm::LLVMContext &ctx);
llvm::Function *strlen_(llvm::Module *M, llvm::LLVMContext &ctx);
llvm::Function *strcmp_(llvm::Module *M, llvm::LLVMContext &ctx);
llvm::Function *free_(llvm::Module *M, llvm::LLVMContext &ctx);
} // namespace RTDecl
