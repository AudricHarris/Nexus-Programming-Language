#include "CodeGen/CodeGen.h"
#include "CodeGen/Manager/ModuleManager.h"
#include "FileReader/FileReader.h"
#include "Lexer/Lexer.h"
#include "Parser/Parser.h"
#include "Token/TokenType.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// -------------------- //
// Windows & unix check //
// -------------------- //

std::string getHomeDirectory() {
  const char *home = nullptr;

#ifdef _WIN32
  home = std::getenv("USERPROFILE");
  if (!home) {
    const char *homedrive = std::getenv("HOMEDRIVE");
    const char *homepath = std::getenv("HOMEPATH");
    if (homedrive && homepath) {
      return std::string(homedrive) + homepath;
    }
  }
#else
  home = std::getenv("HOME");
#endif

  return home ? home : "";
}

fs::path getConfigDir() {
  std::string home = getHomeDirectory();
  if (home.empty()) {
    return fs::current_path() / "nexus_config"; // fallback
  }

#ifdef _WIN32
  // Windows: %APPDATA%\nexus
  const char *appdata = std::getenv("APPDATA");
  if (appdata) {
    return fs::path(appdata) / "nexus";
  }
  return fs::path(home) / "AppData" / "Roaming" / "nexus";
#else
  // Linux/macOS: ~/.config/nexus
  return fs::path(home) / ".config" / "nexus";
#endif
}

fs::path getConfigFilePath() { return getConfigDir() / "config"; }

std::optional<std::string> loadStdlibPath() {
  fs::path configFile = getConfigFilePath();
  if (!fs::exists(configFile)) {
    return std::nullopt;
  }

  std::ifstream file(configFile);
  if (!file.is_open()) {
    return std::nullopt;
  }

  std::string line;
  std::getline(file, line);

  line.erase(0, line.find_first_not_of(" \t\r\n"));
  line.erase(line.find_last_not_of(" \t\r\n") + 1);

  if (line.empty())
    return std::nullopt;

  fs::path stdlibPath(line);
  if (fs::exists(stdlibPath) && fs::is_directory(stdlibPath)) {
    return stdlibPath.string();
  }

  return std::nullopt;
}

bool saveStdlibPath(const std::string &path) {
  fs::path configDir = getConfigDir();
  std::error_code ec;
  fs::create_directories(configDir, ec);
  if (ec) {
    std::cerr << "Warning: Could not create config directory: " << ec.message()
              << "\n";
    return false;
  }

  std::ofstream file(getConfigFilePath());
  if (!file.is_open()) {
    return false;
  }

  file << path << std::endl;
  return true;
}

std::string setupStdlibPath() {
  std::cout << "\nNexus Standard Library Setup\n";
  std::cout << "The standard library path has not been configured yet.\n\n";

  std::string input;
  while (true) {
    std::cout << "Please enter the full path to the Nexus Standard Library "
                 "directory:\n> ";
    std::getline(std::cin, input);

    if (input.empty()) {
      std::cerr << "Path cannot be empty.\n";
      continue;
    }

    fs::path p(input);
    if (fs::exists(p) && fs::is_directory(p)) {
      if (saveStdlibPath(p.string())) {
        std::cout << "Standard library path saved successfully!\n";
        return p.string();
      } else {
        std::cerr << "Failed to save configuration to disk.\n";
      }
    } else {
      std::cerr << "Error: The path does not exist or is not a directory.\n";
    }

    std::cout << "Try again? (y/n): ";
    std::string again;
    std::getline(std::cin, again);
    if (!again.empty() && (again[0] == 'n' || again[0] == 'N')) {
      std::cerr
          << "Setup cancelled. Cannot compile without the standard library.\n";
      std::exit(EXIT_FAILURE);
    }
  }
}

// -------------------- //
// Utility functions    //
// -------------------- //

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
  std::string base = (pos == std::string::npos) ? file : file.substr(0, pos);

#ifdef _WIN32
  return base + ".exe";
#else
  return base + ".x";
#endif
}

// -------------------- //
// Main                 //
// -------------------- //

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: nexus [options] [files...]\n";
    std::cerr << "Options:\n";
    std::cerr
        << "  init          Initialize or reconfigure standard library path\n";
    std::cerr << "  --version     Show version information\n";
    return EXIT_FAILURE;
  }

  std::string firstArg = argv[1];

  if (firstArg == "--version") {
    std::cout << "nexus 1.4.2\n";
    return 0;
  }

  if (firstArg == "init") {
    std::cout << "Nexus Initialization\n";
    setupStdlibPath();
    std::cout << "\nSetup complete! You can now compile your .nx files.\n";
    return 0;
  }

  std::optional<std::string> stdlibOpt = loadStdlibPath();

  if (!stdlibOpt.has_value()) {
    std::cout << "No standard library path has been configured yet.\n";
    stdlibOpt = setupStdlibPath();
  }

  std::string stdlibRoot = stdlibOpt.value();

  std::vector<std::string> inputs;
  for (int i = 1; i < argc; i++) {
    inputs.push_back(argv[i]);
  }

  if (inputs.empty()) {
    std::cerr << "Error: No input files provided.\n";
    return EXIT_FAILURE;
  }

  std::cout << "Compiling " << inputs.size() << " Nexus file(s)...\n";

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
    double tokPerS = tokens.size() / (elapsedS > 0 ? elapsedS : 1.0);

    std::cout << "Tokens: " << tokens.size() << "\n";
    std::cout << "Time (s): " << elapsedS << "\n";
    std::cout << "Tokens / second: " << tokPerS << "\n";

    // Parser
    Parser parser(tokens);
    auto parsed = parser.parse();

    if (!parsed) {
      std::cerr << "Parsing failed for " << file << "\n";
      continue;
    }

    fs::path projectRoot = fs::path(file).parent_path();

    ModuleManager mm(projectRoot, fs::path(stdlibRoot));
    mm.resolveAll(*parsed);

    CodeGenerator cg;
    if (!cg.generate(*parsed, "out")) {
      std::cerr << "Code generation failed for " << file << "\n";
      continue;
    }

    std::string output = getOutputName(file);

    std::string cmd = "clang -Wno-override-module -fsanitize=address "
                      "-fsanitize=leak -g out.ll -lglfw -lGL -o \"" +
                      output + "\"";

    std::cout << "Linking with Clang...\n";
    int res = std::system(cmd.c_str());
    // system("rm -rf out.ll");

    if (res != 0) {
      std::cerr << "Clang compilation failed for " << file << "\n";
      continue;
    }

    std::cout << "Successfully compiled: " << output << "\n";
    std::cout << "Run with: "
#ifdef _WIN32
              << output << "\n\n";
#else
              << "./" << output << "\n\n";
#endif
  }

  std::cout << "Done.\n";
  return EXIT_SUCCESS;
}
