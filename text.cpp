#include "text.hpp"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

/**
 * Returns an empty string if `s` is valid utf8;
 * otherwise returns an error message.
 */
std::string check_utf8(std::string s) {
	for (size_t i = 0; i < s.size(); i++) {
		size_t fail = 0;

		if ((s[i] & 0x80) == 0x80) {
			if ((s[i] & 0xE0) == 0xC0) {
				if (i + 1 >= s.size() || (s[i + 1] & 0xC0) != 0x80) {
					fail = 2;
				} else {
					i += 1;
				}
			} else if ((s[i] & 0xF0) == 0xE0) {
				if (i + 2 >= s.size() || (s[i + 1] & 0xC0) != 0x80 || (s[i + 2] & 0xC0) != 0x80) {
					fail = 3;
				} else {
					i += 2;
				}
			} else if ((s[i] & 0xF8) == 0xF0) {
				if (i + 3 >= s.size() || (s[i + 1] & 0xC0) != 0x80 || (s[i + 2] & 0xC0) != 0x80 || (s[i + 3] & 0xC0) != 0x80) {
					fail = 4;
				} else {
					i += 3;
				}
			} else {
				fail = 1;
			}
		}

		if (fail != 0) {
			std::string out = "\"" + s + "\" is not valid UTF-8 (";
			for (size_t j = 0; j < fail && i + j < s.size(); j++) {
				if (j != 0) {
					out += " ";
				}
				char tmp[6];
				sprintf(tmp, "0x%02X", s[i + j] & 0xFF);
				out += std::string(tmp);
			}
			out += ")";
			return out;
		}
	}

	return "";
}

const char *utf8_next(const char *s, long *c) {
	if (s == NULL) {
		*c = -1;
		return NULL;
	}

	if (*s == '\0') {
		*c = -1;
		return NULL;
	}

	if ((s[0] & 0x80) == 0x80) {
		if ((s[0] & 0xE0) == 0xC0) {
			if ((s[1] & 0xC0) != 0x80) {
				*c = 0xFFFD;
				s++;
			} else {
				*c = ((long) (s[0] & 0x1F) << 6) | ((long) (s[1] & 0x7F));
				s += 2;
			}
		} else if ((s[0] & 0xF0) == 0xE0) {
			if ((s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80) {
				*c = 0xFFFD;
				s++;
			} else {
				*c = ((long) (s[0] & 0x0F) << 12) | ((long) (s[1] & 0x7F) << 6) | ((long) (s[2] & 0x7F));
				s += 3;
			}
		} else if ((s[0] & 0xF8) == 0xF0) {
			if ((s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80 || (s[3] & 0xC0) != 0x80) {
				*c = 0xFFFD;
				s++;
			} else {
				*c = ((long) (s[0] & 0x0F) << 18) | ((long) (s[1] & 0x7F) << 12) | ((long) (s[2] & 0x7F) << 6) | ((long) (s[3] & 0x7F));
				s += 4;
			}
		} else {
			*c = 0xFFFD;
			s++;
		}
	} else {
		*c = s[0];
		s++;
	}

	return s;
}

std::string truncate16(std::string const &s, size_t runes) {
	const char *cp = s.c_str();
	const char *start = cp;
	const char *lastgood = cp;
	size_t len = 0;
	long c;

	while ((cp = utf8_next(cp, &c)) != NULL) {
		if (c <= 0xFFFF) {
			len++;
		} else {
			len += 2;
		}

		if (len <= runes) {
			lastgood = cp;
		} else {
			break;
		}
	}

	return std::string(s, 0, lastgood - start);
}

int integer_zoom(std::string where, std::string text) {
	double d = atof(text.c_str());
	if (!isfinite(d) || d != floor(d) || d < 0 || d > 32) {
		fprintf(stderr, "%s: Expected integer zoom level in \"tippecanoe\" GeoJSON extension, not %s\n", where.c_str(), text.c_str());
		exit(EXIT_FAILURE);
	}
	return d;
}

std::string format_commandline(int argc, char **argv) {
	std::string out;

	for (int i = 0; i < argc; i++) {
		bool need_quote = false;
		for (char *cp = argv[i]; *cp != '\0'; cp++) {
			if (!isalpha(*cp) && !isdigit(*cp) &&
			    *cp != '/' && *cp != '-' && *cp != '_' && *cp != '@' && *cp != ':' &&
			    *cp != '.' && *cp != '%' && *cp != ',') {
				need_quote = true;
				break;
			}
		}

		if (need_quote) {
			out.push_back('\'');
			for (char *cp = argv[i]; *cp != '\0'; cp++) {
				if (*cp == '\'') {
					out.append("'\"'\"'");
				} else {
					out.push_back(*cp);
				}
			}
			out.push_back('\'');
		} else {
			out.append(argv[i]);
		}

		if (i + 1 < argc) {
			out.push_back(' ');
		}
	}

	return out;
}
