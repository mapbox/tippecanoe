#include "csv.hpp"

#define MAXLINE 10000 /* XXX */

std::vector<std::string> csv_split(char *s) {
	std::vector<std::string> ret;

	while (*s && *s != '\n' && *s != '\r') {
		char *start = s;
		int within = 0;

		for (; *s && *s != '\n' && *s != '\r'; s++) {
			if (*s == '"') {
				within = !within;
			}

			if (*s == ',' && !within) {
				break;
			}
		}

		std::string v = std::string(start, s - start);
		ret.push_back(v);

		if (*s == ',') {
			s++;

			while (*s && isspace(*s)) {
				s++;
			}
		}
	}

	return ret;
}

std::string csv_dequote(std::string s) {
	std::string out;
	for (size_t i = 0; i < s.size(); i++) {
		if (s[i] == '"') {
			if (i + 1 < s.size() && s[i + 1] == '"') {
				out.push_back('"');
			}
		} else {
			out.push_back(s[i]);
		}
	}
	return out;
}

void readcsv(char *fn, std::vector<std::string> &header, std::map<std::string, std::vector<std::string>> &mapping) {
	FILE *f = fopen(fn, "r");
	if (f == NULL) {
		perror(fn);
		exit(EXIT_FAILURE);
	}

	char s[MAXLINE];
	if (fgets(s, MAXLINE, f)) {
		header = csv_split(s);

		for (size_t i = 0; i < header.size(); i++) {
			header[i] = csv_dequote(header[i]);
		}
	}
	while (fgets(s, MAXLINE, f)) {
		std::vector<std::string> line = csv_split(s);
		if (line.size() > 0) {
			line[0] = csv_dequote(line[0]);
		}

		for (size_t i = 0; i < line.size() && i < header.size(); i++) {
			// printf("putting %s\n", line[0].c_str());
			mapping.insert(std::pair<std::string, std::vector<std::string>>(line[0], line));
		}
	}

	if (fclose(f) != 0) {
		perror("fclose");
		exit(EXIT_FAILURE);
	}
}

