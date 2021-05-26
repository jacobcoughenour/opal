#include "render_object.h"

using namespace Opal;

void MeshInstance::set_mesh(Renderer::Mesh *mesh) {
	this->_mesh = mesh;
	Renderer::get_singleton()->add_mesh(mesh);
}

void MeshInstance::set_material(Material *material) {
	this->_material = material;
}

void MeshInstance::init() {}

void MeshInstance::draw(DrawContext context) {

	MeshInstance *prev = (MeshInstance *)context.prev_object;

	// skip if previous object used the same material
	if (prev == nullptr || prev->_material != _material) {

		// bind the material

		auto rend = Renderer::get_singleton();

		vkCmdBindPipeline(
				context.cmd_buf,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				rend->_graphics_pipeline
				// material->pipeline
		);

		vkCmdBindDescriptorSets(
				context.cmd_buf,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				rend->_pipeline_layout,
				// material->pipeline_layout,
				0,
				1,
				&rend->_descriptor_sets[context.image_index],
				// &_descriptor_sets[image_index],
				0,
				nullptr);
	}

	// skip if previous object used the same mesh
	if (prev == nullptr || prev->_mesh != _mesh) {

		// send the geometry

		VkBuffer vertex_buffers[] = { _mesh->vertex_buffer.buffer };
		VkDeviceSize offsets[]	  = { 0 };

		vkCmdBindVertexBuffers(context.cmd_buf, 0, 1, vertex_buffers, offsets);
		vkCmdBindIndexBuffer(
				context.cmd_buf,
				_mesh->index_buffer.buffer,
				// offset
				0,
				// index type
				VK_INDEX_TYPE_UINT32);
	}

	// draw the geometry

	vkCmdDrawIndexed(
			context.cmd_buf,
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

void VolumeInstance::init() {}
void VolumeInstance::draw(DrawContext context) {}