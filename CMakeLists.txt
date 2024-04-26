cmake_minimum_required(VERSION 3.6)
project(camera)

add_executable(camera main.cpp)
target_compile_features(camera PRIVATE cxx_std_20)
target_include_directories(camera PRIVATE include)
target_link_directories(camera PRIVATE lib)
target_link_libraries(camera PRIVATE raylib winmm)
target_precompile_headers(camera PRIVATE <glm/glm.hpp>)
target_compile_options(camera PRIVATE
    $<$<CONFIG:Release>:-O3>
)
target_compile_definitions(camera PRIVATE
    $<$<CONFIG:Release>:NDEBUG>
)
