#include "file.h"

std::vector<char> readFile(const std::string &filename) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		throw std::runtime_error("Failed to open file: " + filename);
	}

	// get size of file
	size_t file_size = (size_t)file.tellg();

	// create a buffer to fit the file contents
	std::vector<char> buffer(file_size);

	// read the whole file and stream it into the buffer
	file.seekg(0);
	file.read(buffer.data(), static_cast<std::streamsize>(file_size));

	file.close();

	return buffer;
}

void writeFile(const std::string &filename, const char *content) {

	std::ofstream file(filename, std::ios::binary);

	if (!file.is_open()) {
		throw std::runtime_error("Failed to open file: " + filename);
	}

	file << content;

	file.close();
}
