#include "vulkan_image.h"

namespace vkf {
    struct VulkanImage::Impl {
        VkImage image;
        VkImageView view;
        VkDeviceMemory memory;
        uint32_t width;
        uint32_t height;

        Impl() :
            image {VK_NULL_HANDLE},
            view {VK_NULL_HANDLE},
            memory {VK_NULL_HANDLE},
            width {0u},
            height {0u}
        {}
    };

    VulkanImage::VulkanImage(uint32_t width, uint32_t height, VkFormat format)
        : _p {new Impl()}
    {
    }

    VulkanImage::operator VkImage() {
        return _p->image;
    }

    VulkanImage::operator VkImage() const {
        return _p->image;
    }

    VulkanImage::operator VkImageView() {
        return _p->view;
    }

    VulkanImage::operator VkImageView() const {
        return _p->view;
    }

    VulkanImage::operator VkDeviceMemory() {
        return _p->memory;
    }

    VulkanImage::operator VkDeviceMemory() const {
        return _p->memory;
    }
}
