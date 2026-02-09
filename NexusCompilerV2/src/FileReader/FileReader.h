#ifndef File_Reader
#define File_Reader

#include <optional>
#include <string>

std::optional<std::string> readFile(char *name);
std::string uncommentedCode(std::string code);

#endif
