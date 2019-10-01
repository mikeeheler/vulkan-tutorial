cmake_minimum_required (VERSION 3.0)
project (vulkan-tutorial)

set(CMAKE_CXX_STANDARD 17)
set(VULKAN_SDK_DIR "ext/vulkan-sdk-1.1.121.1/x86_64")
set(VK_LAYER_PATH "${VULKAN_SDK_DIR}/etc/vulkan/explicit_layer.d")
set(vulkan_tutorial_SOURCES
    src/hello_triangle_app
    src/main.cpp
    src/scoped_glfw_window.cpp
    src/tiny_obj_loader.cc)

find_package(glfw3 3.2)

add_definitions(-DGLFW_INCLUDE_VULKAN
                -DGLM_ENABLE_EXPERIMENTAL
                -DGLM_FORCE_DEPTH_ZERO_TO_ONE
                -DGLM_FORCE_RADIANS
                -DSTB_IMAGE_IMPLEMENTATION)

include_directories("include" "${VULKAN_SDK_DIR}/include")
link_directories("${VULKAN_SDK_DIR}/lib")

add_executable (vulkan-tutorial ${vulkan_tutorial_SOURCES})
target_link_libraries (vulkan-tutorial glfw vulkan)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/assets/textures/angel-1507747.jpg
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/textures)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/assets/models/chalet.obj
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/models)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/assets/textures/chalet.jpg
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/textures)
