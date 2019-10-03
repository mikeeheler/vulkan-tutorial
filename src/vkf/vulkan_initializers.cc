#include "vulkan_initializers.h"

namespace vkf::initializers {
    VkCommandBufferAllocateInfo CommandBufferAllocateInfo(
        VkCommandPool command_pool,
        VkCommandBufferLevel level,
        uint32_t buffer_count
    ) {
        VkCommandBufferAllocateInfo result {};
        result.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        result.commandPool = command_pool;
        result.level = level;
        result.commandBufferCount = buffer_count;
        return result;
    }

    VkCommandBufferBeginInfo CommandBufferBeginInfo() {
        VkCommandBufferBeginInfo result {};
        result.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        return result;
    }

    VkFenceCreateInfo FenceCreateInfo(VkFenceCreateFlags flags) {
        VkFenceCreateInfo result {};
        result.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        result.flags = flags;
        return result;
    }

    VkSubmitInfo SubmitInfo() {
        VkSubmitInfo result {};
        result.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        return result;
    }
}
