cmake_minimum_required(VERSION 3.17)
project(opal)

set(CMAKE_CXX_STANDARD 20)

#file(GLOB SOURCE_FILES src/*.cpp src/*.hpp src/*.inl src/*.h src/*.c)
set(SOURCE_FILES)

LIST(APPEND SOURCE_FILES
        src/main.cpp
        src/opal.cpp
        src/opal.h
        )

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

# include nvidia shared sources submodule
SET(BASE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/nvpro)
include(${BASE_DIRECTORY}/shared_sources/CMakeLists_include.txt)



_add_package_VulkanSDK()
_add_package_ImGUI()

#_add_shared_sources_lib()

add_subdirectory(${BASE_DIRECTORY}/shared_sources ${CMAKE_BINARY_DIR}/shared_sources)
#_optional_package_VulkanSDK()
#_add_package_ImGUI()

_process_shared_cmake_code()

# putting this into one of the other branches didn't work
if(WIN32)
    add_definitions(-DVK_USE_PLATFORM_WIN32_KHR)
else()
    add_definitions(-DVK_USE_PLATFORM_XLIB_KHR)
    add_definitions(-DVK_USE_PLATFORM_XCB_KHR)
endif(WIN32)
add_definitions(-DVULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1)


add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${PACKAGE_SOURCE_FILES})

# include vulkan sdk
#find_package(Vulkan REQUIRED)
#target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIRS})
#target_link_libraries(${PROJECT_NAME} Vulkan::Vulkan)


target_link_libraries(${PROJECT_NAME} ${CONAN_LIBS})
target_link_libraries(${PROJECT_NAME} ${PLATFORM_LIBRARIES})
target_link_libraries(${PROJECT_NAME} shared_sources)

_copy_binaries_to_target(${PROJECT_NAME})




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
