#include "vulkan_device.h"
#include "vulkan_initializers.h"
#include "vulkan_tools.h"

#include <algorithm>
#include <assert.h>
#include <cstring>
#include <exception>

namespace vkf {
    struct VulkanDevice::Impl {
        VkPhysicalDevice physical_device;
        VkDevice logical_device;
        VkPhysicalDeviceProperties properties;
        VkPhysicalDeviceFeatures features;
        VkPhysicalDeviceFeatures enabled_features;
        VkPhysicalDeviceMemoryProperties memory_properties;
        std::vector<std::string> supported_extensions;

        bool enable_debug_markers;

        std::vector<VkQueueFamilyProperties> queue_family_properties;
        struct {
            uint32_t graphics;
            uint32_t compute;
            uint32_t transfer;
        } queue_family_indices;

        VkQueue compute_queue;
        VkQueue graphics_queue;
        VkQueue transfer_queue;

        Impl(VkPhysicalDevice physical_device)
            : physical_device {physical_device},
              logical_device {VK_NULL_HANDLE},
              properties {},
              features {},
              enabled_features {},
              memory_properties {},
              supported_extensions {},
              enable_debug_markers {false},
              queue_family_properties {},
              queue_family_indices {UINT32_MAX, UINT32_MAX, UINT32_MAX},
              compute_queue {VK_NULL_HANDLE},
              graphics_queue {VK_NULL_HANDLE},
              transfer_queue {VK_NULL_HANDLE}
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

    VkQueue VulkanDevice::GetComputeQueue() const {
        return _p->compute_queue;
    }

    VkQueue VulkanDevice::GetGraphicsQueue() const {
        return _p->graphics_queue;
    }

    VkQueue VulkanDevice::GetTransferQueue() const {
        return _p->transfer_queue;
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

        uint32_t queue_props_count {static_cast<uint32_t>(_p->queue_family_properties.size())};

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

        *queue_index = UINT32_MAX;
        return false;
    }

    std::vector<VkQueueFamilyProperties> VulkanDevice::GetQueueFamilyProperties() const {
        return _p->queue_family_properties;
    }

