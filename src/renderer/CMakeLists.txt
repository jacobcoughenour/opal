set(renderer_SOURCES 
	config.h 
	render_object.h
	render_object.cpp
	renderer.h 
	renderer.cpp 
	vk_debug.h
	vk_mem_alloc.h
	vk_mem_alloc.cpp
	vk_shader.h
	vk_types.h)

add_library(renderer ${renderer_SOURCES})

target_link_libraries(renderer utils ${CONAN_LIBS})