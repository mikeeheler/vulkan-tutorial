#pragma once

#include <vulkan/vulkan.h>
#include "vulkan_tools.h"

namespace vkf::initializers {
    VkCommandBufferAllocateInfo CommandBufferAllocateInfo(
        VkCommandPool command_pool,
        VkCommandBufferLevel level,
        uint32_t buffer_count
    );

    VkCommandBufferBeginInfo CommandBufferBeginInfo();

    VkFenceCreateInfo FenceCreateInfo(VkFenceCreateFlags flags = VK_FLAGS_NONE);

    VkSubmitInfo SubmitInfo();
}
