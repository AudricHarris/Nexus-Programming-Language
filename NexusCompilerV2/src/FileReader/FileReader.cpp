#include "FileReader.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

std::string fileToString(const std::string &filename) {
  std::ifstream file(filename, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(file),
                     std::istreambuf_iterator<char>());
}

void readFile(char *name) {
  std::string filename = name;

  try {

    std::cout << "File : " << filename << "\n";
    std::string content = fileToString(filename);

    std::cout << "File Content : \n\n" << content << "\n";
    std::cout << "Tokenized Content : \n\n" << "\n";

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n";
  }
}
