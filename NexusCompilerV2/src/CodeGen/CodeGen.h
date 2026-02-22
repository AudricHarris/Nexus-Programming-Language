#ifndef CodeGen_H
#define CodeGen_H
#include "../AST/AST.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include <map>
#include <string>

struct VarInfo {
  llvm::AllocaInst *alloca;
  llvm::Type *type; // pointee type
};

class CodeGenerator {
public:
  CodeGenerator();

  bool generate(const Program &program, const std::string &outputFilename);

private:
  llvm::LLVMContext context;
  std::unique_ptr<llvm::Module> module;
  llvm::IRBuilder<> builder;
  std::map<std::string, VarInfo> namedValues;
  std::map<std::string, llvm::Function *> functionProtos;
  llvm::Type *getLLVMType(const Identifier &typeId);
  llvm::Value *codegen(const Expression &expr);
  llvm::Value *codegen(const Statement &stmt);
  void codegen(const Block &block);
  llvm::Function *codegen(const Function &func);
  llvm::Value *logErrorV(const char *msg);
  std::string hexToAnsi(const std::string &hex);
  std::string replaceHexColors(const std::string &input);
};

#endif
