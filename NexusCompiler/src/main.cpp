#include "CodeGen/CodeGen.h"
#include "CodeGen/Manager/ModuleManager.h"
#include "FileReader/FileReader.h"
#include "Lexer/Lexer.h"
#include "Parser/Parser.h"
#include "Token/TokenType.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <vector>
namespace fs = std::filesystem;

#define NEXUS_STDLIB_PATH "/home/wearwqlf/Documents/NexusStandardLibrary/"

bool endsWith(const std::string &str, const std::string &suffix) {
  if (str.length() < suffix.length())
    return false;
  return str.compare(str.length() - suffix.length(), suffix.length(), suffix) ==
         0;
}

bool hasValidExt(const std::string &f) {
  return endsWith(f, ".nx") || endsWith(f, ".nex") || endsWith(f, ".nexus");
}

std::string getOutputName(const std::string &file) {
  size_t pos = file.find_last_of('.');
  if (pos == std::string::npos)
    return file + ".x";
  return file.substr(0, pos) + ".x";
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: nexus [files...]\n";
    return EXIT_FAILURE;
  }

  std::vector<std::string> inputs;

  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];

    if (arg == "--version") {
      std::cout << "nexus 1.4.2\n";
      return 0;
    }

    inputs.push_back(arg);
  }

  std::cout << "Compiling " << argc - 1 << " nexus files" << "\n";
  for (const auto &file : inputs) {
    if (!hasValidExt(file)) {
      std::cerr << "Skipping invalid file: " << file << "\n";
      continue;
    }

    std::optional<std::string> codeOpt = readFile(file.c_str());
    if (!codeOpt.has_value()) {
      std::cerr << "Failed to read file: " << file << "\n";
      continue;
    }

    std::string code = codeOpt.value();

    std::cout << "\nCompiling: " << file << "\n";

    // Lexer
    auto start = std::chrono::high_resolution_clock::now();
    Lexer lexer(code);
    std::vector<Token> tokens = lexer.Tokenize();
    auto end = std::chrono::high_resolution_clock::now();

    double elapsedMs =
        std::chrono::duration<double, std::milli>(end - start).count();
    double elapsedS = elapsedMs / 1000.0;
    double tokPerS = tokens.size() / (elapsedS > 0 ? elapsedS : 1);

    std::cout << "Tokens:          " << tokens.size() << "\n";
    std::cout << "Time (s):        " << elapsedS << "\n";
    std::cout << "Tokens / second: " << tokPerS << "\n";

    // Parser
    Parser parser(tokens);
    auto parsed = parser.parse();

    if (!parsed) {
      std::cerr << "Parsing failed for " << file << "\n";
      continue;
    }

    fs::path projectRoot = fs::path(file).parent_path();
    fs::path stdlibRoot = fs::path(NEXUS_STDLIB_PATH);

    ModuleManager mm(projectRoot, stdlibRoot);
    mm.resolveAll(*parsed);

    CodeGenerator cg;
    if (!cg.generate(*parsed, "out")) {
      std::cerr << "Codegen failed for " << file << "\n";
      continue;
    }

    std::string output = getOutputName(file);

    // Out.ll to out.x
    std::string cmd = "clang -Wno-override-module -fsanitize=address "
                      "-fsanitize=leak -g out.ll -o " +
                      output;

    int res = system(cmd.c_str());
    // system("rm -rf out.ll");
    if (res != 0) {
      std::cerr << "Clang failed for " << file << "\n";
      continue;
    }

    std::cout << "Run: ./" << output << "\n\n";
  }

  std::cout << "\nDone.\n";
  return EXIT_SUCCESS;
}
