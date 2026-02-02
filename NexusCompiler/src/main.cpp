#include "Tokenizing/Token.h"
#include "Tokenizing/Lexer.h"
#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
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

	std::cout << "=== Tokenizing " << argv[1] << " ===\n\n";

	Lexer lexer(std::move(source));
	auto tokens = lexer.tokenize();
	
	printf("Tokenization is complete : \n");
	for (const auto& tok : tokens) {
		printf("Token   :  ");
		std::cout << tok.toString() << "\n";
	}

	return 0;
}
