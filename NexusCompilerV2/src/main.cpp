#include "FileReader/FileReader.h"
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

  return EXIT_SUCCESS;
}
