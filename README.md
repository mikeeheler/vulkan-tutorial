# VulkanTutorial

Basis: https://vulkan-tutorial.com/Introduction

## Setup

* Download [Vulkan SDK 1.1.121.1](https://vulkan.lunarg.com/sdk/home#sdk/downloadConfirm/1.1.121.1/linux/vulkansdk-linux-x86_64-1.1.121.1.tar.gz)
* Extract to ext/vulkan-sdk-1.1.121.1
* Install libglfw3-dev (Ubuntu) or the equivalent for your dist

This could be ported to other platforms, but I can't be arsed.

## Compiling

    mkdir build
    cd build
    cmake ..
    make

## Generate Shaders

    glslc -fshader-stage=frag src/shaders/psmain.glsl -o build/psmain.spv
    glslc -fshader-stage=vert src/shaders/vsmain.glsl -o build/vsmain.spv

## TODO

https://vulkan-tutorial.com/en/Vertex_buffers/Staging_buffer

* Modify `QueueFamilyIndices` and `findQueueFamilies` to explicitly look for a queue family with the
  `VK_QUEUE_TRANSFER` bit, but not the `VK_QUEUE_GRAPHICS_BIT`.
* Modify `createLogicalDevice` to request a handle to the transfer queue
* Create a second command pool for command buffers that are submitted on the transfer queue family
* Change the `sharingMode` of resourecs to be `VK_SHARING_MODE_CONCURRENT` and specify both the graphics and transfer
  queue families
* Submit any transfer commands like `vkCmdCopyBuffer` (which we'll be using in this chapter) to the transfer queue
  instead of the graphics queue

https://developer.nvidia.com/vulkan-memory-management

* Refactor index and vertex buffer to share same `VkBuffer`

* All of the helper functions that submit commands so far have been set up to execute synchronously by waiting for the queue to become idle. For practical applications it is recommended to combine these operations in a single command buffer and execute them asynchronously for higher throughput, especially the transitions and copy in the `createTextureImage` function. Try to experiment with this by creating a `setupCommandBuffer` that the helper functions record commands into, and add a `flushSetupCommands` to execute the commands that have been recorded so far.


## Refactor

* Create VkInstanec
* Create VkPhysicalDevice
* Create VkDevice
* Create Queues
