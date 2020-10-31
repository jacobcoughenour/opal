#ifndef OPAL_OBJ_LOADER_H
#define OPAL_OBJ_LOADER_H

#include "fileformats/tiny_obj_loader.h"
#include "nvmath/nvmath.h"
#include <array>
#include <iostream>
#include <unordered_map>
#include <vector>

struct MaterialObj {
	nvmath::vec3f ambient = nvmath::vec3f(0.1f, 0.1f, 0.1f);
	nvmath::vec3f diffuse = nvmath::vec3f(0.1f, 0.1f, 0.1f);
	nvmath::vec3f specular = nvmath::vec3f(0.1f, 0.1f, 0.1f);
	nvmath::vec3f transmittance = nvmath::vec3f(0.1f, 0.1f, 0.1f);
	nvmath::vec3f emission = nvmath::vec3f(0.1f, 0.1f, 0.1f);
	float shininess = 0.f;
	float ior = 1.0f;
	float dissolve = 1.f;
	int illumination = 0;
	int texture_id = -1;
};

struct VertexObj {
	nvmath::vec3f position;
	nvmath::vec3f normal;
	nvmath::vec3f color;
	nvmath::vec2f tex_coord;
};

struct ShapeObj {
	uint32_t offset;
	uint32_t nb_index;
	uint32_t mat_index;
};

class ObjLoader {
public:
	void loadModel(const std::string &filename);

	std::vector<VertexObj> vertices;
	std::vector<uint32_t> indices;
	std::vector<MaterialObj> materials;
	std::vector<std::string> textures;
	std::vector<uint32_t> mat_index;
};

#endif // OPAL_OBJ_LOADER_H
