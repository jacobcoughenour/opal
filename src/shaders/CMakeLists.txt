
if (${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "AMD64")
	set(GLSLC "$ENV{VULKAN_SDK}/Bin/glslc.exe")
	set(GLSLANGVALIDATOR "$ENV{VULKAN_SDK}/Bin/glslangValidator.exe")
else ()
	set(GLSLC "glslc")
	set(GLSLANGVALIDATOR "glslangValidator")
endif ()

file(GLOB_RECURSE GLSL_HEADER_FILES
	"*.h"
	"*.glsl")
file(GLOB_RECURSE GLSL_SOURCE_FILES
	"*.comp"
	"*.frag"
	"*.vert"
	"*.rint"
	"*.rchit"
	"*.rahit"
	"*.rmiss"
	"*.rgen")

foreach(GLSL ${GLSL_SOURCE_FILES})
	get_filename_component(FILE_NAME ${GLSL} NAME)
	set(SPIRV "${PROJECT_BINARY_DIR}/shaders/${FILE_NAME}.spv")
	add_custom_command(
		OUTPUT ${SPIRV}
		COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/shaders/"
		COMMAND ${GLSLC} --target-spv=${SPIRV_TARGET_ENV} -o ${SPIRV} ${GLSL}
		MAIN_DEPENDENCY ${GLSL}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
	list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

list(APPEND GLSL_SOURCES ${GLSL_HEADER_FILES})
source_group(Shader_Files FILES ${GLSL_SOURCES})

add_custom_target(
	shaders 
	DEPENDS ${GLSL_HEADER_FILES} ${SPIRV_BINARY_FILES})
