#include "CodeGen/CodeGen.h"
#include "FileReader/FileReader.h"
#include "Lexer/Lexer.h"
#include "Parser/Parser.h"
#include "Token/TokenType.h"
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

int main(int argc, char *argv[]) {
  if (argc != 3) {
    if (argc == 2 && std::string(argv[1]) == "--version") {
      std::cout << "nexus 1.4.2" << std::endl;
      return 0;
    };
    std::cerr << "Usage: nexus input.nx output.x\n";
    return EXIT_FAILURE;
  }

  std::string code = readFile(argv[1]).value_or("");

  std::cout << "Errors : \n";
  Lexer l(code);

  auto start = std::chrono::high_resolution_clock::now();
  std::vector<Token> lst = l.Tokenize();
  auto end = std::chrono::high_resolution_clock::now();

  double elapsedMs =
      std::chrono::duration<double, std::milli>(end - start).count();
  double elapsedS = elapsedMs / 1000.0;
  double tokPerS = lst.size() / elapsedS;

  std::cout << "Tokens:          " << lst.size() << "\n";
  std::cout << "Time (ms):       " << elapsedS << "\n";
  std::cout << "Tokens / second: " << tokPerS << "\n";

  // for (size_t i = 0; i < lst.size(); i++)
  // std::cout << lst[i].toString();

  // td::cout << "Errors : \n";
  Parser parser(lst);
  auto parsed = parser.parse();
  // parsed->toJson(std::cout);
  CodeGenerator cg;
  if (!cg.generate(*parsed, "out")) {
    std::cerr << "Codegen failed\n";
    return 1;
  }

  std::cout << "\nNow run:\n";

  std::string cmd = "clang -Wno-override-module -fsanitize=address "
                    "-fsanitize=leak -g out.ll  -o " +
                    std::string(argv[2]);
  system(cmd.c_str());
  // system("rm -rf out.ll");
  std::cout << "./" << argv[2] << "\n";

  return EXIT_SUCCESS;
}
