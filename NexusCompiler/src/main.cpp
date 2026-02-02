#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <ostream>
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
