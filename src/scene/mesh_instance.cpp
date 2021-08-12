#include "mesh_instance.h"

using namespace Opal;

void MeshInstance::set_mesh(Renderer::Mesh *mesh) {
	this->_mesh = mesh;
	Renderer::get_singleton()->add_mesh(mesh);
}

void MeshInstance::set_material(Material *material) {
	this->_material = material;
}

void MeshInstance::init() {

	// static auto start_time = std::chrono::high_resolution_clock::now();
	// auto current_time	   = std::chrono::high_resolution_clock::now();

	// float time = std::chrono::duration<float, std::chrono::seconds::period>(
	// 					 current_time - start_time)
	// 					 .count();

	// transform = glm::rotate(
	// 		glm::mat4(1.0f),
	// 		time * glm::radians(90.0f),
	// 		glm::vec3(0.0f, 0.0f, 1.0f));
}

void MeshInstance::update(float delta) {

	// static auto start_time = std::chrono::high_resolution_clock::now();
	// auto current_time	   = std::chrono::high_resolution_clock::now();

	// float time = std::chrono::duration<float, std::chrono::seconds::period>(
	// 					 current_time - start_time)
	// 					 .count();

	// transform = glm::rotate(
	// 		glm::mat4(1.0f),
	// 		time * glm::radians(90.0f),
	// 		glm::vec3(0.0f, 0.0f, 1.0f));
}

void MeshInstance::draw(DrawContext *context) {

	MeshInstance *prev = (MeshInstance *)context->prev_object;

	// skip if previous object used the same material
	if (prev == nullptr || prev->_material != _material) {

		// bind the material

		vkCmdBindPipeline(
				context->cmd_buf,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				context->renderer->_graphics_pipeline
				// material->pipeline
		);

		vkCmdBindDescriptorSets(
				context->cmd_buf,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				context->renderer->_pipeline_layout,
				// material->pipeline_layout,
				0,
				1,
				&context->renderer->_descriptor_sets[context->image_index],
				// &_descriptor_sets[image_index],
				0,
				nullptr);
	}

	// send push constants

	Renderer::PushConstants push_constants {
		.model = transform,
		.view  = context->view,
		.proj  = context->proj,
	};
	vkCmdPushConstants(
			context->cmd_buf,
			context->renderer->_pipeline_layout,
			VK_SHADER_STAGE_VERTEX_BIT,
			0,
			sizeof(Renderer::PushConstants),
			&push_constants);

	// skip if previous object used the same mesh
	if (prev == nullptr || prev->_mesh != _mesh) {

		// send the geometry

		VkBuffer vertex_buffers[] = { _mesh->vertex_buffer.buffer };
		VkDeviceSize offsets[]	  = { 0 };

		vkCmdBindVertexBuffers(context->cmd_buf, 0, 1, vertex_buffers, offsets);
		vkCmdBindIndexBuffer(
				context->cmd_buf,
				_mesh->index_buffer.buffer,
				// offset
				0,
				// index type
				VK_INDEX_TYPE_UINT32);
	}

	// draw the geometry

	vkCmdDrawIndexed(
			context->cmd_buf,
			// index count
			static_cast<uint32_t>(_mesh->indices.size()),
			// instance count
			1,
			// first index
			0,
			// vertex offset
			0,
			// first instance
			0);
}
