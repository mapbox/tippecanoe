#include <iostream>

#ifndef LOGGING_HPP
#define LOGGING_HPP

struct json_logger {
	bool json_enabled = false;

	void progress_tile(double progress);
};

#endif