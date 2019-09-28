# VulkanTutorial

Basis: https://vulkan-tutorial.com/Introduction

I tend to do things my own way, so it's not exactly 1:1. But it works.

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
