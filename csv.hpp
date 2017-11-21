#ifndef CSV_HPP
#define CSV_HPP

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <map>

std::vector<std::string> csv_split(const char *s);
std::string csv_dequote(std::string s);
void readcsv(const char *fn, std::vector<std::string> &header, std::map<std::string, std::vector<std::string>> &mapping);
std::string csv_getline(FILE *f);
bool is_number(std::string const &s);

#endif
