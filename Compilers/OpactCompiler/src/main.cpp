// My Packages
#include "FileReader/FileReader.hpp"
#include "Lexer/Lexer.hpp"
#include "Token/TokenType.hpp"

// External packages
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

// Check if Code is windows or not

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
    return fs::current_path() / "opact_config";
  }

#ifdef _WIN32
  // Windows: %APPDATA%\nexus
  const char *appdata = std::getenv("APPDATA");
  if (appdata) {
    return fs::path(appdata) / "opact";
  }
  return fs::path(home) / "AppData" / "Roaming" / "opact";
#else
  // Linux/macOS: ~/.config/opact
  return fs::path(home) / ".config" / "opact";
#endif
}

// ------------------ //
// Utility functions  //
// ------------------ //

bool endsWith(const std::string &str, const std::string &suffix) {
  if (str.length() < suffix.length())
    return false;
  return str.compare(str.length() - suffix.length(), suffix.length(), suffix) ==
         0;
}

bool hasValidExt(const std::string &f) { return endsWith(f, ".op"); }

std::string getOutputName(const std::string &file) {
  size_t pos = file.find_last_of('.');
  std::string base = (pos == std::string::npos) ? file : file.substr(0, pos);

#ifdef _WIN32
  return base + ".exe";
#else
  return base + ".x";
#endif
}

// Main function

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: opact [options] [files...]\n";
    std::cerr << "Options:\n";
    std::cerr << "  --version     Show version information\n";
    return EXIT_FAILURE;
  }

  std::string firstArg = argv[1];

  if (firstArg == "--version") {
    std::cout << "opact [2026.07.20]\n";
    return 0;
  }

  std::vector<std::string> inputs;
  for (int i = 1; i < argc; i++)
    inputs.push_back(argv[i]);

  return EXIT_SUCCESS;
}
