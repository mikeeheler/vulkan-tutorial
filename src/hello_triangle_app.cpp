#include "hello_triangle_app.h"
#include "scoped_glfw_window.h"
#include "vkf/vulkan_device.h"
#include "vkf/vulkan_initializers.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>
#include <stb/stb_image.h>
#include <tiny_obj_loader.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace std {
    template<> struct hash<vulkan_tutorial::vertex> {
        size_t operator()(vulkan_tutorial::vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos) ^
                (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
                (hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
}

namespace {
    VkApplicationInfo create_vk_application_info() {
        VkApplicationInfo appInfo = {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;
        return appInfo;
    }

    VkResult CreateDebugUtilsMessengerEXT(
        VkInstance instance,
        const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkDebugUtilsMessengerEXT* pDebugMessenger
    ) {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if (func == nullptr)
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }

    void DestroyDebugUtilsMessengerEXT(
        VkInstance instance,
        VkDebugUtilsMessengerEXT debugMessenger,
        const VkAllocationCallbacks* pAllocator
    ) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func == nullptr)
            return;
        func(instance, debugMessenger, pAllocator);
    }

    const char* getPresentModeName(VkPresentModeKHR presentMode) {
        #define PRESENT_CASE(x) case VK_PRESENT_MODE_ ## x ## _KHR: return "VK_PRESENT_MODE_" #x "_KHR"
        switch (presentMode) {
            PRESENT_CASE(IMMEDIATE);
            PRESENT_CASE(MAILBOX);
            PRESENT_CASE(FIFO);
            PRESENT_CASE(FIFO_RELAXED);
            PRESENT_CASE(SHARED_DEMAND_REFRESH);
            PRESENT_CASE(SHARED_CONTINUOUS_REFRESH);
        }
        return "Unknown Present Mode";
    }

    std::string getVersionString(uint32_t version) {
        std::stringstream out;
        out << VK_VERSION_MAJOR(version) << '.'
            << VK_VERSION_MINOR(version) << '.'
            << VK_VERSION_PATCH(version);
        return out.str();
    }

    void print_device_extensions(VkPhysicalDevice physicalDevice) {
        uint32_t extensionCount = 0u;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> extensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, extensions.data());

        std::cout << "available device extensions:" << std::endl;
        for (const auto& extension : extensions) {
            std::cout << "  " << extension.extensionName << std::endl;
        }
    }

    void print_instance_extensions() {
        uint32_t extensionCount = 0u;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> extensions(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

        std::cout << "available instance extensions:" << std::endl;
        for (const auto& extension : extensions) {
            std::cout << "  " << extension.extensionName << std::endl;
        }
    }

    std::vector<char> readFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        size_t fileSize = (size_t) file.tellg();
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();

        return buffer;
    }
}

namespace vulkan_tutorial {
    struct hello_triangle_app::Impl {
        std::unique_ptr<vkf::VulkanDevice> device;
    };

    std::array<VkVertexInputAttributeDescription, 3> vertex::getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions = {};

        attributeDescriptions[0].binding = 0u;
        attributeDescriptions[0].location = 0u;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(vertex, pos);

        attributeDescriptions[1].binding = 0u;
        attributeDescriptions[1].location = 1u;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(vertex, color);

        attributeDescriptions[2].binding = 0u;
        attributeDescriptions[2].location = 2u;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(vertex, texCoord);

