include_directories(${CMAKE_SOURCE_DIR}/include)

add_executable(example ${CMAKE_CURRENT_SOURCE_DIR}/example.cpp)
add_dependencies(example fceux_static)
target_link_libraries(example fceux_static)
