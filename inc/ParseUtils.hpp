#ifndef PARSEUTILS_HPP
#define PARSEUTILS_HPP

#include "WebServ.hpp"
#include "InvalidFormat.hpp"

std::vector<std::string>	extractQuotedArgs(const std::string var, const std::string line);
std::string extractSinglePath(const std::string var, const std::string line);
std::string	trim(std::string line);
size_t		findLineEnd(const std::string line);
bool		isDirectory(const std::string path);

#endif
