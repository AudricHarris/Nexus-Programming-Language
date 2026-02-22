#include "FileReader.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>

std::string fileToString(const std::string &filename) {
  std::ifstream file(filename, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(file),
                     std::istreambuf_iterator<char>());
}

std::optional<std::string> readFile(char *name) {
  std::string filename = name;

  try {

    std::cout << "File : " << filename << "\n";
    std::string content = fileToString(filename);
    return uncommentedCode(content);

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n";
    return std::nullopt;
  }
}

std::string uncommentedCode(std::string code) {
  std::string clean;
  int n = code.length();
  int i = 0;

  while (i < n) {
    if (i + 2 < n && code[i] == '/' && code[i + 1] == '*') {
      i += 2;
      while (i < n && code[i] != '\n') {
        i++;
      }
    } else if (i + 2 < n && code[i] == '/' && code[i + 1] == '!') {
      i += 2;
      while (i + 1 < n && !(code[i] == '!' && code[i + 1] == '/')) {
        i++;
      }
      if (i + 1 < n)
        i += 2;
    } else {
      clean += code[i];
      i++;
    }
  }

  return clean;
}
