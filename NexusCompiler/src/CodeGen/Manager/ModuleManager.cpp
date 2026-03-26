#include "ModuleManager.h"
#include "../../Lexer/Lexer.h"
#include "../../Parser/Parser.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

ModuleManager::ModuleManager(fs::path root, fs::path stdlib)
    : projectRoot(std::move(root)), stdlibRoot(std::move(stdlib)) {}

fs::path ModuleManager::importPathToFile(const ImportPath &path,
                                         bool isStdLib) const {
  fs::path root = isStdLib ? stdlibRoot : projectRoot;

  // In my language Nexus will be the standard library
  size_t start = isStdLib ? 1 : 0;
  fs::path result = root;
  for (size_t i = start; i < path.segments.size(); ++i)
    result /= path.segments[i];
  result += ".nx";
  return result;
}

std::unique_ptr<Program> ModuleManager::parseFile(const fs::path &file) {
  std::ifstream f(file);
  if (!f)
    throw std::runtime_error("Cannot open module: " + file.string());

  std::ostringstream ss;
  ss << f.rdbuf();
  std::string src = ss.str();

  Lexer lexer(src);
  auto tokens = lexer.Tokenize();
  Parser parser(std::move(tokens));
  return parser.parse();
}

void ModuleManager::applyFilter(Program &src,
                                const std::vector<std::string> &symbols) {
  if (symbols.empty())
    return;

  std::unordered_set<std::string> wanted(symbols.begin(), symbols.end());

  auto &gs = src.globals;
  gs.erase(
      std::remove_if(gs.begin(), gs.end(),
                     [&](const auto &g) { return !wanted.count(g->name); }),
      gs.end());

  auto &fs = src.functions;
  fs.erase(std::remove_if(fs.begin(), fs.end(),
                          [&](const auto &fn) {
                            return !wanted.count(fn->name.token.getWord());
                          }),
           fs.end());
}

ResolvedModule &ModuleManager::resolveImport(const ImportDecl &decl) {
  fs::path filePath = importPathToFile(decl.path, decl.path.isStdLib);
  std::string canonical = fs::weakly_canonical(filePath).string();

  if (resolved.count(canonical))
    return resolved[canonical];

  if (inProgress.count(canonical))
    throw std::runtime_error("Circular import detected: " + canonical);

  inProgress.insert(canonical);

  auto &mod = resolved[canonical];
  mod.filePath = filePath;
  mod.importedSymbols = decl.symbols;
  mod.ast = parseFile(filePath);

  resolveAll(*mod.ast);

  applyFilter(*mod.ast, decl.symbols);

  inProgress.erase(canonical);
  return mod;
}

void ModuleManager::resolveAll(Program &prog) {
  for (const auto &imp : prog.imports) {
    auto &mod = resolveImport(*imp);

    for (auto it = mod.ast->globals.rbegin(); it != mod.ast->globals.rend();
         ++it) {
      prog.globals.insert(prog.globals.begin(), std::move(*it));
    }

    for (auto it = mod.ast->functions.rbegin(); it != mod.ast->functions.rend();
         ++it) {
      prog.functions.insert(prog.functions.begin(), std::move(*it));
    }

    mod.ast.reset();
  }
  prog.imports.clear();
}
