#include "Dictionary/TokenType.h"
#include "FileReader/FileReader.h"
#include "Lexer/Lexer.h"
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: nexus input.nx\n";
    return EXIT_FAILURE;
  }

  std::string code = readFile(argv[1]).value_or("");
  std::cout << "Clean code : \n" << code << "\n";

  std::cout << "Tokenizing : \n";
  Lexer l(code);
  std::vector<Token> lst = l.Tokenize();

  std::cout << "\n\nTokenized function : \n";
  for (size_t i = 0; i < lst.size(); i++) {
    std::cout << lst[i].toString();
  }

  for (size_t i = 0; i < lst.size(); i++) {
    std::cout << lst[i].getWord();
  }

  std::cout << "\n\nParsing Tokens : \n";

  return EXIT_SUCCESS;
}
