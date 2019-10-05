#include "vulkan_device.h"
#include "vulkan_initializers.h"
#include "vulkan_tools.h"

#include <algorithm>
#include <assert.h>
#include <exception>

namespace vkf {
    struct VulkanDevice::Impl {
        VkPhysicalDevice physical_device;
        VkDevice logical_device;
        VkPhysicalDeviceProperties properties;
        VkPhysicalDeviceFeatures features;
        VkPhysicalDeviceFeatures enabled_features;
        VkPhysicalDeviceMemoryProperties memory_properties;
        std::vector<VkQueueFamilyProperties> queue_family_properties;
        std::vector<std::string> supported_extensions;
        VkCommandPool command_pool;
        bool enable_debug_markers;

        struct {
            uint32_t graphics;
            uint32_t compute;
            uint32_t transfer;
        } queue_family_indices;

        Impl(VkPhysicalDevice physical_device)
            : physical_device {physical_device},
              logical_device {VK_NULL_HANDLE},
              properties {},
              features {},
              enabled_features {},
              memory_properties {},
              queue_family_properties {},
              supported_extensions {},
              command_pool {VK_NULL_HANDLE},
              enable_debug_markers {false},
              queue_family_indices {0u, 0u, 0u}
        {
        }
    };

    VulkanDevice::VulkanDevice(VkPhysicalDevice physical_device)
        : _p {new Impl(physical_device)}
    {
        assert(physical_device != VK_NULL_HANDLE);

        vkGetPhysicalDeviceProperties(_p->physical_device, &_p->properties);
        vkGetPhysicalDeviceFeatures(_p->physical_device, &_p->features);
        vkGetPhysicalDeviceMemoryProperties(_p->physical_device, &_p->memory_properties);

        uint32_t queue_family_count;
        vkGetPhysicalDeviceQueueFamilyProperties(_p->physical_device, &queue_family_count, nullptr);
        assert(queue_family_count > 0u);
        _p->queue_family_properties.resize(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(_p->physical_device, &queue_family_count, _p->queue_family_properties.data());

        uint32_t extension_count;
        vkEnumerateDeviceExtensionProperties(_p->physical_device, nullptr, &extension_count, nullptr);
        if (extension_count > 0u) {
            std::vector<VkExtensionProperties> extensions(extension_count);
            VkResult result = vkEnumerateDeviceExtensionProperties(
                _p->physical_device,
                nullptr,
                &extension_count, &extensions.front());
            if (result == VK_SUCCESS) {
                for (const auto& extension : extensions) {
                    _p->supported_extensions.push_back(std::string(extension.extensionName));
                }
            }
        }
    }

    VulkanDevice::~VulkanDevice() {
        if (_p->command_pool != VK_NULL_HANDLE)
            vkDestroyCommandPool(_p->logical_device, _p->command_pool, nullptr);

        if (_p->logical_device != VK_NULL_HANDLE)
            vkDestroyDevice(_p->logical_device, nullptr);
    }


    VkPhysicalDevice VulkanDevice::GetPhysicalDevice() const {
        return _p->physical_device;
    }

    VkPhysicalDeviceFeatures VulkanDevice::GetFeatures() const {
        return _p->features;
    }

    VkPhysicalDeviceFeatures VulkanDevice::GetEnabledFeatures() const {
        return _p->enabled_features;
    }

    VkPhysicalDeviceMemoryProperties VulkanDevice::GetMemoryProperties() const {
        return _p->memory_properties;
    }

    VkDevice VulkanDevice::GetLogicalDevice() const {
        return _p->logical_device;
    }

    VkCommandPool VulkanDevice::GetDefaultCommandPool() const {
        return _p->command_pool;
    }

    bool VulkanDevice::FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties, uint32_t* memoryType) const {
        assert(memoryType != nullptr);

        for (uint32_t i = 0u; i < _p->memory_properties.memoryTypeCount; ++i) {
            if ((typeBits & 1u) != 0u) {
                if ((_p->memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
                    *memoryType = i;
                    return true;
                }
            }
            typeBits >>= 1;
        }

        *memoryType = 0u;
        return false;
    }

    bool VulkanDevice::GetQueueFamilyIndex(VkQueueFlagBits queue_flags, uint32_t* queue_index) const {
        assert(queue_index != nullptr);

        uint32_t queue_props_count = static_cast<uint32_t>(_p->queue_family_properties.size());

        if ((queue_flags & VK_QUEUE_COMPUTE_BIT) != 0u) {
            for (uint32_t i = 0u; i < queue_props_count; ++i) {
                uint32_t queue_family_flags = _p->queue_family_properties[i].queueFlags;
                if ((queue_family_flags & queue_flags) != 0u
                    && (queue_family_flags & VK_QUEUE_GRAPHICS_BIT) == 0u
                ) {
                    *queue_index = i;
                    return true;
                }
            }
        }

        if ((queue_flags & VK_QUEUE_TRANSFER_BIT) != 0u) {
            for (uint32_t i = 0u; i < queue_props_count; ++i) {
                uint32_t queue_family_flags = _p->queue_family_properties[i].queueFlags;
                if ((queue_family_flags & queue_flags) != 0u
                    && (queue_family_flags & VK_QUEUE_GRAPHICS_BIT) == 0u
                    && (queue_family_flags & VK_QUEUE_COMPUTE_BIT) == 0u
                ) {
                    *queue_index = i;
                    return true;
                }
            }
        }

        for (uint32_t i = 0u; i < queue_props_count; ++i) {
            if ((_p->queue_family_properties[i].queueFlags & queue_flags) != 0u) {
                *queue_index = i;
                return true;
            }
        }

        *queue_index = static_cast<uint32_t>(VK_NULL_HANDLE);
        return false;
    }

    bool VulkanDevice::IsExtensionSupported(const char* extension_name) const {
        auto it = std::find(_p->supported_extensions.begin(), _p->supported_extensions.end(), extension_name);
        return it != _p->supported_extensions.end();
    }

    VkResult VulkanDevice::InitLogicalDevice(VkQueueFlags queue_types) {
        return InitLogicalDevice(_p->features, {}, nullptr, true, queue_types);
    }

    VkResult VulkanDevice::InitLogicalDevice(
        VkPhysicalDeviceFeatures enabled_features,
        std::vector<const char*> enabled_extensions,
        void* next_chain,
        bool use_swapchain,
        VkQueueFlags queue_types
    ) {
        const float default_queue_priority {1.0f};

        std::vector<VkDeviceQueueCreateInfo> queue_create_infos {};

        if ((queue_types & VK_QUEUE_GRAPHICS_BIT) != 0u
            && GetQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT, &_p->queue_family_indices.graphics)
        ) {
            VkDeviceQueueCreateInfo queue_create_info {};
            queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_create_info.queueFamilyIndex = _p->queue_family_indices.graphics;
            queue_create_info.queueCount = 1u;
            queue_create_info.pQueuePriorities = &default_queue_priority;
            queue_create_infos.push_back(queue_create_info);
        }

        if ((queue_types & VK_QUEUE_COMPUTE_BIT) != 0u
            && GetQueueFamilyIndex(VK_QUEUE_COMPUTE_BIT, &_p->queue_family_indices.compute)
            && _p->queue_family_indices.compute != _p->queue_family_indices.graphics
        ) {
            VkDeviceQueueCreateInfo queue_create_info {};
            queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_create_info.queueFamilyIndex = _p->queue_family_indices.compute;
            queue_create_info.queueCount = 1u;
            queue_create_info.pQueuePriorities = &default_queue_priority;
            queue_create_infos.push_back(queue_create_info);
        }

        if ((queue_types & VK_QUEUE_TRANSFER_BIT) != 0u
            && GetQueueFamilyIndex(VK_QUEUE_TRANSFER_BIT, &_p->queue_family_indices.transfer)
            && _p->queue_family_indices.transfer != _p->queue_family_indices.compute
            && _p->queue_family_indices.transfer != _p->queue_family_indices.graphics
        ) {
            VkDeviceQueueCreateInfo queue_create_info {};
            queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_create_info.queueFamilyIndex = _p->queue_family_indices.transfer;
            queue_create_info.queueCount = 1u;
            queue_create_info.pQueuePriorities = &default_queue_priority;
            queue_create_infos.push_back(queue_create_info);
        }

        std::vector<const char*> device_extensions(enabled_extensions);
        if (use_swapchain)
            device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

        VkDeviceCreateInfo device_create_info {};
        device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
        device_create_info.pQueueCreateInfos = queue_create_infos.data();
        device_create_info.pEnabledFeatures = &enabled_features;

        VkPhysicalDeviceFeatures2 physical_device_features_2 {};
        if (next_chain != nullptr) {
            physical_device_features_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            physical_device_features_2.features = enabled_features;
            physical_device_features_2.pNext = next_chain;
            device_create_info.pEnabledFeatures = nullptr;
            device_create_info.pNext = &physical_device_features_2;
        }

        if (IsExtensionSupported(VK_EXT_DEBUG_MARKER_EXTENSION_NAME)) {
            device_extensions.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
            _p->enable_debug_markers = true;
        }

        if (device_extensions.size() > 0u) {
            device_create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
            device_create_info.ppEnabledExtensionNames = device_extensions.data();
        }

        VkResult result = vkCreateDevice(_p->physical_device, &device_create_info, nullptr, &_p->logical_device);
        if (result != VK_SUCCESS)
            return result;

        _p->enabled_features = enabled_features;
        _p->command_pool = CreateCommandPool(_p->queue_family_indices.graphics);

        return VK_SUCCESS;
    }

