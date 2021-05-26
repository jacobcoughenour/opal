#ifndef __FILE_H__
#define __FILE_H__

#include <fstream>
#include <vector>

std::vector<char> readFile(const std::string &filename);

void writeFile(const std::string &filename, const char *content);

#endif // __FILE_H__