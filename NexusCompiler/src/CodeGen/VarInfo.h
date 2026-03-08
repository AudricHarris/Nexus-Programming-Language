
#ifndef VARINFO_H
#define VARINFO_H

#include "llvm/IR/Instructions.h"
#include "llvm/IR/Type.h"
#include <string>

// ---------------------------------------------------- //
// Per-variable metadata tracked during code generation //
// ---------------------------------------------------- //
struct VarInfo {
  llvm::AllocaInst *allocaInst = nullptr;
  llvm::Type *type = nullptr;
  bool isBorrowed = false;
  bool isMoved = false;
  bool isReference = false;
  bool isConst = false;
  llvm::Type *pointeeType = nullptr;
  std::string sourceName;
  bool ownsHeap = false;

  VarInfo() = default;

  VarInfo(llvm::AllocaInst *a, llvm::Type *t, bool borrowed, bool moved,
          bool ref = false, bool c = false, std::string src = "")
      : allocaInst(a), type(t), isBorrowed(borrowed), isMoved(moved),
        isReference(ref), isConst(c), sourceName(std::move(src)) {}
};

#endif // VARINFO_H
