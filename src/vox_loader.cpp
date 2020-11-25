#include "vox_loader.h"
// #include "nvh/nvprint.hpp"
#include <fstream>

#ifdef WIN32
#define OS_PATH_SEPARATOR "\\"
#else
#define OS_PATH_SEPARATOR "/"
#endif

static inline std::string get_path(const std::string &file) {
	std::string dir;
	size_t index = file.find_last_of("\\/");
	if (index != std::string::npos)
		dir = file.substr(0, index);
	if (!dir.empty())
		dir += OS_PATH_SEPARATOR;
	return dir;
}

void MagicaVoxelLoader::loadMagicaVoxelFile(const std::string &filename) {

	std::ifstream file;

	file.open(filename.c_str());

	while (!file.eof()) {
	}
}