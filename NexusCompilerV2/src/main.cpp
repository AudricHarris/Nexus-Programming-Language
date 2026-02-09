#include "FileReader/FileReader.h"
#include <cstdlib>
#include <iostream>

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: nexus input.nx\n";
    return EXIT_FAILURE;
  }

  readFile(argv[1]);

  return EXIT_SUCCESS;
}
