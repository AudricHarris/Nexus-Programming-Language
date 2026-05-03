#pragma once
#include "../../AST/AST.h"
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

struct ResolvedModule {
  fs::path filePath;
  std::unique_ptr<Program> ast;
  std::vector<std::string> importedSymbols;
};

class ModuleManager {
public:
  explicit ModuleManager(fs::path projectRoot, fs::path stdlibRoot);

  void resolveAll(Program &prog);

private:
  fs::path projectRoot;
  fs::path stdlibRoot;

  std::unordered_set<std::string> inProgress;
  std::unordered_map<std::string, ResolvedModule> resolved;

  ResolvedModule &resolveImport(const ImportDecl &decl);

  fs::path importPathToFile(const ImportPath &path, bool isStdLib) const;

  void applyFilter(Program &src, const std::vector<std::string> &symbols);

  std::unique_ptr<Program> parseFile(const fs::path &file);
};
