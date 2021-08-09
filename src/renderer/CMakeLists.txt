set(renderer_SOURCES 
	config.h 
	render_object.h
	render_object.cpp
	renderer.h 
	renderer.cpp 
	vk_debug.h
	vk_shader.h
	vk_types.h
	)

add_library(renderer ${renderer_SOURCES})

# thirdparty stuff

set(vma_SOURCES
	${THIRDPARTY_DIR}/vulkan-memory-allocator/vk_mem_alloc.h
	${THIRDPARTY_DIR}/vulkan-memory-allocator/vk_mem_alloc.cpp)
add_library(vk_mem_alloc ${vma_SOURCES})
set_target_properties(vk_mem_alloc PROPERTIES COMPILE_FLAGS "-w")

set(spirv_reflect_SOURCES
	${THIRDPARTY_DIR}/spirv-reflect/include/spirv/unified1/spirv.h
	${THIRDPARTY_DIR}/spirv-reflect/spirv_reflect.h
	${THIRDPARTY_DIR}/spirv-reflect/spirv_reflect.c)
add_library(spirv_reflect ${spirv_reflect_SOURCES})
set_target_properties(spirv_reflect PROPERTIES COMPILE_FLAGS "-w")

target_link_libraries(renderer utils vk_mem_alloc spirv_reflect ${CONAN_LIBS})