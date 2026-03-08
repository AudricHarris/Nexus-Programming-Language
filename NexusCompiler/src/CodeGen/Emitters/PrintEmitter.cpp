#include "PrintEmitter.h"
#include "../TypeResolver.h"
#include "llvm/IR/Constants.h"
#include <regex>

using namespace llvm;

// ------------------------------------- //
// Internal types & helpers (file-local) //
// ------------------------------------- //

struct FmtArg {
  std::string spec;
  Value *value;
};

static std::string unescapeString(const std::string &s) {
  std::string out;
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

static Value *evalInterp(const std::string &inner, LLVMContext &ctx,
                         IRBuilder<> &B,
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
  try {
    size_t pos;
    long long v = std::stoll(inner, &pos);
    if (pos == inner.size())
      return ConstantInt::get(Type::getInt32Ty(ctx), v);
  } catch (...) {
  }
  try {
    size_t pos;
    double v = std::stod(inner, &pos);
    if (pos == inner.size())
      return ConstantFP::get(Type::getDoubleTy(ctx), v);
  } catch (...) {
  }
  return nullptr;
}

static std::string buildPrintfFmt(const std::string &raw, LLVMContext &ctx,
                                  IRBuilder<> &B,
                                  const std::map<std::string, VarInfo> &vars,
                                  std::vector<FmtArg> &args) {
  std::string fmt;
  size_t i = 0;
  while (i < raw.size()) {
    if (raw[i] == '\\' && i + 1 < raw.size() && raw[i + 1] == '{') {
      fmt += '{';
      i += 2;
      continue;
    }
    if (raw[i] != '{') {
      if (raw[i] == '%')
        fmt += '%';
      fmt += raw[i++];
      continue;
    }
    if (i + 1 < raw.size() && raw[i + 1] == '{') {
      fmt += '{';
      i += 2;
      continue;
    }

    // Scan to matching '}'
    size_t start = i + 1, depth = 1, j = start;
    while (j < raw.size() && depth > 0) {
      if (raw[j] == '{')
        ++depth;
      else if (raw[j] == '}')
        --depth;
      if (depth > 0)
        ++j;
      else
        break;
    }
    if (j >= raw.size()) {
      fmt += raw[i++];
      continue;
    }

    std::string inner = raw.substr(start, j - start);
    Value *val = evalInterp(inner, ctx, B, vars);
    if (val) {
      Type *ty = val->getType();
      if (ty->isIntegerTy(1)) {
        Value *t = B.CreateGlobalString("true", "bt");
        Value *f = B.CreateGlobalString("false", "bf");
        args.push_back({"%s", B.CreateSelect(val, t, f, "bstr")});
        fmt += "%s";
      } else if (ty->isIntegerTy()) {
        if (ty->getIntegerBitWidth() < 64)
          val = B.CreateSExt(val, Type::getInt64Ty(ctx));
        args.push_back({"%lld", val});
        fmt += "%lld";
      } else if (ty->isFloatingPointTy()) {
        if (!ty->isDoubleTy())
          val = B.CreateFPExt(val, Type::getDoubleTy(ctx));
        args.push_back({"%g", val});
        fmt += "%g";
      } else if (ty->isPointerTy()) {
        args.push_back({"%s", val});
        fmt += "%s";
      } else {
        // Struct — assume string-like, extract data ptr
        if (auto *ai = dyn_cast<AllocaInst>(val)) {
          if (TypeResolver::isString(ai->getAllocatedType())) {
            StructType *st = cast<StructType>(ai->getAllocatedType());
            Value *load = B.CreateLoad(st, ai);
            Value *ptr = B.CreateExtractValue(load, {0});
            args.push_back({"%s", ptr});
            fmt += "%s";
          }
        }
      }
    } else {
      fmt += '{' + inner + '}';
    }
    i = j + 1;
  }
  return fmt;
}

// -------------- //
// Colour helpers //
// -------------- //

std::string PrintEmitter::hexToAnsi(const std::string &hex) {
  if (hex.size() != 7 || hex[0] != '#')
    return hex;
  int r = std::stoi(hex.substr(1, 2), nullptr, 16);
  int g = std::stoi(hex.substr(3, 2), nullptr, 16);
  int b = std::stoi(hex.substr(5, 2), nullptr, 16);
  return "\033[38;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" +
         std::to_string(b) + "m";
}

std::string PrintEmitter::replaceHexColors(const std::string &input) {
  std::regex re(R"(#([0-9a-fA-F]{6}))");
  std::string result;
  std::sregex_iterator it(input.begin(), input.end(), re), end;
  size_t last = 0;
  for (; it != end; ++it) {
    auto &m = *it;
    result += input.substr(last, m.position() - last);
    result += hexToAnsi(m.str());
    last = m.position() + m.length();
  }
  return result + input.substr(last);
}

// ---------- //
// Public API //
// ---------- //

Value *PrintEmitter::handlePrintf(const CallExpr &e, IRBuilder<> &B,
                                  LLVMContext &ctx, Module *M,
                                  const std::map<std::string, VarInfo> &vars) {
  auto *strArg = dynamic_cast<const StrLitExpr *>(e.arguments[0].get());
  if (!strArg)
    return nullptr; // caller logs error

  std::string raw = unescapeString(strArg->lit.getWord());
  raw = replaceHexColors(raw) + "\033[0m";

  std::vector<FmtArg> fmtArgs;
  std::string fmt = buildPrintfFmt(raw, ctx, B, vars, fmtArgs);

  llvm::Function *printfF = M->getFunction("printf");
  if (!printfF)
    return nullptr;

  Value *fmtPtr = B.CreateGlobalString(fmt, ".fmt");
  std::vector<Value *> args = {fmtPtr};

  for (auto &fa : fmtArgs) {
    if (fa.spec == "%s") {
      if (auto *ai = dyn_cast<AllocaInst>(fa.value)) {
        Type *at = ai->getAllocatedType();
        if (TypeResolver::isString(at)) {
          Value *loaded = B.CreateLoad(cast<StructType>(at), ai);
          args.push_back(B.CreateExtractValue(loaded, {0}));
          continue;
        }
      }
    }
    args.push_back(fa.value);
  }
  return B.CreateCall(printfF, args, "printf.ret");
}

Value *PrintEmitter::handlePrint(const CallExpr &e, IRBuilder<> &B,
                                 LLVMContext &ctx, Module *M) {
  (void)ctx;
  auto *strArg = dynamic_cast<const StrLitExpr *>(e.arguments[0].get());
  if (!strArg)
    return nullptr;

  llvm::Function *printfF = M->getFunction("printf");
  if (!printfF)
    return nullptr;

  std::string fmt = strArg->lit.getWord() + "\n";
  Value *fmtPtr = B.CreateGlobalString(fmt, ".fmt");
  return B.CreateCall(printfF, {fmtPtr}, "printf.ret");
}
