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
        VkDevice GetLogicalDevice() const;

        VkPhysicalDeviceFeatures GetFeatures() const;
        VkPhysicalDeviceFeatures GetEnabledFeatures() const;
        VkPhysicalDeviceProperties GetProperties() const;
        VkPhysicalDeviceMemoryProperties GetMemoryProperties() const;

        VkQueue GetComputeQueue() const;
        VkQueue GetGraphicsQueue() const;
        VkQueue GetTransferQueue() const;

        bool FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties, uint32_t* memoryType) const;
        bool GetQueueFamilyIndex(VkQueueFlagBits queue_flags, uint32_t* queue_index) const;
        std::vector<VkQueueFamilyProperties> GetQueueFamilyProperties() const;
        bool GetPresentationQueueFamilyIndex(VkSurfaceKHR surface, uint32_t* queue_index) const;
        bool IsExtensionSupported(const char* extension_name) const;

        VkResult InitLogicalDevice(VkQueueFlags queue_types);
        VkResult InitLogicalDevice(VkQueueFlags queue_flags, VkSurfaceKHR target_surface);
        VkResult InitLogicalDevice(
            VkPhysicalDeviceFeatures enabled_features,
            std::vector<const char*> enabled_extensions,
            VkQueueFlags queue_types,
            VkSurfaceKHR target_surface,
            void* next_chain
        );

        VkResult CreateBuffer(
            VkBufferUsageFlags usage_flags,
            VkMemoryPropertyFlags memory_property_flags,
            VkDeviceSize size,
            VkBuffer* buffer,
            VkDeviceMemory* memory,
            void* data = nullptr
        ) const;
        VkResult CreateCommandPool(uint32_t queue_family_index, VkCommandPool* command_pool) const;
        VkResult CreateCommandPool(
            uint32_t queue_family_index,
            VkCommandPoolCreateFlags flags,
            VkCommandPool* command_pool
        ) const;

        VkCommandBuffer CreateCommandBuffer(VkCommandPool command_pool, VkCommandBufferLevel level) const;
        void FlushCommandBuffer(VkCommandBuffer command_buffer, VkQueue queue) const;
        void WaitIdle() const;

        operator VkDevice();
        operator VkPhysicalDevice();

    private:
        struct Impl; std::experimental::propagate_const<std::unique_ptr<Impl>> _p;
    };
}
