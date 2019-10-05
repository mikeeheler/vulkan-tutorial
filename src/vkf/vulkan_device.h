#pragma once

#include <vulkan/vulkan.h>
#include <experimental/propagate_const>
#include <memory>
#include <string>
#include <vector>

namespace vkf {
    class VulkanDevice {
    public:
        explicit VulkanDevice(VkPhysicalDevice physical_device);
        ~VulkanDevice();

        VkPhysicalDevice GetPhysicalDevice() const;
        VkPhysicalDeviceFeatures GetFeatures() const;
        VkPhysicalDeviceFeatures GetEnabledFeatures() const;
        VkPhysicalDeviceProperties GetProperties() const;
        VkPhysicalDeviceMemoryProperties GetMemoryProperties() const;
        VkDevice GetLogicalDevice() const;
        VkCommandPool GetDefaultCommandPool() const;

        bool FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties, uint32_t* memoryType) const;
        bool GetQueueFamilyIndex(VkQueueFlagBits queue_flags, uint32_t* queue_index) const;
        bool IsExtensionSupported(const char* extension_name) const;

        VkResult InitLogicalDevice(VkQueueFlags queue_types);
        VkResult InitLogicalDevice(
            VkPhysicalDeviceFeatures enabled_features,
            std::vector<const char*> enabled_extensions,
            void* next_chain = nullptr,
            bool use_swapchain = true,
            VkQueueFlags queue_types = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT
        );

        VkCommandBuffer CreateCommandBuffer(VkCommandBufferLevel level, bool begin = false) const;
        VkCommandPool CreateCommandPool(
            uint32_t queue_family_index,
            VkCommandPoolCreateFlags flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
        ) const;
        void FlushCommandBuffer(VkCommandBuffer command_buffer, VkQueue queue, bool free = true) const;
        void WaitIdle() const;

        operator VkDevice();
        operator VkPhysicalDevice();

    private:
        struct Impl; std::experimental::propagate_const<std::unique_ptr<Impl>> _p;
    };
}
