#pragma once
#include "TypeResolver.h"
#include "VarInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include <map>
#include <string>

namespace codegen_utils {

// --------------------------- //
// unescapeString              //
// Process backslash sequences //
// --------------------------- //
inline std::string unescapeString(const std::string &s) {
  std::string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '\\' && i + 1 < s.size()) {
      switch (s[i + 1]) {
      case 'n':
        out += '\n';
        ++i;
        break;
      case 't':
        out += '\t';
        ++i;
        break;
      case 'r':
        out += '\r';
        ++i;
        break;
      case '\\':
        out += '\\';
        ++i;
        break;
      case '"':
        out += '"';
        ++i;
        break;
      case '0':
        out += '\0';
        ++i;
        break;
      case '{':
        out += '{';
        ++i;
        break;
      default:
        out += s[i];
        break;
      }
    } else {
      out += s[i];
    }
  }
  return out;
}

// ---------------------------------------------------- //
// evalInterp                                           //
// Evaluate a string-interpolation expression fragment. //
// ---------------------------------------------------- //
inline llvm::Value *evalInterp(const std::string &inner, llvm::LLVMContext &ctx,
                               llvm::IRBuilder<> &B,
                               const std::map<std::string, VarInfo> &vars) {

  bool isIdent = !inner.empty();
  for (char c : inner)
    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
      isIdent = false;
      break;
    }

  if (isIdent) {
    auto it = vars.find(inner);
    if (it != vars.end() && !it->second.isMoved) {
      if (TypeResolver::isString(it->second.type) ||
          TypeResolver::isArray(it->second.type))
        return it->second.allocaInst;
      return B.CreateLoad(it->second.type, it->second.allocaInst,
                          inner + ".load");
    }
    return nullptr;
  }

  // Integer literal
  try {
    size_t pos;
    long long v = std::stoll(inner, &pos);
    if (pos == inner.size())
      return llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), v);
  } catch (...) {
  }

  // Float literal
  try {
    size_t pos;
    double v = std::stod(inner, &pos);
    if (pos == inner.size())
      return llvm::ConstantFP::get(llvm::Type::getDoubleTy(ctx), v);
  } catch (...) {
  }

  return nullptr;
}

} // namespace codegen_utils
