#ifndef PARSE_ERROR_H
#define PARSE_ERROR_H

#include <stdexcept>
#include <string>

class ParseError : public std::runtime_error {
public:
  int line;
  int column;

  ParseError(int l, int c, const std::string &message)
      : std::runtime_error(message), line(l), column(c) {}
};

#endif
