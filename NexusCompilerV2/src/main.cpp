#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

std::string fileToString(const std::string &filename) {
  std::ifstream file(filename);
  return std::string(std::istreambuf_iterator<char>(file),
                     std::istreambuf_iterator<char>());
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: nexus input.nx\n";
    return 1;
  }

  std::string filename = argv[1];

  std::ifstream file(filename);
  if (!file) {
    throw std::runtime_error("Cannot open file: " + filename);
  }

  try {
    std::string content = fileToString(filename);

    std::cout << "File Content : \n\n" << content << "\n";
    std::cout << "Tokenized Content : \n\n" << "\n";

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }

  return 0;
}
