
#define TINYOBJLOADER_IMPLEMENTATION
#include "obj_loader.h"
#include "nvh/nvprint.hpp"

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

void ObjLoader::loadModel(const std::string &filename) {
	tinyobj::ObjReader reader;
	reader.ParseFromFile(filename);

	if (!reader.Valid()) {
		LOGE(reader.Error().c_str());
		std::cerr << "Cannot load: " << filename << std::endl;
		assert(reader.Valid());
	}

	for (const auto &m : reader.GetMaterials()) {
		MaterialObj mat;
		mat.ambient = nvmath::vec3f(m.ambient[0], m.ambient[1], m.ambient[2]);
		mat.diffuse = nvmath::vec3f(m.diffuse[0], m.diffuse[1], m.diffuse[2]);
		mat.specular = nvmath::vec3f(m.specular[0], m.specular[1], m.specular[2]);
		mat.emission = nvmath::vec3f(m.emission[0], m.emission[1], m.emission[2]);
		mat.transmittance = nvmath::vec3f(m.transmittance[0], m.transmittance[1], m.transmittance[2]);
		mat.dissolve = m.dissolve;
		mat.ior = m.ior;
		mat.shininess = m.shininess;
		mat.illumination = m.illum;
		if (!m.diffuse_texname.empty()) {
			textures.push_back(m.diffuse_texname);
			mat.texture_id = static_cast<int>(textures.size()) - 1;
		}
		materials.emplace_back(mat);
	}

	if (materials.empty())
		materials.emplace_back(MaterialObj());

	const auto &attrib = reader.GetAttrib();

	for (const auto &shape : reader.GetShapes()) {
		vertices.reserve(shape.mesh.indices.size() + vertices.size());
		indices.reserve(shape.mesh.indices.size() + indices.size());
		mat_index.insert(mat_index.end(), shape.mesh.material_ids.begin(), shape.mesh.material_ids.end());

		for (const auto &index : shape.mesh.indices) {
			VertexObj vertex = {};

			const float *vp = &attrib.vertices[3 * index.vertex_index];
			vertex.position = { *(vp + 0), *(vp + 1), *(vp + 2) };

			if (!attrib.normals.empty() && index.normal_index >= 0) {
				const float *np = &attrib.normals[3 * index.normal_index];
				vertex.normal = { *(np + 0), *(np + 1), *(np + 2) };
			}
			if (!attrib.texcoords.empty() && index.texcoord_index >= 0) {
				const float *tp = &attrib.texcoords[2 * index.texcoord_index + 0];
				vertex.tex_coord = { *tp, 1.0f - *(tp + 1) };
			}
			if (!attrib.colors.empty()) {
				const float *vc = &attrib.colors[3 * index.vertex_index];
				vertex.color = { *(vc + 0), *(vc + 1), *(vc + 2) };
			}

			vertices.push_back(vertex);
			indices.push_back(static_cast<int>(indices.size()));
		}
	}

	for (auto &i : mat_index) {
		if (i < 0 || i > materials.size()) {
			i = 0;
		}
	}

	if (attrib.normals.empty()) {
		for (size_t i = 0; i < indices.size(); i += 3) {
			auto &v0 = vertices[indices[i + 0]];
			auto &v1 = vertices[indices[i + 1]];
			auto &v2 = vertices[indices[i + 2]];

			auto n = nvmath::normalize(nvmath::cross((v1.position - v0.position), (v2.position - v0.position)));

			v0.normal = n;
			v1.normal = n;
			v2.normal = n;
		}
	}
}