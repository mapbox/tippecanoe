#ifndef TEXT_HPP
#define TEXT_HPP

#include <string>

std::string check_utf8(std::string text);
const char *utf8_next(const char *s, long *c);
std::string truncate16(std::string const &s, size_t runes);
void aprintf(std::string *buf, const char *format, ...);
int integer_zoom(std::string where, std::string text);

#endif
