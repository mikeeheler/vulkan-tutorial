#include "vulkan_buffer.h"
#include "vulkan_device.h"
#include "vulkan_tools.h"

#include <assert.h>
#include <cstring>
#include <exception>

namespace vkf {
    struct VulkanBuffer::Impl {
        VkBuffer buffer;
        VkDeviceMemory memory;
        VkDevice device;
        void* mapped_memory;

        VkBufferUsageFlags usage;
        VkDeviceSize size;

        Impl() :
            buffer {VK_NULL_HANDLE},
            memory {VK_NULL_HANDLE},
            device {VK_NULL_HANDLE},
            mapped_memory {nullptr},
            usage {0u},
            size {0}
        {}
    };

    VulkanBuffer::VulkanBuffer(
        VulkanDevice* device,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags memory_properties,
        void* data
    ) : _p(new Impl()) {
        _p->device = *device;
        _p->usage = usage;
        _p->size = size;

        VK_CHECK_RESULT(device->CreateBuffer(usage, 0, size, &_p->buffer, &_p->memory, data));
    }

    VulkanBuffer::~VulkanBuffer() {
        Destroy();
    }

    VkDeviceSize VulkanBuffer::GetSize() const {
        return _p->size;
    }

    VkBufferUsageFlags VulkanBuffer::GetUsage() const {
        return _p->usage;
    }

    VkResult VulkanBuffer::Bind(VkDeviceSize offset) {
        return vkBindBufferMemory(_p->device, _p->buffer, _p->memory, offset);
    }

    void VulkanBuffer::Destroy() {
        if (_p->buffer != VK_NULL_HANDLE)
            vkDestroyBuffer(_p->device, _p->buffer, nullptr);
        if (_p->memory != VK_NULL_HANDLE)
            vkFreeMemory(_p->device, _p->memory, nullptr);

        _p->buffer = VK_NULL_HANDLE;
        _p->memory = VK_NULL_HANDLE;
    }

    void VulkanBuffer::CopyTo(void* data, VkDeviceSize size) {
        assert(_p->mapped_memory != nullptr);
        memcpy(_p->mapped_memory, data, size);
    }

    VkResult VulkanBuffer::Flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) {
        VkMappedMemoryRange mapped_range {};
        mapped_range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        mapped_range.memory = _p->memory;
        mapped_range.offset = offset;
        mapped_range.size = size;
        return vkFlushMappedMemoryRanges(_p->device, 1u, &mapped_range);
    }

    VkResult VulkanBuffer::Map(VkDeviceSize size, VkDeviceSize offset) {
        return vkMapMemory(_p->device, _p->memory, offset, size, 0u, &_p->mapped_memory);
    }

    void VulkanBuffer::Unmap() {
        if (_p->mapped_memory != nullptr) {
            vkUnmapMemory(_p->device, _p->memory);
            _p->mapped_memory = nullptr;
        }
    }

    VulkanBuffer::operator VkBuffer() {
        return _p->buffer;
    }

    VulkanBuffer::operator VkBuffer() const {
        return _p->buffer;
    }

    VulkanBuffer::operator VkDeviceMemory() {
        return _p->memory;
    }

    VulkanBuffer::operator VkDeviceMemory() const {
        return _p->memory;
    }
}
