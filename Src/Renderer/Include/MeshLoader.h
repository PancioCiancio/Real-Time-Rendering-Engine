//
// Created by apant on 20/07/2025.
//

#ifndef MESH_H
#define MESH_H

#include <cstdint>
#include <vector>
#include <assimp/scene.h>

#include <glm/glm.hpp>


namespace Renderer
{

class BatchCpu;

class Mesh
{
	Mesh() = delete;

public:
	static void Load(
		const char* file_path,
		BatchCpu*   batch);

private:
	static void QueryVerticesCount(
		const aiScene* scene,
		uint32_t*      vertices_count);

	static void QueryVertecesPosition(
		const aiScene*          scene,
		std::vector<glm::vec3>& positions);

	static void QueryVertecesNormal(
		const aiScene*          scene,
		std::vector<glm::vec3>& normals);

	static void QueryIndicesCount(
		const aiScene* scene,
		uint32_t*      indeces_count);

	static void QueryIndices(
		const aiScene*         scene,
		std::vector<uint32_t>& indices);
};

}
#endif //MESH_H