        return attributeDescriptions;
    }

    VkVertexInputBindingDescription vertex::getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription = {};
        bindingDescription.binding = 0u;
        bindingDescription.stride = sizeof(vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    hello_triangle_app::hello_triangle_app()
      : _p {new Impl},
        _colorImage {VK_NULL_HANDLE},
        _colorImageMemory {VK_NULL_HANDLE},
        _colorImageView {VK_NULL_HANDLE},
        _commandBuffers {},
        _currentFrame(0u),
        _debugMessenger {nullptr},
        _depthImage {VK_NULL_HANDLE},
        _depthImageMemory {VK_NULL_HANDLE},
        _depthImageView {VK_NULL_HANDLE},
        _descriptorPool {VK_NULL_HANDLE},
        _descriptorSetLayout {VK_NULL_HANDLE},
        _descriptorSets {},
        _framebufferResized {false},
        _fullscreenToggleRequested {false},
        _graphicsPipeline {VK_NULL_HANDLE},
        _graphicsQueue {VK_NULL_HANDLE},
        _imageAvailableSemaphores {},
        _indexBuffer {VK_NULL_HANDLE},
        _indexBufferMemory {VK_NULL_HANDLE},
        _indices {},
        _inFlightFences {},
        _instance {VK_NULL_HANDLE},
        _instanceExtensions {
            VK_KHR_DEVICE_GROUP_CREATION_EXTENSION_NAME,
            VK_KHR_DISPLAY_EXTENSION_NAME,
            VK_KHR_GET_DISPLAY_PROPERTIES_2_EXTENSION_NAME,
            VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
            VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME
        },
        _mipLevels {1u},
        _msaaSamples {VK_SAMPLE_COUNT_1_BIT},
        _pipelineLayout {VK_NULL_HANDLE},
        _presentQueue {VK_NULL_HANDLE},
        _renderFinishedSemaphores {},
        _renderPass {VK_NULL_HANDLE},
        _surface {VK_NULL_HANDLE},
        _swapchain {VK_NULL_HANDLE},
        _swapchainExtent {0u, 0u},
        _swapchainFramebuffers {},
        _swapchainImageFormat {VK_FORMAT_UNDEFINED},
        _swapchainImages {},
        _swapchainImageViews {},
        _textureImage {VK_NULL_HANDLE},
        _textureImageMemory {VK_NULL_HANDLE},
        _textureImageView {VK_NULL_HANDLE},
        _textureSampler {VK_NULL_HANDLE},
        _uniformBuffers {},
        _uniformBuffersMemory {},
        _validationLayers {
#if ENABLE_VALIDATION_LAYERS
            "VK_LAYER_KHRONOS_validation"
#endif
        },
        _vertexBuffer {VK_NULL_HANDLE},
        _vertexBufferMemory {VK_NULL_HANDLE},
        _vertices {},
        _window {}
    {}

    hello_triangle_app::~hello_triangle_app() {
        cleanup();
    }

    void hello_triangle_app::run() {
        initWindow();
        initInputHandlers();
        initVulkan();
        mainLoop();
        cleanup();
    }

    VkCommandBuffer hello_triangle_app::beginSingleTimeCommands() {
        VkCommandBuffer commandBuffer = _p->device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        VkCommandBufferBeginInfo beginInfo = vkf::initializers::CommandBufferBeginInfo();
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        return commandBuffer;
    }

    bool hello_triangle_app::checkDeviceExtensionsSupport(VkPhysicalDevice device) const {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        // TODO check against list of required extensions from.. somewhere
        return true;
    }

    bool hello_triangle_app::checkValidationLayerSupport() const {
        if (_validationLayers.size() == 0)
            return true;

        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        if (layerCount == 0)
            return false;

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        std::cout << "validation layers:" << std::endl;
        for (const auto& layerProperties : availableLayers) {
            std::cout << "  " << layerProperties.layerName << std::endl;
        }

        for (const char* layerName : _validationLayers) {
            bool layerFound = false;

            for (const auto& layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound) {
                return false;
            }
        }

        return true;
    }

    VkExtent2D hello_triangle_app::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const {
        if (capabilities.currentExtent.width != UINT32_MAX) {
            return capabilities.currentExtent;
        }

        int width, height;
        glfwGetFramebufferSize(const_cast<GLFWwindow*>(_window.get()), &width, &height);

        VkExtent2D actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };

        actualExtent.width = std::max(
            capabilities.minImageExtent.width,
            std::min(capabilities.maxImageExtent.width, actualExtent.width)
        );
        actualExtent.height = std::max(
            capabilities.minImageExtent.height,
            std::min(capabilities.maxImageExtent.height, actualExtent.height)
        );

        return actualExtent;
    }

    VkPresentModeKHR hello_triangle_app::chooseSwapPresentMode(
        const std::vector<VkPresentModeKHR>& availablePresentModes
    ) const {
        std::vector<VkPresentModeKHR> sortedModes(availablePresentModes.begin(), availablePresentModes.end());
        std::sort(sortedModes.begin(), sortedModes.end());

        std::vector<VkPresentModeKHR> preferredModes {
            VK_PRESENT_MODE_MAILBOX_KHR,
            VK_PRESENT_MODE_IMMEDIATE_KHR,
            VK_PRESENT_MODE_FIFO_RELAXED_KHR,
            VK_PRESENT_MODE_FIFO_KHR
        };

        for (const auto& preferredMode : preferredModes) {
            if (std::binary_search(sortedModes.begin(), sortedModes.end(), preferredMode)) {
                std::cout << "swapchain: selecting present mode: " << getPresentModeName(preferredMode) << std::endl;
                return preferredMode;
            }
        }

        throw std::runtime_error("no available present modes");
    }

    VkSurfaceFormatKHR hello_triangle_app::chooseSwapSurfaceFormat(
        const std::vector<VkSurfaceFormatKHR>& availableFormats
    ) const {
        if (availableFormats.size() == 0) {
            throw std::runtime_error("no surface formats available for swapchain");
        }

        for (const auto& availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8_UNORM
                && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
            ) {
                return availableFormat;
            }
        }

        return availableFormats[0];
    }

    void hello_triangle_app::cleanup() {
        if (_instance == VK_NULL_HANDLE)
            return;

        cleanupSwapchain();

        auto device = _p->device->GetLogicalDevice();

        vkDestroySampler(device, _textureSampler, nullptr);
        vkDestroyImageView(device, _textureImageView, nullptr);
        vkDestroyImage(device, _textureImage, nullptr);
        vkFreeMemory(device, _textureImageMemory, nullptr);
        vkDestroyDescriptorSetLayout(device, _descriptorSetLayout, nullptr);
        vkDestroyBuffer(device, _indexBuffer, nullptr);
        vkFreeMemory(device, _indexBufferMemory, nullptr);
        vkDestroyBuffer(device, _vertexBuffer, nullptr);
        vkFreeMemory(device, _vertexBufferMemory, nullptr);
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            vkDestroySemaphore(device, _imageAvailableSemaphores[i], nullptr);
            vkDestroySemaphore(device, _renderFinishedSemaphores[i], nullptr);
            vkDestroyFence(device, _inFlightFences[i], nullptr);
        }
        _p->device.reset(nullptr);
#if ENABLE_VALIDATION_LAYERS
        DestroyDebugUtilsMessengerEXT(_instance, _debugMessenger, nullptr);
