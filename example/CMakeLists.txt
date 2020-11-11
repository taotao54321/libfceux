include_directories(${CMAKE_SOURCE_DIR}/include)

find_package(SDL2 REQUIRED)
find_package(fmt REQUIRED)

add_executable(example ${CMAKE_CURRENT_SOURCE_DIR}/example.cpp)
add_dependencies(example fceux_static)
target_compile_features(example PRIVATE cxx_std_17)
target_include_directories(example PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(example PRIVATE ${SDL2_LIBRARIES} fmt::fmt fceux_static)
