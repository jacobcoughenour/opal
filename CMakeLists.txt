cmake_minimum_required(VERSION 3.17)
project(opal)

set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

add_executable(opal src/main.cpp)

find_package(Vulkan REQUIRED)
target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} Vulkan::Vulkan)

target_link_libraries(${PROJECT_NAME} ${CONAN_LIBS})

#find_package(glm CONFIG REQUIRED)
#target_link_libraries(${PROJECT_NAME} glm)
#
#find_package(glfw3 CONFIG REQUIRED)
#target_link_libraries(${PROJECT_NAME} glfw)
#
#find_path(STB_INCLUDE_DIRs "stb.h")
#target_include_directories(${PROJECT_NAME} PRIVATE ${STB_INCLUDE_DIRS})

# shaders

if (${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "AMD64")
    set(GLSLC "$ENV{VULKAN_SDK}/Bin/glslc.exe")
else ()
    set(GLSLC "$ENV{VULKAN_SDK}/Bin32/glslc.exe")
endif ()

file(GLOB_RECURSE GLSL_SOURCE_FILES
        "src/shaders/*.frag"
        "src/shaders/*.vert"
        )

foreach (GLSL ${GLSL_SOURCE_FILES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${PROJECT_BINARY_DIR}/shaders/${FILE_NAME}.spv")
    add_custom_command(
            OUTPUT ${SPIRV}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/shaders/"
            COMMAND ${GLSLC} ${GLSL} -o ${SPIRV}
            DEPENDS ${GLSL})
    set_property(SOURCE ${GLSL} APPEND PROPERTY OBJECT_DEPENDS ${SPIRV})
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach (GLSL)

add_custom_target(
        Shaders
        DEPENDS ${SPIRV_BINARY_FILES}
)

add_dependencies(${PROJECT_NAME} Shaders)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders/"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_BINARY_DIR}/shaders"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders"
        )

# textures
# todo: make a resource building system

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/textures/"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_BINARY_DIR}/textures"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/textures"
        )
file(COPY "src/textures" DESTINATION ${PROJECT_BINARY_DIR})
