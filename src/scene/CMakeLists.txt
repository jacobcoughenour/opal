set(scene_SOURCES 
	mesh_instance.h
	mesh_instance.cpp
	scene.h
	scene.cpp)

add_library(scene ${scene_SOURCES})

target_link_libraries(scene renderer utils ${CONAN_LIBS})