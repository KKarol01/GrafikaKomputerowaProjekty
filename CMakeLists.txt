cmake_minimum_required(VERSION 3.6)
project(brdf)

add_library(imgui STATIC 
    imgui/imgui_draw.cpp
    imgui/imgui_impl_glfw.cpp
    imgui/imgui_impl_opengl3.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui.cpp
)

target_include_directories(imgui PUBLIC ${CMAKE_SOURCE_DIR}/include INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_directories(imgui PUBLIC ${CMAKE_SOURCE_DIR}/lib)
target_link_libraries(imgui PUBLIC glfw3dll)

add_executable(brdf main.cpp glad.c)
target_compile_features(brdf PRIVATE cxx_std_23)
target_link_libraries(brdf PRIVATE imgui)
target_compile_options(brdf PRIVATE
    $<$<CONFIG:Release>:-O3>
)
target_precompile_headers(brdf PRIVATE
    <glad/glad.h>
    <GLFW/glfw3.h>
)
target_compile_definitions(brdf PRIVATE
    $<$<CONFIG:Release>:NDEBUG>
)

configure_file(${CMAKE_SOURCE_DIR}/shared/glfw3.dll ${CMAKE_BINARY_DIR}/glfw3.dll COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/shaders/raytrace.frag ${CMAKE_BINARY_DIR}/shaders/raytrace.frag COPYONLY)