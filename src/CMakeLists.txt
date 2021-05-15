add_subdirectory(renderer)
add_subdirectory(utils)

set(${PROJECT_NAME}_SOURCES app.cpp app.h main.cpp)

add_executable(${PROJECT_NAME} ${${PROJECT_NAME}_SOURCES})

target_link_libraries(${PROJECT_NAME} ${CONAN_LIBS} renderer utils)

# Make the executable a default target to build & run in Visual Studio
set_property(DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})