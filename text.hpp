#ifndef TEXT_HPP
#define TEXT_HPP

#include <string>

std::string check_utf8(std::string text);
const char *utf8_next(const char *s, int64_t *c);
std::string truncate16(std::string const &s, size_t runes);
int32_t integer_zoom(std::string where, std::string text);

#endif
