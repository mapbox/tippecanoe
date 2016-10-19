#include "text.hpp"
#include <stdio.h>

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
