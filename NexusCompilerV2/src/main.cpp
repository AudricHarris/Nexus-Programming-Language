#include "FileReader/FileReader.h"
#include "Lexer/Lexer.h"
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: nexus input.nx\n";
    return EXIT_FAILURE;
  }

  std::string code = readFile(argv[1]).value_or("");
  std::cout << "Clean code : \n" << code << "\n";

  std::cout << "Tokenizing : \n";
  Lexer l(code);
  l.Tokenize();

  return EXIT_SUCCESS;
}
