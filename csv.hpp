#ifndef CSV_HPP
#define CSV_HPP

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <map>

std::vector<std::string> csv_split(char *s);
std::string csv_dequote(std::string s);
void readcsv(char *fn, std::vector<std::string> &header, std::map<std::string, std::vector<std::string>> &mapping);

#endif
