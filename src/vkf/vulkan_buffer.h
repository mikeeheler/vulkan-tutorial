#pragma once

#include <vulkan/vulkan.h>
#include <experimental/propagate_const>
#include <memory>

namespace vkf {
    class VulkanDevice;

    class VulkanBuffer {
    public:
        VulkanBuffer(
            VulkanDevice* device,
            VkDeviceSize size,
            VkBufferUsageFlags flags,
            VkMemoryPropertyFlags memory_properties,
            void* data);
        VulkanBuffer(VulkanBuffer&& other);
        ~VulkanBuffer();

        VkDeviceSize GetSize() const;
        VkBufferUsageFlags GetUsage() const;

        VkResult Bind(VkDeviceSize offset = 0);
        void CopyTo(void* data, VkDeviceSize size);
        void Destroy();
        VkResult Flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        VkResult Map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        void Unmap();

        operator VkBuffer();
        operator VkBuffer() const;
        operator VkDeviceMemory();
        operator VkDeviceMemory() const;

    private:
        struct Impl; std::experimental::propagate_const<std::unique_ptr<Impl>> _p;
    };
}
