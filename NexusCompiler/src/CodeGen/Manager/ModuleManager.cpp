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
  if (!symbols.empty()) {
    std::unordered_set<std::string> wanted(symbols.begin(), symbols.end());
    src.globals.erase(
        std::remove_if(src.globals.begin(), src.globals.end(),
                       [&](const auto &g) { return !wanted.count(g->name); }),
        src.globals.end());
    src.functions.erase(
        std::remove_if(src.functions.begin(), src.functions.end(),
                       [&](const auto &fn) {
                         return !wanted.count(fn->name.token.getWord());
                       }),
        src.functions.end());
    return;
  }

  src.globals.erase(std::remove_if(src.globals.begin(), src.globals.end(),
                                   [](const auto &g) { return !g->isPublic; }),
                    src.globals.end());
  src.functions.erase(
      std::remove_if(src.functions.begin(), src.functions.end(),
                     [](const auto &fn) { return !fn->isPublic; }),
      src.functions.end());
  src.structs.erase(std::remove_if(src.structs.begin(), src.structs.end(),
                                   [](const auto &s) { return !s->isPublic; }),
                    src.structs.end());
}

ResolvedModule &ModuleManager::resolveImport(const ImportDecl &decl) {
  fs::path filePath = importPathToFile(decl.path, decl.path.isStdLib);
  std::string canonical = fs::weakly_canonical(filePath).string();
  std::cerr << "DEBUG resolving: " << filePath << "\n";
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

    for (auto it = mod.ast->structs.rbegin(); it != mod.ast->structs.rend();
         ++it)
      prog.structs.insert(prog.structs.begin(), std::move(*it));

    for (auto it = mod.ast->externBlocks.rbegin();
         it != mod.ast->externBlocks.rend(); ++it)
      prog.externBlocks.insert(prog.externBlocks.begin(), std::move(*it));

    for (auto it = mod.ast->globals.rbegin(); it != mod.ast->globals.rend();
         ++it)
      prog.globals.insert(prog.globals.begin(), std::move(*it));

    for (auto it = mod.ast->functions.rbegin(); it != mod.ast->functions.rend();
         ++it)
      prog.functions.insert(prog.functions.begin(), std::move(*it));

    mod.ast.reset();
  }
  prog.imports.clear();
}