#endif
        vkDestroySurfaceKHR(_instance, _surface, nullptr);
        vkDestroyInstance(_instance, nullptr);
        _window.destroy();
        glfwTerminate();

        _debugMessenger = VK_NULL_HANDLE;
        _descriptorSetLayout = VK_NULL_HANDLE;
        _imageAvailableSemaphores.clear();
        _indexBuffer = VK_NULL_HANDLE;
        _indexBufferMemory = VK_NULL_HANDLE;
        _inFlightFences.clear();
        _instance = VK_NULL_HANDLE;
        _graphicsQueue = VK_NULL_HANDLE;
        _mipLevels = 1u;
        _msaaSamples = VK_SAMPLE_COUNT_1_BIT;
        _renderFinishedSemaphores.clear();
        _surface = VK_NULL_HANDLE;
        _textureImage = VK_NULL_HANDLE;
        _textureImageMemory = VK_NULL_HANDLE;
        _textureImageView = VK_NULL_HANDLE;
        _textureSampler = VK_NULL_HANDLE;
        _vertexBuffer = VK_NULL_HANDLE;
        _vertexBufferMemory = VK_NULL_HANDLE;
        _window = scoped_glfw_window();
    }

    void hello_triangle_app::cleanupSwapchain() {
        auto device = _p->device->GetLogicalDevice();

        vkDestroyImageView(device, _colorImageView, nullptr);
        vkDestroyImage(device, _colorImage, nullptr);
        vkFreeMemory(device, _colorImageMemory, nullptr);
        vkDestroyImageView(device, _depthImageView, nullptr);
        vkDestroyImage(device, _depthImage, nullptr);
        vkFreeMemory(device, _depthImageMemory, nullptr);
        for (auto framebuffer : _swapchainFramebuffers) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }
        vkFreeCommandBuffers(
            device,
            _p->device->GetDefaultCommandPool(),
            static_cast<uint32_t>(_commandBuffers.size()),
            _commandBuffers.data()
        );
        vkDestroyPipeline(device, _graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(device, _pipelineLayout, nullptr);
        vkDestroyRenderPass(device, _renderPass, nullptr);
        for (const auto& imageView: _swapchainImageViews) {
            vkDestroyImageView(device, imageView, nullptr);
        }
        vkDestroySwapchainKHR(device, _swapchain, nullptr);
        for (size_t i = 0; i < _swapchainImages.size(); ++i) {
            vkDestroyBuffer(device, _uniformBuffers[i], nullptr);
            vkFreeMemory(device, _uniformBuffersMemory[i], nullptr);
        }
        vkDestroyDescriptorPool(device, _descriptorPool, nullptr);

        _commandBuffers.clear();
        _descriptorPool = VK_NULL_HANDLE;
        _fullscreenToggleRequested = false;
        _framebufferResized = false;
        _graphicsPipeline = VK_NULL_HANDLE;
        _pipelineLayout = VK_NULL_HANDLE;
        _renderPass = VK_NULL_HANDLE;
        _swapchain = VK_NULL_HANDLE;
        _swapchainFramebuffers.clear();
        _swapchainImageViews.clear();
        _uniformBuffers.clear();
        _uniformBuffersMemory.clear();
    }

    void hello_triangle_app::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkBufferCopy copyRegion = {};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1u, &copyRegion);

        endSingleTimeCommands(commandBuffer);
    }

    void hello_triangle_app::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkBufferImageCopy region = {};
        region.bufferOffset = 0;
        region.bufferRowLength = 0u;
        region.bufferImageHeight = 0u;

        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0u;
        region.imageSubresource.baseArrayLayer = 0u;
        region.imageSubresource.layerCount = 1u;

        region.imageOffset = {0, 0, 0};
        region.imageExtent = {width, height, 1u};

        vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &region);

        endSingleTimeCommands(commandBuffer);
    }

    void hello_triangle_app::createBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkBuffer& buffer,
        VkDeviceMemory& bufferMemory
    ) {
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkResult result = vkCreateBuffer(*_p->device, &bufferInfo, nullptr, &buffer);
        if (result != VK_SUCCESS)
            throw std::runtime_error("failed to create vertex buffer");

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(*_p->device, buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        if (!_p->device->FindMemoryType(memRequirements.memoryTypeBits, properties, &allocInfo.memoryTypeIndex))
            throw std::runtime_error("unable to find compatible memory type");

        result = vkAllocateMemory(*_p->device, &allocInfo, nullptr, &bufferMemory);
        if (result != VK_SUCCESS)
            throw std::runtime_error("failed to allocate vertex buffer memory");

        vkBindBufferMemory(*_p->device, buffer, bufferMemory, 0);
    }

    void hello_triangle_app::createColorResources() {
        VkFormat colorFormat = _swapchainImageFormat;

        createImage(
            _swapchainExtent.width,
            _swapchainExtent.height,
            1u,
            _msaaSamples,
            colorFormat,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            _colorImage,
            _colorImageMemory
        );
        _colorImageView = createImageView(_colorImage, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1u);

        transitionImageLayout(
            _colorImage,
            colorFormat,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            1u
        );
    }

    void hello_triangle_app::createCommandBuffers() {
        _commandBuffers.resize(_swapchainFramebuffers.size());

        for (size_t i = 0; i < _commandBuffers.size(); ++i) {
            auto commandBuffer = _p->device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);

            VkCommandBufferBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = 0u;
            beginInfo.pInheritanceInfo = nullptr;

            VkResult beginResult = vkBeginCommandBuffer(commandBuffer, &beginInfo);
            if (beginResult != VK_SUCCESS)
                throw std::runtime_error("failed to begin recording command buffer");

            std::array<VkClearValue, 2> clearValues = {};
            clearValues[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
            clearValues[1].depthStencil = {1.0f, 0u};

            VkRenderPassBeginInfo renderPassInfo = {};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = _renderPass;
            renderPassInfo.framebuffer = _swapchainFramebuffers[i];
            renderPassInfo.renderArea.offset = { 0, 0 };
            renderPassInfo.renderArea.extent = _swapchainExtent;
            renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
            renderPassInfo.pClearValues = clearValues.data();

            vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline);
            VkBuffer vertexBuffers[] = {_vertexBuffer};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0u, 1u, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(commandBuffer, _indexBuffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdBindDescriptorSets(
                commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelineLayout,
                0u, 1u, &_descriptorSets[i],
                0u, nullptr);
            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(_indices.size()), 1u, 0u, 0u, 0u);
            vkCmdEndRenderPass(commandBuffer);

            VkResult endResult = vkEndCommandBuffer(commandBuffer);
            if (endResult != VK_SUCCESS)
                throw std::runtime_error("failed to record command buffer");

            _commandBuffers[i] = commandBuffer;
        }
    }

    void hello_triangle_app::createDepthResources() {
        VkFormat depthFormat = findDepthFormat();
        if (depthFormat == VK_FORMAT_UNDEFINED)
            throw std::runtime_error("unable to find a compatible depth/stencil format");

        createImage(
            _swapchainExtent.width,
            _swapchainExtent.height,
            1u,
            _msaaSamples,
            depthFormat,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            _depthImage,
            _depthImageMemory
        );
        _depthImageView = createImageView(_depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1u);

        transitionImageLayout(
            _depthImage,
            depthFormat,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            1u
        );
    }

    void hello_triangle_app::createDescriptorPool() {
        std::array<VkDescriptorPoolSize, 2> poolSizes = {};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = static_cast<uint32_t>(_swapchainImages.size());
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = static_cast<uint32_t>(_swapchainImages.size());

        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = static_cast<uint32_t>(_swapchainImages.size());

        VkResult result = vkCreateDescriptorPool(*_p->device, &poolInfo, nullptr, &_descriptorPool);
        if (result != VK_SUCCESS)
            throw std::runtime_error("failed to create descriptor pool");
    }

    void hello_triangle_app::createDescriptorSetLayout() {
        VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
        samplerLayoutBinding.binding = 1u;
        samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding.descriptorCount = 1u;
        samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        samplerLayoutBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutBinding uboLayoutBinding = {};
        uboLayoutBinding.binding = 0u;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.descriptorCount = 1u;
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        uboLayoutBinding.pImmutableSamplers = nullptr;

        std::array<VkDescriptorSetLayoutBinding, 2> bindings = { uboLayoutBinding, samplerLayoutBinding };

        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        VkResult result = vkCreateDescriptorSetLayout(*_p->device, &layoutInfo, nullptr, &_descriptorSetLayout);
        if (result != VK_SUCCESS)
            throw std::runtime_error("failed to create descriptor set layout");
    }

    void hello_triangle_app::createDescriptorSets() {
        std::vector<VkDescriptorSetLayout> layouts(_swapchainImages.size(), _descriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = _descriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(_swapchainImages.size());
        allocInfo.pSetLayouts = layouts.data();

        _descriptorSets.resize(_swapchainImages.size());
        VkResult result = vkAllocateDescriptorSets(*_p->device, &allocInfo, _descriptorSets.data());
        if (result != VK_SUCCESS)
            throw std::runtime_error("failed to allocate descriptor");

        for (size_t i = 0; i < _swapchainImages.size(); ++i) {
            VkDescriptorBufferInfo bufferInfo = {};
            bufferInfo.buffer = _uniformBuffers[i];
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(uniform_buffer_object);

            VkDescriptorImageInfo imageInfo = {};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = _textureImageView;
            imageInfo.sampler = _textureSampler;

            std::array<VkWriteDescriptorSet, 2> descriptorWrites = {};

            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = _descriptorSets[i];
            descriptorWrites[0].dstBinding = 0u;
            descriptorWrites[0].dstArrayElement = 0u;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites[0].descriptorCount = 1u;
            descriptorWrites[0].pBufferInfo = &bufferInfo;
            descriptorWrites[0].pImageInfo = nullptr;
            descriptorWrites[0].pTexelBufferView = nullptr;

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = _descriptorSets[i];
            descriptorWrites[1].dstBinding = 1u;
            descriptorWrites[1].dstArrayElement = 0u;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[1].descriptorCount = 1u;
            descriptorWrites[1].pBufferInfo = nullptr;
            descriptorWrites[1].pImageInfo = &imageInfo;
            descriptorWrites[1].pTexelBufferView = nullptr;

            vkUpdateDescriptorSets(
                *_p->device,
                static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(),
                0u, nullptr);
        }
    }

    void hello_triangle_app::createFramebuffers() {
        _swapchainFramebuffers.resize(_swapchainImageViews.size());

        for (size_t i = 0; i < _swapchainImageViews.size(); ++i) {
            std::array<VkImageView, 3> attachments = {
                _colorImageView,
                _depthImageView,
                _swapchainImageViews[i]
            };

            VkFramebufferCreateInfo framebufferInfo = {};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = _renderPass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = _swapchainExtent.width;
            framebufferInfo.height = _swapchainExtent.height;
            framebufferInfo.layers = 1u;

            VkResult result = vkCreateFramebuffer(*_p->device, &framebufferInfo, nullptr, &_swapchainFramebuffers[i]);
            if (result != VK_SUCCESS)
                throw std::runtime_error("failed to create framebuffer");
        }
    }

    void hello_triangle_app::createGraphicsPipeline() {
        auto fragShaderCode = readFile("psmain.spv");
        std::cout << "read psmain.spv (" << fragShaderCode.size() << " bytes)" << std::endl;
        auto vertShaderCode = readFile("vsmain.spv");
        std::cout << "read vsmain.spv (" << vertShaderCode.size() << " bytes)" << std::endl;

        VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);
        VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
        VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

        auto attributeDescriptions = vertex::getAttributeDescriptions();
        auto bindingDescription = vertex::getBindingDescription();

        VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1u;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float) _swapchainExtent.width;
        viewport.height = (float) _swapchainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0;

        VkRect2D scissor = {};
        scissor.offset = { 0, 0 };
        scissor.extent = _swapchainExtent;

        VkPipelineViewportStateCreateInfo viewportState = {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1u;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1u;
        viewportState.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterizer = {};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f;
        rasterizer.depthBiasClamp = 0.0f;
        rasterizer.depthBiasSlopeFactor = 0.0f;

        VkPipelineMultisampleStateCreateInfo multisampling = {};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = _msaaSamples;
        multisampling.sampleShadingEnable = VK_TRUE;
        multisampling.minSampleShading = 0.2f;
        multisampling.pSampleMask = nullptr;
        multisampling.alphaToCoverageEnable = VK_FALSE;
        multisampling.alphaToOneEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT
            | VK_COLOR_COMPONENT_G_BIT
            | VK_COLOR_COMPONENT_B_BIT
            | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo colorBlending = {};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f;
        colorBlending.blendConstants[1] = 0.0f;
        colorBlending.blendConstants[2] = 0.0f;
        colorBlending.blendConstants[3] = 0.0f;

        VkPipelineDepthStencilStateCreateInfo depthStencil = {};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.minDepthBounds = 0.0f;
        depthStencil.maxDepthBounds = 1.0f;
        depthStencil.stencilTestEnable = VK_FALSE;
        depthStencil.front = {};
        depthStencil.back = {};

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1u;
        pipelineLayoutInfo.pSetLayouts = &_descriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 0u;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;

        VkResult result = vkCreatePipelineLayout(*_p->device, &pipelineLayoutInfo, nullptr, &_pipelineLayout);
        if (result != VK_SUCCESS)
            throw std::runtime_error("failed to create pipeline layout!");

        VkGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2u;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = nullptr;
        pipelineInfo.layout = _pipelineLayout;
        pipelineInfo.renderPass = _renderPass;
        pipelineInfo.subpass = 0u;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = -1;

        result = vkCreateGraphicsPipelines(*_p->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_graphicsPipeline);
        if (result != VK_SUCCESS)
            throw std::runtime_error("failed to create graphics pipeline");

        vkDestroyShaderModule(*_p->device, vertShaderModule, nullptr);
        vkDestroyShaderModule(*_p->device, fragShaderModule, nullptr);
    }

    void hello_triangle_app::createImage(
        uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format,
        VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
        VkImage& image, VkDeviceMemory& imageMemory
    ) {
        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1u;
        imageInfo.mipLevels = mipLevels;
        imageInfo.arrayLayers = 1u;
        imageInfo.format = format;
        imageInfo.tiling = tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.samples = numSamples;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.flags = 0;

        VkResult result = vkCreateImage(*_p->device, &imageInfo, nullptr, &image);
        if (result != VK_SUCCESS)
            throw std::runtime_error("failed to create image");

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(*_p->device, image, &memRequirements);

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        if (!_p->device->FindMemoryType(memRequirements.memoryTypeBits, properties, &allocInfo.memoryTypeIndex))
            throw std::runtime_error("unable to find compatible memory type");

        result = vkAllocateMemory(*_p->device, &allocInfo, nullptr, &imageMemory);
        if (result != VK_SUCCESS)
            throw std::runtime_error("failed to allocate image memory");

        vkBindImageMemory(*_p->device, image, imageMemory, 0);
    }

    VkImageView hello_triangle_app::createImageView(
        VkImage image,
        VkFormat format,
        VkImageAspectFlags aspectFlags,
        uint32_t mipLevels
    ) {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = aspectFlags;
        viewInfo.subresourceRange.baseMipLevel = 0u;
        viewInfo.subresourceRange.levelCount = mipLevels;
        viewInfo.subresourceRange.baseArrayLayer = 0u;
        viewInfo.subresourceRange.layerCount = 1u;

        VkImageView imageView;
        VkResult result = vkCreateImageView(*_p->device, &viewInfo, nullptr, &imageView);
        if (result != VK_SUCCESS)
            throw std::runtime_error("failed to create texture image view");

        return imageView;
    }

    void hello_triangle_app::createImageViews() {
        _swapchainImageViews.resize(_swapchainImages.size());

        for (size_t i = 0; i < _swapchainImages.size(); ++i) {
            std::cout << "create view for swapchain image #" << i << std::endl;
            _swapchainImageViews[i] = createImageView(
                _swapchainImages[i],
                _swapchainImageFormat,
                VK_IMAGE_ASPECT_COLOR_BIT,
                1u
            );
        }
    }

    void hello_triangle_app::createIndexBuffer() {
        VkDeviceSize bufferSize = sizeof(_indices[0]) * _indices.size();

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(
            bufferSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingBuffer,
            stagingBufferMemory
        );

        void* data;
        vkMapMemory(*_p->device, stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, _indices.data(), static_cast<size_t>(bufferSize));
        vkUnmapMemory(*_p->device, stagingBufferMemory);

        createBuffer(
            bufferSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            _indexBuffer,
            _indexBufferMemory
        );

        copyBuffer(stagingBuffer, _indexBuffer, bufferSize);

        vkDestroyBuffer(*_p->device, stagingBuffer, nullptr);
        vkFreeMemory(*_p->device, stagingBufferMemory, nullptr);
    }

    void hello_triangle_app::createInstance() {
#if ENABLE_VALIDATION_LAYERS
        if (!checkValidationLayerSupport()) {
            throw std::runtime_error("validation layers requested, but not available!");
        }
#endif

        auto appInfo = create_vk_application_info();
        auto extensions = getRequiredExtensions();

        std::cout << "required instance extensions:" << std::endl;
        for (const auto& extension : extensions) {
            std::cout << "  " << extension << std::endl;
        }

        print_instance_extensions();

        VkInstanceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

#if ENABLE_VALIDATION_LAYERS
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
        createInfo.enabledLayerCount = static_cast<uint32_t>(_validationLayers.size());
        createInfo.ppEnabledLayerNames = _validationLayers.data();

        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
#else
        createInfo.enabledLayerCount = 0u;
        createInfo.pNext = nullptr;
#endif

        VkResult result = vkCreateInstance(&createInfo, nullptr, &_instance);
        if (result != VK_SUCCESS)
            throw std::runtime_error("Failed to create VkInstance");
    }

    void hello_triangle_app::createLogicalDevice() {
        VK_CHECK_RESULT(_p->device->InitLogicalDevice(VK_QUEUE_GRAPHICS_BIT));

        uint32_t graphics_queue_index;
        if (!_p->device->GetQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT, &graphics_queue_index))
            throw std::runtime_error("no graphics queue present");

        vkGetDeviceQueue(*_p->device, graphics_queue_index, 0u, &_graphicsQueue);
        vkGetDeviceQueue(*_p->device, graphics_queue_index, 0u, &_presentQueue);
    }

    void hello_triangle_app::createRenderPass() {
        VkFormat depthFormat = findDepthFormat();
        if (depthFormat == VK_FORMAT_UNDEFINED)
            throw std::runtime_error("unable to find compatible depth/stencil format");

        VkAttachmentDescription colorAttachment = {};
        colorAttachment.format = _swapchainImageFormat;
        colorAttachment.samples = _msaaSamples;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorAttachmentRef = {};
        colorAttachmentRef.attachment = 0u;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription depthAttachment = {};
        depthAttachment.format = depthFormat;
        depthAttachment.samples = _msaaSamples;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthAttachmentRef = {};
        depthAttachmentRef.attachment = 1u;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription colorAttachmentResolve = {};
        colorAttachmentResolve.format = _swapchainImageFormat;
        colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachmentResolveRef = {};
        colorAttachmentResolveRef.attachment = 2u;
        colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1u;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;
        subpass.pResolveAttachments = &colorAttachmentResolveRef;

        VkSubpassDependency dependency = {};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0u;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0u;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
            | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        std::array<VkAttachmentDescription, 3> attachments = {
            colorAttachment,
            depthAttachment,
            colorAttachmentResolve
        };
        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1u;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1u;
        renderPassInfo.pDependencies = &dependency;

        VK_CHECK_RESULT(vkCreateRenderPass(*_p->device, &renderPassInfo, nullptr, &_renderPass));
    }

    void hello_triangle_app::createSyncObjects() {
        _imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        _renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        _inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo = {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            VK_CHECK_RESULT(vkCreateSemaphore(*_p->device, &semaphoreInfo, nullptr, &_imageAvailableSemaphores[i]));
            VK_CHECK_RESULT(vkCreateSemaphore(*_p->device, &semaphoreInfo, nullptr, &_renderFinishedSemaphores[i]));
            VK_CHECK_RESULT(vkCreateFence(*_p->device, &fenceInfo, nullptr, &_inFlightFences[i]));
        }
    }

    VkShaderModule hello_triangle_app::createShaderModule(const std::vector<char>& code) {
        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shaderModule;
        VK_CHECK_RESULT(vkCreateShaderModule(*_p->device, &createInfo, nullptr, &shaderModule));

        return shaderModule;
    }

    void hello_triangle_app::createSurface() {
        VK_CHECK_RESULT(glfwCreateWindowSurface(_instance, _window.get(), nullptr, &_surface));
    }

    void hello_triangle_app::createSwapchain() {
        swap_chain_support_details swapchainSupport = querySwapchainSupport(*_p->device);

        VkExtent2D extent = chooseSwapExtent(swapchainSupport.capabilities);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapchainSupport.presentModes);
        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapchainSupport.formats);

        // uint32_t imageCount = swapchainSupport.capabilities.maxImageCount;
        uint32_t imageCount = swapchainSupport.capabilities.minImageCount;
        if (swapchainSupport.capabilities.maxImageCount > 0u
            && imageCount > swapchainSupport.capabilities.maxImageCount
        ) {
            imageCount = swapchainSupport.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = _surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1u;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0u;
        createInfo.pQueueFamilyIndices = nullptr;
        createInfo.preTransform = swapchainSupport.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = true;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        VK_CHECK_RESULT(vkCreateSwapchainKHR(*_p->device, &createInfo, nullptr, &_swapchain));

        vkGetSwapchainImagesKHR(*_p->device, _swapchain, &imageCount, nullptr);
        _swapchainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(*_p->device, _swapchain, &imageCount, _swapchainImages.data());

        _swapchainExtent = extent;
        _swapchainImageFormat = surfaceFormat.format;
    }

    void hello_triangle_app::createUniformBuffers() {
        VkDeviceSize bufferSize = sizeof(uniform_buffer_object);

        _uniformBuffers.resize(_swapchainImages.size());
        _uniformBuffersMemory.resize(_swapchainImages.size());

        for (size_t i = 0; i < _swapchainImages.size(); ++i) {
            createBuffer(
                bufferSize,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                _uniformBuffers[i],
                _uniformBuffersMemory[i]
            );
        }
    }

    void hello_triangle_app::createTextureImage() {
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load(TEXTURE_PATH.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        if (pixels == nullptr)
            throw std::runtime_error("failed to load texture image");

        VkDeviceSize imageSize = texWidth * texHeight * 4;
        _mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1u;

        std::cout << "read " << TEXTURE_PATH << ": "
            << texWidth << 'x' << texHeight << 'x' << texChannels
            << " (" << _mipLevels << " mips)"
            << std::endl;

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(
            imageSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingBuffer,
            stagingBufferMemory);

        void* data;
        vkMapMemory(*_p->device, stagingBufferMemory, 0, imageSize, 0, &data);
        memcpy(data, pixels, static_cast<size_t>(imageSize));
        vkUnmapMemory(*_p->device, stagingBufferMemory);

        stbi_image_free(pixels);

        createImage(
            texWidth, texHeight, _mipLevels,
            VK_SAMPLE_COUNT_1_BIT,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            _textureImage, _textureImageMemory
        );
        transitionImageLayout(
            _textureImage,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            _mipLevels
        );
        copyBufferToImage(stagingBuffer, _textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
        // transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL while generating mipmaps

        generateMipmaps(_textureImage, VK_FORMAT_R8G8B8A8_UNORM, texWidth, texHeight, _mipLevels);

        vkDestroyBuffer(*_p->device, stagingBuffer, nullptr);
        vkFreeMemory(*_p->device, stagingBufferMemory, nullptr);
    }

    void hello_triangle_app::createTextureImageView() {
        _textureImageView = createImageView(
            _textureImage,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_ASPECT_COLOR_BIT,
            _mipLevels);
    }

    void hello_triangle_app::createTextureSampler() {
        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = 16.0f;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = static_cast<float>(_mipLevels);

        VK_CHECK_RESULT(vkCreateSampler(*_p->device, &samplerInfo, nullptr, &_textureSampler));
    }

    void hello_triangle_app::createVertexBuffer() {
        VkDeviceSize bufferSize = sizeof(_vertices[0]) * _vertices.size();

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(
            bufferSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingBuffer,
            stagingBufferMemory
        );
        void* data;
        vkMapMemory(*_p->device, stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, _vertices.data(), static_cast<size_t>(bufferSize));
        vkUnmapMemory(*_p->device, stagingBufferMemory);

        createBuffer(
            bufferSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            _vertexBuffer,
            _vertexBufferMemory
        );
        copyBuffer(stagingBuffer, _vertexBuffer, bufferSize);

        vkDestroyBuffer(*_p->device, stagingBuffer, nullptr);
        vkFreeMemory(*_p->device, stagingBufferMemory, nullptr);
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL hello_triangle_app::debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData
    ) {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
        return VK_FALSE;
    }

    void hello_triangle_app::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
        auto app = reinterpret_cast<hello_triangle_app*>(glfwGetWindowUserPointer(window));
        app->_framebufferResized = true;
    }

    void hello_triangle_app::drawFrame() {
        vkWaitForFences(*_p->device, 1u, &_inFlightFences[_currentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(
            *_p->device,
            _swapchain,
            UINT64_MAX,
            _imageAvailableSemaphores[_currentFrame],
            VK_NULL_HANDLE,
            &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapchain();
            return;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquired swapchain image");
        }

        updateUniformBuffer(imageIndex);

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = { _imageAvailableSemaphores[_currentFrame] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1u;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1u;
        submitInfo.pCommandBuffers = &_commandBuffers[imageIndex];

        VkSemaphore signalSemaphores[] = { _renderFinishedSemaphores[_currentFrame] };
        submitInfo.signalSemaphoreCount = 1u;
        submitInfo.pSignalSemaphores = signalSemaphores;

        vkResetFences(*_p->device, 1u, &_inFlightFences[_currentFrame]);

        result = vkQueueSubmit(_graphicsQueue, 1u, &submitInfo, _inFlightFences[_currentFrame]);
        if (result != VK_SUCCESS)
            throw std::runtime_error("failed to submit draw command buffer");

        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1u;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapchains[] = { _swapchain };
        presentInfo.swapchainCount = 1u;
        presentInfo.pSwapchains = swapchains;
        presentInfo.pImageIndices = &imageIndex;
        presentInfo.pResults = nullptr;

        result = vkQueuePresentKHR(_presentQueue, &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR
            || result == VK_SUBOPTIMAL_KHR
            || _framebufferResized
            || _fullscreenToggleRequested
        ) {
            recreateSwapchain();
        }
        else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image");
        }

        vkQueueWaitIdle(_presentQueue);

        _currentFrame = (_currentFrame + 1u) % MAX_FRAMES_IN_FLIGHT;
    }

    void hello_triangle_app::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
        _p->device->FlushCommandBuffer(commandBuffer, _graphicsQueue);
    }

    VkFormat hello_triangle_app::findDepthFormat() const {
        return findSupportedFormat(
            {
                VK_FORMAT_D24_UNORM_S8_UINT,
                VK_FORMAT_X8_D24_UNORM_PACK32,
                VK_FORMAT_D32_SFLOAT,
                VK_FORMAT_D32_SFLOAT_S8_UINT,
                VK_FORMAT_D16_UNORM_S8_UINT
            },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );
    }

    VkFormat hello_triangle_app::findSupportedFormat(
        const std::vector<VkFormat>& candidates,
        VkImageTiling tiling,
        VkFormatFeatureFlags features
    ) const {
        for (VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(*_p->device, format, &props);

            if (
                tiling == VK_IMAGE_TILING_LINEAR
                && (props.linearTilingFeatures & features) == features
            ) {
                return format;
            }
            else if (
                tiling == VK_IMAGE_TILING_OPTIMAL
                && (props.optimalTilingFeatures & features) == features
            ) {
                return format;
            }
        }

        return VK_FORMAT_UNDEFINED;
    }

    void hello_triangle_app::generateMipmaps(
        VkImage image,
        VkFormat imageFormat,
        int32_t texWidth,
        int32_t texHeight,
        uint32_t mipLevels
    ) {
        VkFormatProperties formatProperties;
        vkGetPhysicalDeviceFormatProperties(*_p->device, imageFormat, &formatProperties);

        VkFormatFeatureFlagBits requiredFlags = VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
        if ((formatProperties.optimalTilingFeatures & requiredFlags) != requiredFlags)
            throw std::runtime_error("texture image format does not suppoer linear blitting");

        std::cout << "generating " << mipLevels << " mips." << std::endl;

        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.image = image;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseArrayLayer = 0u;
        barrier.subresourceRange.layerCount = 1u;
        barrier.subresourceRange.levelCount = 1u;

        int32_t mipWidth = texWidth;
        int32_t mipHeight = texHeight;

        for (uint32_t i = 1u; i < mipLevels; ++i) {
            barrier.subresourceRange.baseMipLevel = i - 1u;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                0u, nullptr,
                0u, nullptr,
                1u, &barrier
            );

            VkImageBlit blit = {};
            blit.srcOffsets[0] = { 0, 0, 0 };
            blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = i - 1u;
            blit.srcSubresource.baseArrayLayer = 0u;
            blit.srcSubresource.layerCount = 1u;
            blit.dstOffsets[0] = { 0, 0, 0 };
            blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = 0u;
            blit.dstSubresource.layerCount = 1u;

            vkCmdBlitImage(
                commandBuffer,
                image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1u, &blit,
                VK_FILTER_LINEAR
            );

            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                0u, nullptr,
                0u, nullptr,
                1u, &barrier
            );

            if (mipWidth > 1) mipWidth /= 2;
            if (mipHeight > 1) mipHeight /= 2;
        }

        barrier.subresourceRange.baseMipLevel = mipLevels - 1u;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
            0u, nullptr,
            0u, nullptr,
            1u, &barrier
        );

        endSingleTimeCommands(commandBuffer);
    }

    VkSampleCountFlagBits hello_triangle_app::getMaxUsableSampleCount() const {
        VkPhysicalDeviceProperties physicalDeviceProperties;
        vkGetPhysicalDeviceProperties(*_p->device, &physicalDeviceProperties);

        VkSampleCountFlags counts = std::min(
            physicalDeviceProperties.limits.framebufferColorSampleCounts,
            physicalDeviceProperties.limits.framebufferDepthSampleCounts
        );

        #define TEST_SAMPLE_COUNT(x) \
            if ((counts & VK_SAMPLE_COUNT_##x##_BIT) == VK_SAMPLE_COUNT_##x##_BIT) \
                return VK_SAMPLE_COUNT_##x##_BIT;

        TEST_SAMPLE_COUNT(64);
        TEST_SAMPLE_COUNT(32);
        TEST_SAMPLE_COUNT(16);
        TEST_SAMPLE_COUNT(8);
        TEST_SAMPLE_COUNT(4);
        TEST_SAMPLE_COUNT(2);

        #undef TEST_SAMPLE_COUNT

        return VK_SAMPLE_COUNT_1_BIT;
    }

    std::vector<const char*> hello_triangle_app::getRequiredExtensions() const {
        uint32_t glfwExtensionCount = 0u;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
        for (const auto& extension : _instanceExtensions) {
            extensions.push_back(extension);
        }

#if ENABLE_VALIDATION_LAYERS
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

        return extensions;
    }

    void hello_triangle_app::handleGlfwKeyPress(GLFWwindow* window, int key, int scancode, int action, int mods) {
        if (window == nullptr) return;
        auto app = reinterpret_cast<vulkan_tutorial::hello_triangle_app*>(glfwGetWindowUserPointer(window));
        if (app == nullptr) return;

        app->handleKeyPress(key, scancode, action, mods);
    }

    void hello_triangle_app::handleKeyPress(int key, int scancode, int action, int mods) {
        if (action != GLFW_RELEASE) return;

        bool altDown = (mods & GLFW_MOD_ALT) == GLFW_MOD_ALT;
        if (altDown && (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER))
            toggleFullscreen();
    }

    bool hello_triangle_app::hasStencilComponent(VkFormat format) const {
        // Ordered by probability
        return format == VK_FORMAT_D24_UNORM_S8_UINT
            || format == VK_FORMAT_D32_SFLOAT_S8_UINT
            || format == VK_FORMAT_D16_UNORM_S8_UINT
            || format == VK_FORMAT_S8_UINT;
    }

    void hello_triangle_app::initInputHandlers() {
        glfwSetKeyCallback(_window, &handleGlfwKeyPress);
    }

    void hello_triangle_app::initVulkan() {
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapchain();
        createImageViews();
        createRenderPass();
        createDescriptorSetLayout();
        createGraphicsPipeline();
        createColorResources();
        createDepthResources();
        createFramebuffers();
        createTextureImage();
        createTextureImageView();
        createTextureSampler();
        loadModel();
        createVertexBuffer();
        createIndexBuffer();
        createUniformBuffers();
        createDescriptorPool();
        createDescriptorSets();
        createCommandBuffers();
        createSyncObjects();
    }

    void hello_triangle_app::initWindow() {
        if (glfwInit() != GLFW_TRUE)
            throw std::runtime_error("glfwInit failed");

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        _window = scoped_glfw_window();
        _window.init(INITIAL_WIDTH, INITIAL_HEIGHT, "Vulkan Test");
        glfwSetWindowUserPointer(_window.get(), this);
        glfwSetFramebufferSizeCallback(_window.get(), framebufferResizeCallback);
    }

    bool hello_triangle_app::isFullscreen() const {
        return false;
    }

    void hello_triangle_app::loadModel() {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        bool result = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATH.c_str());
        if (!result)
            throw std::runtime_error(warn + err);
        std::cout << "read " << MODEL_PATH << std::endl;

        std::unordered_map<vertex, uint32_t> uniqueVertices = {};

        for (const auto& shape : shapes) {
            for (const auto& index : shape.mesh.indices) {
                vertex vertex = {};

                vertex.pos = {
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]
                };

                vertex.texCoord = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };

                vertex.color = { 1.0f, 1.0f, 1.0f };

                if (uniqueVertices.count(vertex) == 0) {
                    uniqueVertices[vertex] = static_cast<uint32_t>(_vertices.size());
                    _vertices.push_back(vertex);
                }

                _indices.push_back(uniqueVertices[vertex]);
            }
        }
    }

    void hello_triangle_app::mainLoop() {
        while (glfwWindowShouldClose(_window.get()) == GLFW_FALSE) {
            glfwPollEvents();
            drawFrame();
        }

        _p->device->WaitIdle();
    }

    void hello_triangle_app::pickPhysicalDevice() {
        uint32_t deviceGroupCount = 0u;
        vkEnumeratePhysicalDeviceGroups(_instance, &deviceGroupCount, nullptr);
        std::vector<VkPhysicalDeviceGroupProperties> deviceGroups(deviceGroupCount);
        vkEnumeratePhysicalDeviceGroups(_instance, &deviceGroupCount, deviceGroups.data());

        std::multimap<int32_t, VkPhysicalDeviceGroupProperties> candidates;

        std::cout << "physical device groups:" << std::endl;
        for (int i = 0; i < deviceGroups.size(); ++i) {
            std::cout << "  device group #" << i << ':' << std::endl;
            const auto& deviceGroup = deviceGroups[i];
            if (deviceGroup.physicalDeviceCount == 0) {
                std::cout << "    no devices" << std::endl;
                continue;
            }

            std::cout << "    devices (" << deviceGroup.physicalDeviceCount << "):" << std::endl;
            int32_t score = INT32_MAX;
            for (int j = 0; j < deviceGroup.physicalDeviceCount; ++j) {
                const auto& device = deviceGroup.physicalDevices[j];
                VkPhysicalDeviceProperties deviceProperties;
                vkGetPhysicalDeviceProperties(device, &deviceProperties);
                std::cout << "      " << deviceProperties.deviceName << std::endl;
                if (j == 0)
                    print_device_extensions(device);
                score = std::min(score, rateDeviceSuitability(device));
            }
            candidates.insert(std::make_pair(score, deviceGroup));
        }

        const auto& winner = candidates.rbegin();
        if (winner->first > 0) {
            _p->device.reset(new vkf::VulkanDevice(winner->second.physicalDevices[0]));
            _msaaSamples = getMaxUsableSampleCount();
            std::cout << "using msaa samples: " << _msaaSamples << std::endl;
        }
        else {
            throw std::runtime_error("failed to find a suitable GPU!");
        }
    }

    void hello_triangle_app::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) const {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = &debugCallback;
    }

    swap_chain_support_details hello_triangle_app::querySwapchainSupport(VkPhysicalDevice device) const {
        swap_chain_support_details details;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, _surface, &details.capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, _surface, &formatCount, nullptr);
        if (formatCount > 0u) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, _surface, &formatCount, details.formats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, _surface, &presentModeCount, nullptr);
        if (presentModeCount > 0u) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, _surface, &presentModeCount, details.presentModes.data());
        }

        std::cout << "available present modes:" << std::endl;
        for (const auto& availablePresentMode : details.presentModes) {
            std::cout << "  " << getPresentModeName(availablePresentMode) << std::endl;
        }

        return details;
    }

    int32_t hello_triangle_app::rateDeviceSuitability(VkPhysicalDevice physicalDevice) const {
        if (!checkDeviceExtensionsSupport(physicalDevice)) {
            return 0;
        }

        swap_chain_support_details swapchainSupport = querySwapchainSupport(physicalDevice);
        if (swapchainSupport.formats.empty()
            || swapchainSupport.presentModes.empty())
        {
            return 0;
        }

        VkPhysicalDeviceFeatures deviceFeatures;
        VkPhysicalDeviceProperties deviceProperties;

        vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);
        vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);

        if (!deviceFeatures.samplerAnisotropy)
            return 0;

        int score = 0;

        if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score += 1000;
        }

        score += deviceProperties.limits.maxImageDimension2D;

        // Find a graphics queue family with surface present support
        uint32_t queue_family_count = 0u;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queue_family_count, nullptr);
        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queue_family_count, queue_families.data());

        for (uint32_t i = 0; i < queue_family_count; ++i) {
            const auto& queue_family = queue_families[i];
            if (queue_family.queueCount == 0u
                || (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0u
            ) {
                continue;
            }

            VkBool32 present_support = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, _surface, &present_support);
            if (present_support == VK_TRUE) {
                score += 1000;
                break;
            }
        }

        return score;
    }

    void hello_triangle_app::recreateSwapchain() {
        int width = 0, height = 0;
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(_window.get(), &width, &height);
            glfwWaitEvents();
        }

        _p->device->WaitIdle();

        cleanupSwapchain();

        createSwapchain();
        createImageViews();
        createRenderPass();
        createGraphicsPipeline();
        createColorResources();
        createDepthResources();
        createFramebuffers();
        createUniformBuffers();
        createDescriptorPool();
        createDescriptorSets();
        createCommandBuffers();
    }

    void hello_triangle_app::setupDebugMessenger() {
#if ENABLE_VALIDATION_LAYERS
        VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
        populateDebugMessengerCreateInfo(createInfo);

        auto result = CreateDebugUtilsMessengerEXT(_instance, &createInfo, nullptr, &_debugMessenger);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to set up the debug messenger!");
        }
