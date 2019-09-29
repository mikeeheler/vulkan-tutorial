cmake_minimum_required (VERSION 3.0)
project (vulkan-tutorial)

set(CMAKE_CXX_STANDARD 17)
set(VULKAN_SDK_DIR "ext/vulkan-sdk-1.1.121.1/x86_64")
set(VK_LAYER_PATH "${VULKAN_SDK_DIR}/etc/vulkan/explicit_layer.d")
set(vulkan_tutorial_SOURCES
    src/hello_triangle_app
    src/main.cpp
    src/scoped_glfw_window.cpp)

find_package(glfw3 3.2)

add_definitions(-DGLFW_INCLUDE_VULKAN -DGLM_FORCE_RADIANS -DSTB_IMAGE_IMPLEMENTATION)

include_directories("include" "${VULKAN_SDK_DIR}/include")
link_directories("${VULKAN_SDK_DIR}/lib")

add_executable (vulkan-tutorial ${vulkan_tutorial_SOURCES})
target_link_libraries (vulkan-tutorial glfw vulkan)
