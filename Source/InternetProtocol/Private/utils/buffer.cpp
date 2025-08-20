/** 
 * Copyright © 2025 Nathan Miguel
 */

#include "utils/buffer.h"

std::string trim_whitespace(const std::string& str) {
	std::string::const_iterator start = str.begin();
	while (start != str.end() && std::isspace(*start)) {
		start++;
	}

	std::string::const_iterator end = str.end();
	do {
		end--;
	} while (std::distance(start, end) > 0 && std::isspace(*end));

	return std::string(start, end + 1);
}

void string_to_lower(std::string& str) {
	std::string lower_str;
	lower_str.reserve(str.size());
	for (char &c : str) {
		c = static_cast<char>(std::tolower(c));
	}
}
