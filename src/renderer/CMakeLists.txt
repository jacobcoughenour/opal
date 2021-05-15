set(renderer_SOURCES renderer.cpp renderer.h)

add_library(renderer ${renderer_SOURCES})

target_link_libraries(renderer utils ${CONAN_LIBS})