    bool VulkanDevice::GetPresentationQueueFamilyIndex(VkSurfaceKHR surface, uint32_t* queue_index) const {
        assert(surface != VK_NULL_HANDLE);
        assert(queue_index != nullptr);

        for (uint32_t i = 0u; i < static_cast<uint32_t>(_p->queue_family_properties.size()); ++i) {
            if ((_p->queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0u)
                continue;

            VkBool32 present_support = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(_p->physical_device, i, surface, &present_support);
            if (present_support == VK_TRUE) {
                *queue_index = i;
                return true;
            }
        }

        *queue_index = 0u;
        return false;
    }

    bool VulkanDevice::IsExtensionSupported(const char* extension_name) const {
        auto it = std::find(_p->supported_extensions.begin(), _p->supported_extensions.end(), extension_name);
        return it != _p->supported_extensions.end();
    }

    VkResult VulkanDevice::InitLogicalDevice(VkQueueFlags queue_types) {
        return InitLogicalDevice(_p->features, {}, queue_types, VK_NULL_HANDLE, nullptr);
    }

    VkResult VulkanDevice::InitLogicalDevice(VkQueueFlags queue_types, VkSurfaceKHR target_surface) {
        return InitLogicalDevice(_p->features, {}, queue_types, target_surface, nullptr);
    }

    VkResult VulkanDevice::InitLogicalDevice(
        VkPhysicalDeviceFeatures enabled_features,
        std::vector<const char*> enabled_extensions,
        VkQueueFlags queue_types,
        VkSurfaceKHR target_surface,
        void* next_chain
    ) {
        const float default_queue_priority {1.0f};

        std::vector<const char*> device_extensions(enabled_extensions);
        if (target_surface != VK_NULL_HANDLE)
            device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

        uint32_t compute_queue_index {UINT32_MAX};
        uint32_t graphics_queue_index {UINT32_MAX};
        uint32_t transfer_queue_index {UINT32_MAX};
        std::vector<VkDeviceQueueCreateInfo> queue_create_infos {};

        bool have_graphics_queue {false};

        if ((queue_types & VK_QUEUE_GRAPHICS_BIT) != 0u) {
            have_graphics_queue = (target_surface != VK_NULL_HANDLE)
                ? GetPresentationQueueFamilyIndex(target_surface, &graphics_queue_index)
                : GetQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT, &graphics_queue_index);
        }

        // It's important to get the graphics queue first, if requested
        if (have_graphics_queue) {
            VkDeviceQueueCreateInfo queue_create_info {};
            queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_create_info.queueFamilyIndex = graphics_queue_index;
            queue_create_info.queueCount = 1u;
            queue_create_info.pQueuePriorities = &default_queue_priority;
            queue_create_infos.push_back(queue_create_info);
        }

        if ((queue_types & VK_QUEUE_COMPUTE_BIT) != 0u
            && GetQueueFamilyIndex(VK_QUEUE_COMPUTE_BIT, &compute_queue_index)
            && compute_queue_index != graphics_queue_index
        ) {
            VkDeviceQueueCreateInfo queue_create_info {};
            queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_create_info.queueFamilyIndex = compute_queue_index;
            queue_create_info.queueCount = 1u;
            queue_create_info.pQueuePriorities = &default_queue_priority;
            queue_create_infos.push_back(queue_create_info);
        }

        if ((queue_types & VK_QUEUE_TRANSFER_BIT) != 0u
            && GetQueueFamilyIndex(VK_QUEUE_TRANSFER_BIT, &transfer_queue_index)
            && transfer_queue_index != compute_queue_index
            && transfer_queue_index != graphics_queue_index
        ) {
            VkDeviceQueueCreateInfo queue_create_info {};
            queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_create_info.queueFamilyIndex = transfer_queue_index;
            queue_create_info.queueCount = 1u;
            queue_create_info.pQueuePriorities = &default_queue_priority;
            queue_create_infos.push_back(queue_create_info);
        }

        std::cout << "queues: " << queue_create_infos.size() << std::endl;

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

        bool enable_debug_markers {IsExtensionSupported(VK_EXT_DEBUG_MARKER_EXTENSION_NAME)};
        if (enable_debug_markers)
            device_extensions.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);

        if (device_extensions.size() > 0u) {
            device_create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
            device_create_info.ppEnabledExtensionNames = device_extensions.data();
        }

        VkDevice device;
        VkResult result = vkCreateDevice(_p->physical_device, &device_create_info, nullptr, &device);
        if (result != VK_SUCCESS)
            return result;

        _p->logical_device = device;
        _p->enable_debug_markers = enable_debug_markers;
        _p->enabled_features = enabled_features;
        _p->queue_family_indices.compute = compute_queue_index;
        _p->queue_family_indices.graphics = graphics_queue_index;
        _p->queue_family_indices.transfer = transfer_queue_index;

        if (compute_queue_index != UINT32_MAX)
            vkGetDeviceQueue(device, compute_queue_index, 0u, &_p->compute_queue);
        if (graphics_queue_index != UINT32_MAX)
            vkGetDeviceQueue(device, graphics_queue_index, 0u, &_p->graphics_queue);
        if (transfer_queue_index != UINT32_MAX)
            vkGetDeviceQueue(device, transfer_queue_index, 0u, &_p->transfer_queue);

        return VK_SUCCESS;
    }

