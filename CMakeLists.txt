cmake_minimum_required(VERSION 3.17)
project(opal)

set(CMAKE_CXX_STANDARD 20)

file(GLOB SOURCE_FILES src/*.cpp src/*.hpp src/*.inl src/*.h src/*.c)

# Download automatically, you can also just copy the conan.cmake file
if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
    file(DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/v0.15/conan.cmake"
                "${CMAKE_BINARY_DIR}/conan.cmake")
endif()

include(${CMAKE_BINARY_DIR}/conan.cmake)
conan_cmake_run(CONANFILE conanfile.txt BASIC_SETUP)

#include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
#conan_basic_setup()

# include nvidia shared sources submodule
SET(BASE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/nvpro)
include(${BASE_DIRECTORY}/shared_sources/CMakeLists_include.txt)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    SET(CMAKE_INSTALL_PREFIX "${BASE_DIRECTORY}/_install" CACHE PATH "folder in which INSTALL will put everything needed to run the binaries" FORCE)
endif(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
if(VULKANSDK_LOCATION)
    string(REPLACE "\\" "/" VULKANSDK_LOCATION ${VULKANSDK_LOCATION})
endif()
if(CMAKE_INSTALL_PREFIX)
    string(REPLACE "\\" "/" CMAKE_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
endif()
if(MSVC)
    _make_relative("${EXECUTABLE_OUTPUT_PATH}/config" "${CMAKE_CURRENT_SOURCE_DIR}" TOPROJECT)
else()
    _make_relative("${EXECUTABLE_OUTPUT_PATH}" "${CMAKE_CURRENT_SOURCE_DIR}" TOPROJECT)
endif()
if(MSVC)
    _make_relative("${EXECUTABLE_OUTPUT_PATH}/config" "${DOWNLOAD_TARGET_DIR}" TO_DOWNLOAD_TARGET_DIR)
else()
    _make_relative("${EXECUTABLE_OUTPUT_PATH}" "${DOWNLOAD_TARGET_DIR}" TO_DOWNLOAD_TARGET_DIR)
endif()

add_definitions(-DPROJECT_RELDIRECTORY="${TOPROJECT}/")
add_definitions(-DPROJECT_ABSDIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/")
add_definitions(-DPROJECT_NAME="${PROJECT_NAME}")
add_definitions(-DPROJECT_DOWNLOAD_RELDIRECTORY="${TO_DOWNLOAD_TARGET_DIR}/")
add_definitions(-DPROJECT_DOWNLOAD_ABSDIRECTORY="${DOWNLOAD_TARGET_DIR}/")


_add_package_VulkanSDK()
_add_package_ImGUI()

#_add_shared_sources_lib()

add_subdirectory(${BASE_DIRECTORY}/shared_sources ${CMAKE_BINARY_DIR}/shared_sources)
#_optional_package_VulkanSDK()
#_add_package_ImGUI()

_process_shared_cmake_code()

# putting this into one of the other branches didn't work
if (WIN32)
    add_definitions(-DVK_USE_PLATFORM_WIN32_KHR)
else ()
    add_definitions(-DVK_USE_PLATFORM_XLIB_KHR)
    add_definitions(-DVK_USE_PLATFORM_XCB_KHR)
endif (WIN32)
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


if (${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "AMD64")
    set(GLSLC "$ENV{VULKAN_SDK}/Bin/glslc.exe")
    set(GLSLANGVALIDATOR "$ENV{VULKAN_SDK}/Bin/glslangValidator.exe")
    set(SPIRVDIS "$ENV{VULKAN_SDK}/Bin/spirv-dis.exe")
    set(SPIRVCFG "$ENV{VULKAN_SDK}/Bin/spirv-cfg.exe")
else ()
    set(GLSLC "glslc")
    set(GLSLANGVALIDATOR "glslangValidator")
endif ()


SET(VULKAN_TARGET_ENV vulkan1.2)

UNSET(GLSL_SOURCES)
UNSET(SPV_OUTPUT)
file(GLOB_RECURSE GLSL_HEADER_FILES "src/shaders/*.h" "src/shaders/*.glsl")
file(GLOB_RECURSE GLSL_SOURCE_FILES
        "src/shaders/*.comp"
        "src/shaders/*.frag"
        "src/shaders/*.vert"
        "src/shaders/*.rint"
        "src/shaders/*.rchit"
        "src/shaders/*.rahit"
        "src/shaders/*.rmiss"
        "src/shaders/*.rgen"
        )


foreach (GLSL ${GLSL_SOURCE_FILES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${PROJECT_BINARY_DIR}/shaders/${FILE_NAME}.spv")
    set(DOT "${PROJECT_BINARY_DIR}/shaders/${FILE_NAME}.dot")
    set(DIS "${PROJECT_BINARY_DIR}/shaders/${FILE_NAME}.log")
    add_custom_command(
            OUTPUT ${SPIRV}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/shaders/"
            COMMAND echo ${GLSLC} --target-env=${VULKAN_TARGET_ENV} -o ${SPIRV} ${GLSL}
            COMMAND ${GLSLC} --target-env=${VULKAN_TARGET_ENV} -o ${SPIRV} ${GLSL}
            # COMMAND ${SPIRVDIS} -o ${DIS} ${SPIRV} --color --offsets
            # COMMAND ${SPIRVCFG} -o ${DOT} ${SPIRV}
            MAIN_DEPENDENCY ${GLSL}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    # set_property(SOURCE ${GLSL} APPEND PROPERTY OBJECT_DEPENDS ${SPIRV})
    list(APPEND GLSL_SOURCES ${GLSL})
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach (GLSL)

list(APPEND GLSL_SOURCES ${GLSL_HEADER_FILES})
source_group(Shader_Files FILES ${GLSL_SOURCES})

add_custom_target(
        Shaders
        DEPENDS ${GLSL_HEADER_FILES} ${SPIRV_BINARY_FILES}
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