#endif
    }

    void hello_triangle_app::transitionImageLayout(
        VkImage image,
        VkFormat format,
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        uint32_t mipLevels
    ) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.baseMipLevel = 0u;
        barrier.subresourceRange.levelCount = mipLevels;
        barrier.subresourceRange.baseArrayLayer = 0u;
        barrier.subresourceRange.layerCount = 1u;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = 0;

        if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            if (hasStencilComponent(format)) {
                barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }
        }
        else {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }

        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED
            && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        ) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
            && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        ) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED
            && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        ) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED
            && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        ) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        }
        else {
            throw std::invalid_argument("unsupported layout transition");
        }

        vkCmdPipelineBarrier(
            commandBuffer,
            sourceStage,
            destinationStage,
            0,
            0u, nullptr,
            0u, nullptr,
            1u, &barrier
        );

        endSingleTimeCommands(commandBuffer);
    }

    void hello_triangle_app::toggleFullscreen() {
        _fullscreenToggleRequested = true;
    }

    void hello_triangle_app::updateUniformBuffer(uint32_t currentImage) {
        static auto startTime = std::chrono::high_resolution_clock::now();

        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        uniform_buffer_object ubo = {};
        ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(30.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.view = glm::lookAt(
            glm::vec3(2.0f, 2.0f, 2.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 1.0f)
        );
        ubo.proj = glm::perspective(
            glm::radians(45.0f),
            _swapchainExtent.width / static_cast<float>(_swapchainExtent.height),
            0.1f,
            10.0f
        );
        ubo.proj[1][1] *= -1;

        void* data;
        vkMapMemory(*_p->device, _uniformBuffersMemory[currentImage], 0, sizeof(ubo), 0, &data);
        memcpy(data, &ubo, sizeof(ubo));
        vkUnmapMemory(*_p->device, _uniformBuffersMemory[currentImage]);
    }
}