    VkResult VulkanDevice::CreateBuffer(
        VkBufferUsageFlags usage_flags,
        VkMemoryPropertyFlags memory_property_flags,
        VkDeviceSize size,
        VkBuffer* buffer,
        VkDeviceMemory* memory,
        void* data
    ) const {
        assert(buffer != nullptr);
        assert(memory != nullptr);

        *buffer = VK_NULL_HANDLE;
        *memory = VK_NULL_HANDLE;

        VkResult result = VK_SUCCESS;
        VkBuffer result_buffer = VK_NULL_HANDLE;
        VkDeviceMemory result_memory = VK_NULL_HANDLE;

        const auto clear_result = [this, result_buffer, result_memory]() {
            if (result_buffer != VK_NULL_HANDLE)
                vkDestroyBuffer(_p->logical_device, result_buffer, nullptr);
            if (result_memory != VK_NULL_HANDLE)
                vkFreeMemory(_p->logical_device, result_memory, nullptr);
        };

        VkBufferCreateInfo create_info = initializers::BufferCreateInfo(usage_flags, size);
        create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        result = vkCreateBuffer(_p->logical_device, &create_info, nullptr, &result_buffer);
        if (result != VK_SUCCESS)
            return result;

        VkMemoryRequirements memory_requirements;
        VkMemoryAllocateInfo memory_alloc_info = initializers::MemoryAllocateInfo();
        vkGetBufferMemoryRequirements(_p->logical_device, result_buffer, &memory_requirements);
        memory_alloc_info.allocationSize = memory_requirements.size;
        if (!FindMemoryType(
            memory_requirements.memoryTypeBits,
            memory_property_flags,
            &memory_alloc_info.memoryTypeIndex
        )) {
            clear_result();
            return VK_ERROR_FORMAT_NOT_SUPPORTED;
        }
        result = vkAllocateMemory(_p->logical_device, &memory_alloc_info, nullptr, &result_memory);
        if (result != VK_SUCCESS) {
            clear_result();
            return result;
        }

        if (data != nullptr) {
            void* mapped;
            result = vkMapMemory(_p->logical_device, result_memory, 0, size, 0, &mapped);
            if (result != VK_SUCCESS) {
                clear_result();
                return result;
            }
            memcpy(mapped, data, size);
            if ((memory_property_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0u) {
                VkMappedMemoryRange mapped_range = initializers::MappedMemoryRange();
                mapped_range.memory = result_memory;
                mapped_range.offset = 0;
                mapped_range.size = size;
                result = vkFlushMappedMemoryRanges(_p->logical_device, 1u, &mapped_range);
                if (result != VK_SUCCESS) {
                    clear_result();
                    return result;
                }
            }
            vkUnmapMemory(_p->logical_device, result_memory);
        }

        result = vkBindBufferMemory(_p->logical_device, result_buffer, result_memory, 0);
        if (result != VK_SUCCESS) {
            clear_result();
            return result;
        }

        *buffer = result_buffer;
        *memory = result_memory;

        return result;
    }

    VkCommandBuffer VulkanDevice::CreateCommandBuffer(VkCommandPool command_pool, VkCommandBufferLevel level) const {
        VkCommandBufferAllocateInfo alloc_info = initializers::CommandBufferAllocateInfo(command_pool, level, 1);
        VkCommandBuffer result;
        VK_CHECK_RESULT(vkAllocateCommandBuffers(_p->logical_device, &alloc_info, &result));

        return result;
    }

    VkResult VulkanDevice::CreateCommandPool(uint32_t queue_family_index, VkCommandPool* command_pool) const {
        return CreateCommandPool(queue_family_index, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, command_pool);
    }

    VkResult VulkanDevice::CreateCommandPool(
        uint32_t queue_family_index,
        VkCommandPoolCreateFlags flags,
        VkCommandPool* command_pool
    ) const {
        assert(command_pool != nullptr);

        VkCommandPoolCreateInfo create_info {};
        create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        create_info.queueFamilyIndex = queue_family_index;
        create_info.flags = flags;

        VkCommandPool result_command_pool = VK_NULL_HANDLE;
        VkResult result = vkCreateCommandPool(_p->logical_device, &create_info, nullptr, &result_command_pool);
        *command_pool = result_command_pool;
        return result;
    }

    void VulkanDevice::FlushCommandBuffer(VkCommandBuffer command_buffer, VkQueue queue) const {
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
    }

    void VulkanDevice::WaitIdle() const {
        vkDeviceWaitIdle(_p->logical_device);
    }

    VulkanDevice::operator VkDevice() { return _p->logical_device; }
    VulkanDevice::operator VkPhysicalDevice() { return _p->physical_device; }
}