    VkCommandBuffer VulkanDevice::CreateCommandBuffer(VkCommandBufferLevel level, bool begin) const {
        VkCommandBufferAllocateInfo alloc_info = initializers::CommandBufferAllocateInfo(_p->command_pool, level, 1);
        VkCommandBuffer result;
        VK_CHECK_RESULT(vkAllocateCommandBuffers(_p->logical_device, &alloc_info, &result));

        if (begin) {
            auto begin_info = initializers::CommandBufferBeginInfo();
            VK_CHECK_RESULT(vkBeginCommandBuffer(result, &begin_info));
        }

        return result;
    }

    VkCommandPool VulkanDevice::CreateCommandPool(uint32_t queue_family_index, VkCommandPoolCreateFlags flags) const {
        VkCommandPoolCreateInfo create_info {};
        create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        create_info.queueFamilyIndex = queue_family_index;
        create_info.flags = flags;

        VkCommandPool result;
        VK_CHECK_RESULT(vkCreateCommandPool(_p->logical_device, &create_info, nullptr, &result));
        return result;
    }

    void VulkanDevice::FlushCommandBuffer(VkCommandBuffer command_buffer, VkQueue queue, bool free) const {
        if (command_buffer == VK_NULL_HANDLE)
            return;

        VK_CHECK_RESULT(vkEndCommandBuffer(command_buffer));

        VkSubmitInfo submit_info = initializers::SubmitInfo();
        submit_info.commandBufferCount = 1u;
        submit_info.pCommandBuffers = &command_buffer;

        VkFenceCreateInfo fence_info = initializers::FenceCreateInfo();
        VkFence fence;
        VK_CHECK_RESULT(vkCreateFence(_p->logical_device, &fence_info, nullptr, &fence));

        VK_CHECK_RESULT(vkQueueSubmit(queue, 1u, &submit_info, fence));
        VK_CHECK_RESULT(vkWaitForFences(_p->logical_device, 1u, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));

        vkDestroyFence(_p->logical_device, fence, nullptr);

        if (free)
            vkFreeCommandBuffers(_p->logical_device, _p->command_pool, 1u, &command_buffer);
    }

    void VulkanDevice::WaitIdle() const {
        vkDeviceWaitIdle(_p->logical_device);
    }

    VulkanDevice::operator VkDevice() { return _p->logical_device; }
    VulkanDevice::operator VkPhysicalDevice() { return _p->physical_device; }
}
