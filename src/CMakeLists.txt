add_subdirectory(renderer)
add_subdirectory(shaders)
add_subdirectory(utils)

set(${PROJECT_NAME}_SOURCES app.cpp app.h main.cpp)

add_executable(${PROJECT_NAME} ${${PROJECT_NAME}_SOURCES})

target_link_libraries(${PROJECT_NAME} ${CONAN_LIBS} renderer utils)

add_dependencies(${PROJECT_NAME} shaders)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders/"
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${PROJECT_BINARY_DIR}/shaders"
		"$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders")

# Make the executable a default target to build & run in Visual Studio
set_property(DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})