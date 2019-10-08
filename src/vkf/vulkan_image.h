#pragma once

#include <vulkan/vulkan.h>
#include <experimental/propagate_const>
#include <memory>

namespace vkf {
    class VulkanImage {
    public:
        VulkanImage(uint32_t width, uint32_t height, VkFormat format);

        operator VkImage();
        operator VkImage() const;
        operator VkImageView();
        operator VkImageView() const;
        operator VkDeviceMemory();
        operator VkDeviceMemory() const;

    private:
        struct Impl; std::experimental::propagate_const<std::unique_ptr<Impl>> _p;
    };
}
