cmake_minimum_required(VERSION 3.4)
project(opal)

set(CMAKE_CXX_STANDARD 20)

# Download automatically, you can also just copy the conan.cmake file
if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
    file(DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/v0.15/conan.cmake"
                "${CMAKE_BINARY_DIR}/conan.cmake")
endif()

SET(VULKAN_TARGET_ENV vulkan1.1)
SET(SPIRV_TARGET_ENV spirv1.4)

if (WIN32)
    add_definitions(-DVK_USE_PLATFORM_WIN32_KHR)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
else ()
    add_definitions(-DVK_USE_PLATFORM_XLIB_KHR)
    add_definitions(-DVK_USE_PLATFORM_XCB_KHR)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_XLIB_KHR)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_XCB_KHR)
endif (WIN32)
add_definitions(-DVULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1)

# this disables the annoying warnings from building conan libraries
set(CONAN_SYSTEM_INCLUDES ON)

include(${CMAKE_BINARY_DIR}/conan.cmake)
conan_cmake_run(CONANFILE conanfile.txt BASIC_SETUP BUILD missing PROFILE clang)

SET(THIRDPARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty)

add_subdirectory(src)
