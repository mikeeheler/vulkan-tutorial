#pragma once

#include <vulkan/vulkan.h>
#include <assert.h>
#include <cstdint>
#include <iostream>

#define VK_CHECK_RESULT(f) { \
    VkResult res = (f); \
    if (res != VK_SUCCESS) { \
        std::cerr << "Fatal : VkResult in " << __FILE__ << " at line " << __LINE__ << std::endl; \
        assert(res == VK_SUCCESS); \
    } \
}

#define USE_VALIDATION_LAYERS 1

#define DEFAULT_FENCE_TIMEOUT UINT64_MAX
#define VK_FLAGS_NONE 0
