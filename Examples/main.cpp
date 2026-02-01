#include <cstdlib>
#include <fstream>
#include <iostream>

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "Incorrect usage.  Correct usage is nexus <input.nx>"
              << std::endl;
    return EXIT_FAILURE;
  }

  std::fstream input(argv[1], std::ios::in);

  return EXIT_SUCCESS;
}
