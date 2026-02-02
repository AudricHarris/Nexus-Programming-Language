#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <ostream>

// ----------------------- //
// Steps for my compiler   //
// ----------------------- //

/*
 * 1- Read source
 * 2- Tokenize
 * 3- Parse -> AST
 * 4- Codegen -> LLVM Module
 * 5- Write object file / Executable
 */


int main (int argc, char *argv[]) {
	if (argc < 2)
	{
		std::cerr << "Incorrect usage. Correct usage is nexus <input.nx>" << std::endl;
		return EXIT_FAILURE;
	}
	
	std::ifstream f(argv[1]);
	if (f.is_open())
	{
		printf("Nexus file Opened\n");
		std::cout << f.rdbuf();
	}
	else
	{
		printf("File doesn't exist\n");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
