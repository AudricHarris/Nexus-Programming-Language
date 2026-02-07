#include "Parsing/AST.h"
#include "Parsing/Parser.h"
#include "Tokenizing/Lexer.h"
#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: nexus <input.nx>\n";
    return 1;
  }

  std::ifstream file(argv[1]);
  if (!file.is_open()) {
    std::cerr << "Cannot open file: " << argv[1] << "\n";
    return 1;
  }

  std::string source((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>());

  std::cout << "File loaded: " << source.size() << " bytes\n";

  // === TOKENIZATION ===
  std::cout << "=== Tokenizing " << argv[1] << " ===\n\n";

  try {
    Lexer lexer(std::move(source));
    auto tokens = lexer.tokenize();

    std::cout << "Tokenization complete: " << tokens.size() << " tokens\n";

    // Print first 20 tokens to see what we got
    std::cout << "\nFirst tokens:\n";
    for (size_t i = 0; i < std::min(size_t(20), tokens.size()); i++) {
      std::cout << "  [" << i << "] " << tokens[i].toString() << "\n";
    }
    std::cout << "\n";

    // === PARSING ===
    std::cout << "=== Parsing ===\n\n";

    Parser parser(tokens);
    std::cout << "Parser created\n";

    auto program = parser.parse();
    std::cout << "Parse completed\n";

    if (program) {
      std::cout << "Parsing successful!\n\n";
      std::cout << "=== Abstract Syntax Tree ===\n\n";
      program->print(0);
    } else {
      std::cerr << "Error: Parser returned null program\n";
      return 1;
    }
  } catch (const ParseError &e) {
    std::cerr << "Parse error: " << e.what() << "\n";
    return 1;
  } catch (const std::exception &e) {
    std::cerr << "Unexpected error: " << e.what() << "\n";
    return 1;
  }

  std::cout << "\n=== Compilation completed successfully ===\n";
  return 0;
}
