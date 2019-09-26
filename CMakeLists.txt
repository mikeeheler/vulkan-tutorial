cmake_minimum_required (VERSION 3.0)
project (vulkan-tutorial)

set(CMAKE_CXX_STANDARD 17)
set(vulkan_tutorial_SOURCES
    src/hello_triangle_app
    src/main.cpp
    src/scoped_glfw_window.cpp)

find_package(glfw3 3.2)

add_definitions(-DGLFW_INCLUDE_VULKAN)

include_directories("${CMAKE_SOURCE_DIR}/include" "${CMAKE_SOURCE_DIR}/ext/vulkan-sdk-1.1.121.1/x86_64/include")
add_executable (vulkan-tutorial ${vulkan_tutorial_SOURCES})
target_link_libraries (vulkan-tutorial glfw)
