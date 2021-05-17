set(renderer_SOURCES 
	renderer.cpp 
	renderer.h 
	config.h 
	shader.h
	vk_mem_alloc.h
	vk_mem_alloc.cpp)

add_library(renderer ${renderer_SOURCES})

target_link_libraries(renderer utils ${CONAN_LIBS})