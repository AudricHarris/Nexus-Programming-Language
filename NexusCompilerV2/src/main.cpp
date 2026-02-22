#include "CodeGen/CodeGen.h"
#include "Dictionary/TokenType.h"
#include "FileReader/FileReader.h"
#include "Lexer/Lexer.h"
#include "Parser/Parser.h"
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

int main(int argc, char *argv[]) {
  if (argc != 3) {
    std::cerr << "Usage: nexus input.nx output.x\n";
    return EXIT_FAILURE;
  }

  std::string code = readFile(argv[1]).value_or("");

  std::cout << "Tokenizing Errors : \n";
  Lexer l(code);
  std::vector<Token> lst = l.Tokenize();

  std::cout << "\n\nParsing Errors : \n";
  Parser parser(lst);
  auto parsed = parser.parse();
  std::cout << "\n\nParsing Tokens : \n";
  parsed->toJson(std::cout, 0);
  CodeGenerator cg;
  if (!cg.generate(*parsed, "out")) {
    std::cerr << "Codegen failed\n";
    return 1;
  }

  std::cout << "Now run:\n";
  std::string cmd = "clang out.ll -o " + std::string(argv[2]);
  system(cmd.c_str());
  system("rm -rf out.ll");
  std::cout << "./" << argv[2] << "\n";

  return EXIT_SUCCESS;
}
