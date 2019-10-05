#include "vulkan_initializers.h"

namespace vkf::initializers {
    VkBufferCreateInfo BufferCreateInfo() {
        VkBufferCreateInfo result {};
        result.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        return result;
    }

    VkBufferCreateInfo BufferCreateInfo(VkBufferUsageFlags usage_flags, VkDeviceSize size) {
        VkBufferCreateInfo result {};
        result.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        result.usage = usage_flags;
        result.size = size;
        return result;
    }

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

    VkMappedMemoryRange MappedMemoryRange() {
        VkMappedMemoryRange result {};
        result.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        return result;
    }

    VkMemoryAllocateInfo MemoryAllocateInfo() {
        VkMemoryAllocateInfo result {};
        result.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        return result;
    }

    VkSubmitInfo SubmitInfo() {
        VkSubmitInfo result {};
        result.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        return result;
    }
}
