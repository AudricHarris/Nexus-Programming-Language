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

    std::cout << "File Content : \n\n" << content << "\n";
    std::cout << "Tokenized Content : \n\n" << "\n";
    uncommentedCode(content);
    return content;
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n";
    return std::nullopt;
  }
}


std::string uncommentedCode(std::string code)
{
  bool inComment = false;

  for (int i = 0; i < code.length(); i++)
    if (code[i] != '/' && code[i+1] != '!' || code[i+1] != '*')
      std::cout << "char :" << code[i] << "\n";
    else
      if ( code[++i] == '*')
        while (i<code.length() && code[i++] != '\n') {}
      std::cerr << "Found Next line \n";

  return code;
}
