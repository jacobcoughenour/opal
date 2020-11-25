#ifndef OPAL_VOX_LOADER_H
#define OPAL_VOX_LOADER_H

// #include "nvmath/nvmath.h"
#include <array>
#include <iostream>
#include <unordered_map>
#include <vector>

// https://github.com/ephtracy/voxel-model/blob/master/MagicaVoxel-file-format-vox.txt

struct MagicaVoxelChunk {
	// chunk id
	char chunk_id[4];
	// chunk size
	uint32_t chunk_size = { 0 };
};

class MagicaVoxelLoader {
public:
	void loadMagicaVoxelFile(const std::string &filename);

	char id[4];
	uint32_t version;

	std::vector<MagicaVoxelChunk> models;
	std::vector<MagicaVoxelChunk> palette;
	std::vector<MagicaVoxelChunk> materials;
};

#endif // OPAL_VOX_LOADER_H
