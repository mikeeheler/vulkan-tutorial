#include "hello_triangle_app.h"
#include "scoped_glfw_window.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stb/stb_image.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>

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

    void print_extensions() {
        uint32_t extensionCount = 0u;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> extensions(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

        std::cout << "available extensions:" << std::endl;
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
    std::array<VkVertexInputAttributeDescription, 2> vertex::getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions = {};

        attributeDescriptions[0].binding = 0u;
        attributeDescriptions[0].location = 0u;
        attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(vertex, pos);

        attributeDescriptions[1].binding = 0u;
        attributeDescriptions[1].location = 1u;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(vertex, color);

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
      : _commandBuffers {},
        _commandPool {VK_NULL_HANDLE},
        _currentFrame(0u),
        _debugMessenger {nullptr},
        _descriptorPool {VK_NULL_HANDLE},
        _descriptorSetLayout {VK_NULL_HANDLE},
        _descriptorSets {},
        _device {VK_NULL_HANDLE},
        _deviceExtensions {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        },
        _framebufferResized {false},
        _graphicsPipeline {VK_NULL_HANDLE},
        _graphicsQueue {VK_NULL_HANDLE},
        _imageAvailableSemaphores {},
        _indexBuffer {VK_NULL_HANDLE},
        _indexBufferMemory {VK_NULL_HANDLE},
        _indices {0, 1, 2, 2, 3, 0},
        _inFlightFences {},
        _instance {VK_NULL_HANDLE},
        _physicalDevice {VK_NULL_HANDLE},
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
            "VK_LAYER_KHRONOS_validation"
        },
        _vertexBuffer {VK_NULL_HANDLE},
        _vertexBufferMemory {VK_NULL_HANDLE},
        _vertices {
            {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
            {{ 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
            {{ 0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
            {{-0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}}
        },
        _window {}
    {}

    hello_triangle_app::~hello_triangle_app() {
        cleanup();
    }

    void hello_triangle_app::run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

    VkCommandBuffer hello_triangle_app::beginSingleTimeCommands() {
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = _commandPool;
        allocInfo.commandBufferCount = 1u;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(_device, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        return commandBuffer;
    }

    bool hello_triangle_app::checkDeviceExtensionsSupport(VkPhysicalDevice device) const {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(_deviceExtensions.begin(), _deviceExtensions.end());

        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    bool hello_triangle_app::checkValidationLayerSupport() const {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

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

        vkDestroySampler(_device, _textureSampler, nullptr);
        vkDestroyImageView(_device, _textureImageView, nullptr);
        vkDestroyImage(_device, _textureImage, nullptr);
        vkFreeMemory(_device, _textureImageMemory, nullptr);
        vkDestroyDescriptorSetLayout(_device, _descriptorSetLayout, nullptr);
        vkDestroyBuffer(_device, _indexBuffer, nullptr);
        vkFreeMemory(_device, _indexBufferMemory, nullptr);
        vkDestroyBuffer(_device, _vertexBuffer, nullptr);
        vkFreeMemory(_device, _vertexBufferMemory, nullptr);
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            vkDestroySemaphore(_device, _imageAvailableSemaphores[i], nullptr);
            vkDestroySemaphore(_device, _renderFinishedSemaphores[i], nullptr);
            vkDestroyFence(_device, _inFlightFences[i], nullptr);
        }
        vkDestroyCommandPool(_device, _commandPool, nullptr);
        vkDestroyDevice(_device, nullptr);
        if (ENABLE_VALIDATION_LAYERS) {
            DestroyDebugUtilsMessengerEXT(_instance, _debugMessenger, nullptr);
        }
        vkDestroySurfaceKHR(_instance, _surface, nullptr);
        vkDestroyInstance(_instance, nullptr);
        _window.destroy();
        glfwTerminate();

        _commandPool = VK_NULL_HANDLE;
        _debugMessenger = VK_NULL_HANDLE;
        _descriptorSetLayout = VK_NULL_HANDLE;
        _device = VK_NULL_HANDLE;
        _imageAvailableSemaphores.clear();
        _indexBuffer = VK_NULL_HANDLE;
        _indexBufferMemory = VK_NULL_HANDLE;
        _inFlightFences.clear();
        _instance = VK_NULL_HANDLE;
        _graphicsQueue = VK_NULL_HANDLE;
        _physicalDevice = VK_NULL_HANDLE;
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
        for (auto framebuffer : _swapchainFramebuffers) {
            vkDestroyFramebuffer(_device, framebuffer, nullptr);
        }
        vkFreeCommandBuffers(_device, _commandPool, static_cast<uint32_t>(_commandBuffers.size()), _commandBuffers.data());
        vkDestroyPipeline(_device, _graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(_device, _pipelineLayout, nullptr);
        vkDestroyRenderPass(_device, _renderPass, nullptr);
        for (const auto& imageView: _swapchainImageViews) {
            vkDestroyImageView(_device, imageView, nullptr);
        }
        vkDestroySwapchainKHR(_device, _swapchain, nullptr);
        for (size_t i = 0; i < _swapchainImages.size(); ++i) {
            vkDestroyBuffer(_device, _uniformBuffers[i], nullptr);
            vkFreeMemory(_device, _uniformBuffersMemory[i], nullptr);
        }
        vkDestroyDescriptorPool(_device, _descriptorPool, nullptr);

        _commandBuffers.clear();
        _descriptorPool = VK_NULL_HANDLE;
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

        VkResult result = vkCreateBuffer(_device, &bufferInfo, nullptr, &buffer);
        if (result != VK_SUCCESS)
            throw std::runtime_error("failed to create vertex buffer");

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(_device, buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

        result = vkAllocateMemory(_device, &allocInfo, nullptr, &bufferMemory);
        if (result != VK_SUCCESS)
            throw std::runtime_error("failed to allocate vertex buffer memory");

        vkBindBufferMemory(_device, buffer, bufferMemory, 0);
    }

    void hello_triangle_app::createCommandBuffers() {
        _commandBuffers.resize(_swapchainFramebuffers.size());

        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = _commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = static_cast<uint32_t>(_commandBuffers.size());

        VkResult result = vkAllocateCommandBuffers(_device, &allocInfo, _commandBuffers.data());
        if (result != VK_SUCCESS)
            throw std::runtime_error("failed to allocate command buffers");

        for (size_t i = 0; i < _commandBuffers.size(); ++i) {
            const auto& commandBuffer = _commandBuffers[i];

            VkCommandBufferBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = 0u;
            beginInfo.pInheritanceInfo = nullptr;

            VkResult beginResult = vkBeginCommandBuffer(commandBuffer, &beginInfo);
            if (beginResult != VK_SUCCESS)
                throw std::runtime_error("failed to begin recording command buffer");

            VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
            VkRenderPassBeginInfo renderPassInfo = {};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = _renderPass;
            renderPassInfo.framebuffer = _swapchainFramebuffers[i];
            renderPassInfo.renderArea.offset = { 0, 0 };
            renderPassInfo.renderArea.extent = _swapchainExtent;
            renderPassInfo.clearValueCount = 1u;
            renderPassInfo.pClearValues = &clearColor;

            vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline);
            VkBuffer vertexBuffers[] = {_vertexBuffer};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0u, 1u, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(commandBuffer, _indexBuffer, 0, VK_INDEX_TYPE_UINT16);
            vkCmdBindDescriptorSets(
                commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelineLayout,
                0u, 1u, &_descriptorSets[i],
                0u, nullptr);
            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(_indices.size()), 1u, 0u, 0u, 0u);
            vkCmdEndRenderPass(commandBuffer);

            VkResult endResult = vkEndCommandBuffer(commandBuffer);
            if (endResult != VK_SUCCESS)
                throw std::runtime_error("failed to record command buffer");
        }
    }

    void hello_triangle_app::createCommandPool() {
        queue_family_indices queueFamilyIndices = findQueueFamilies(_physicalDevice);

        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
        poolInfo.flags = 0u;

        VkResult result = vkCreateCommandPool(_device, &poolInfo, nullptr, &_commandPool);
        if (result != VK_SUCCESS)
            throw std::runtime_error("failed to create command pool");
    }

    void hello_triangle_app::createDescriptorPool() {
        VkDescriptorPoolSize poolSize = {};
        poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSize.descriptorCount = static_cast<uint32_t>(_swapchainImages.size());

        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1u;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = static_cast<uint32_t>(_swapchainImages.size());

        VkResult result = vkCreateDescriptorPool(_device, &poolInfo, nullptr, &_descriptorPool);
        if (result != VK_SUCCESS)
            throw std::runtime_error("failed to create descriptor pool");
    }

    void hello_triangle_app::createDescriptorSetLayout() {
        VkDescriptorSetLayoutBinding uboLayoutBinding = {};
        uboLayoutBinding.binding = 0u;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.descriptorCount = 1u;
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        uboLayoutBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1u;
        layoutInfo.pBindings = &uboLayoutBinding;

        VkResult result = vkCreateDescriptorSetLayout(_device, &layoutInfo, nullptr, &_descriptorSetLayout);
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
        VkResult result = vkAllocateDescriptorSets(_device, &allocInfo, _descriptorSets.data());
        if (result != VK_SUCCESS)
            throw std::runtime_error("failed to allocate descriptor");

        for (size_t i = 0; i < _swapchainImages.size(); ++i) {
            VkDescriptorBufferInfo bufferInfo = {};
            bufferInfo.buffer = _uniformBuffers[i];
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(uniform_buffer_object);

            VkWriteDescriptorSet descriptorWrite = {};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = _descriptorSets[i];
            descriptorWrite.dstBinding = 0u;
            descriptorWrite.dstArrayElement = 0u;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrite.descriptorCount = 1u;
            descriptorWrite.pBufferInfo = &bufferInfo;
            descriptorWrite.pImageInfo = nullptr;
            descriptorWrite.pTexelBufferView = nullptr;

            vkUpdateDescriptorSets(_device, 1u, &descriptorWrite, 0u, nullptr);
        }
    }

    void hello_triangle_app::createFramebuffers() {
        _swapchainFramebuffers.resize(_swapchainImageViews.size());

        for (size_t i = 0; i < _swapchainImageViews.size(); ++i) {
            VkImageView attachments[] = {
                _swapchainImageViews[i]
            };

            VkFramebufferCreateInfo framebufferInfo = {};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = _renderPass;
            framebufferInfo.attachmentCount = 1u;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = _swapchainExtent.width;
            framebufferInfo.height = _swapchainExtent.height;
            framebufferInfo.layers = 1u;

            VkResult result = vkCreateFramebuffer(_device, &framebufferInfo, nullptr, &_swapchainFramebuffers[i]);
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
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f;
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

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1u;
        pipelineLayoutInfo.pSetLayouts = &_descriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 0u;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;

        VkResult result = vkCreatePipelineLayout(_device, &pipelineLayoutInfo, nullptr, &_pipelineLayout);
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
        pipelineInfo.pDepthStencilState = nullptr;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = nullptr;
        pipelineInfo.layout = _pipelineLayout;
        pipelineInfo.renderPass = _renderPass;
        pipelineInfo.subpass = 0u;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = -1;

        result = vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_graphicsPipeline);
        if (result != VK_SUCCESS)
            throw std::runtime_error("failed to create graphics pipeline");

        vkDestroyShaderModule(_device, vertShaderModule, nullptr);
        vkDestroyShaderModule(_device, fragShaderModule, nullptr);
    }

    void hello_triangle_app::createImage(
        uint32_t width, uint32_t height, VkFormat format,
        VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
        VkImage& image, VkDeviceMemory& imageMemory
    ) {
        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1u;
        imageInfo.mipLevels = 1u;
        imageInfo.arrayLayers = 1u;
        imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.flags = 0;

        VkResult result = vkCreateImage(_device, &imageInfo, nullptr, &image);
        if (result != VK_SUCCESS)
            throw std::runtime_error("failed to create image");

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(_device, image, &memRequirements);

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        result = vkAllocateMemory(_device, &allocInfo, nullptr, &imageMemory);
        if (result != VK_SUCCESS)
            throw std::runtime_error("failed to allocate image memory");

        vkBindImageMemory(_device, image, imageMemory, 0);
    }

    VkImageView hello_triangle_app::createImageView(VkImage image, VkFormat format) {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0u;
        viewInfo.subresourceRange.levelCount = 1u;
        viewInfo.subresourceRange.baseArrayLayer = 0u;
        viewInfo.subresourceRange.layerCount = 1u;

        VkImageView imageView;
        VkResult result = vkCreateImageView(_device, &viewInfo, nullptr, &imageView);
        if (result != VK_SUCCESS)
            throw std::runtime_error("failed to create texture image view");

        return imageView;
    }

    void hello_triangle_app::createImageViews() {
        _swapchainImageViews.resize(_swapchainImages.size());

        for (size_t i = 0; i < _swapchainImages.size(); ++i) {
            std::cout << "create view for swapchain image #" << i << std::endl;
            _swapchainImageViews[i] = createImageView(_swapchainImages[i], _swapchainImageFormat);
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
        vkMapMemory(_device, stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, _indices.data(), static_cast<size_t>(bufferSize));
        vkUnmapMemory(_device, stagingBufferMemory);

        createBuffer(
            bufferSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            _indexBuffer,
            _indexBufferMemory
        );

        copyBuffer(stagingBuffer, _indexBuffer, bufferSize);

        vkDestroyBuffer(_device, stagingBuffer, nullptr);
        vkFreeMemory(_device, stagingBufferMemory, nullptr);
    }

    void hello_triangle_app::createInstance() {
        if (ENABLE_VALIDATION_LAYERS && !checkValidationLayerSupport()) {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        auto appInfo = create_vk_application_info();
        auto extensions = getRequiredExtensions();

        print_extensions();

        VkInstanceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
        if (ENABLE_VALIDATION_LAYERS) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(_validationLayers.size());
            createInfo.ppEnabledLayerNames = _validationLayers.data();

            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
        }
        else {
            createInfo.enabledLayerCount = 0u;
            createInfo.pNext = nullptr;
        }

        VkResult result = vkCreateInstance(&createInfo, nullptr, &_instance);
        if (result != VK_SUCCESS)
            throw std::runtime_error("Failed to create VkInstance");
    }

    void hello_triangle_app::createLogicalDevice() {
        queue_family_indices indices = findQueueFamilies(_physicalDevice);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {
            indices.graphicsFamily.value(),
            indices.presentFamily.value()
        };

        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo = {};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1u;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures = {};

        VkDeviceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(_deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = _deviceExtensions.data();

        if (ENABLE_VALIDATION_LAYERS) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(_validationLayers.size());
            createInfo.ppEnabledLayerNames = _validationLayers.data();
        }
        else {
            createInfo.enabledLayerCount = 0u;
        }

        VkResult result = vkCreateDevice(_physicalDevice, &createInfo, nullptr, &_device);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical device!");
        }

        vkGetDeviceQueue(_device, indices.graphicsFamily.value(), 0u, &_graphicsQueue);
        vkGetDeviceQueue(_device, indices.presentFamily.value(), 0u, &_presentQueue);
    }

    void hello_triangle_app::createRenderPass() {
        VkAttachmentDescription colorAttachment = {};
        colorAttachment.format = _swapchainImageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachmentRef = {};
        colorAttachmentRef.attachment = 0u;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1u;
        subpass.pColorAttachments = &colorAttachmentRef;

        VkSubpassDependency dependency = {};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0u;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0u;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
            | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1u;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1u;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1u;
        renderPassInfo.pDependencies = &dependency;

        VkResult result = vkCreateRenderPass(_device, &renderPassInfo, nullptr, &_renderPass);
        if (result != VK_SUCCESS)
            throw std::runtime_error("failed to create render pass");
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
            VkResult result = vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &_imageAvailableSemaphores[i]);
            if (result != VK_SUCCESS)
                throw std::runtime_error("failed to create image available semaphore");

            result = vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &_renderFinishedSemaphores[i]);
            if (result != VK_SUCCESS)
                throw std::runtime_error("failed to create render finished semaphore");

            result = vkCreateFence(_device, &fenceInfo, nullptr, &_inFlightFences[i]);
            if (result != VK_SUCCESS)
                throw std::runtime_error("failed to create in-flight fence");
        }
    }

    VkShaderModule hello_triangle_app::createShaderModule(const std::vector<char>& code) {
        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shaderModule;
        VkResult result = vkCreateShaderModule(_device, &createInfo, nullptr, &shaderModule);
        if (result != VK_SUCCESS)
            throw std::runtime_error("failed to create shader module");

        return shaderModule;
    }

    void hello_triangle_app::createSurface() {
        VkResult result = glfwCreateWindowSurface(_instance, _window.get(), nullptr, &_surface);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface!");
        }
    }

    void hello_triangle_app::createSwapchain() {
        swap_chain_support_details swapchainSupport = querySwapchainSupport(_physicalDevice);

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

        queue_family_indices indices = findQueueFamilies(_physicalDevice);
        uint32_t queueFamilyIndices[] = {
            indices.graphicsFamily.value(),
            indices.presentFamily.value()
        };

        if (indices.graphicsFamily != indices.presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2u;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0u;
            createInfo.pQueueFamilyIndices = nullptr;
        }

        createInfo.preTransform = swapchainSupport.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = true;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        VkResult result = vkCreateSwapchainKHR(_device, &createInfo, nullptr, &_swapchain);
        if (result != VK_SUCCESS)
            throw std::runtime_error("failed to create the swapchain");

        vkGetSwapchainImagesKHR(_device, _swapchain, &imageCount, nullptr);
        _swapchainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(_device, _swapchain, &imageCount, _swapchainImages.data());

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
        stbi_uc* pixels = stbi_load("textures/texture.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        if (pixels == nullptr)
            throw std::runtime_error("failed to load texture image");

        std::cout << "read textures/textures.jpg: " << texWidth << 'x' << texHeight << 'x' << texChannels << std::endl;

        VkDeviceSize imageSize = texWidth * texHeight * 4;

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(
            imageSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingBuffer,
            stagingBufferMemory);

        void* data;
        vkMapMemory(_device, stagingBufferMemory, 0, imageSize, 0, &data);
        memcpy(data, pixels, static_cast<size_t>(imageSize));
        vkUnmapMemory(_device, stagingBufferMemory);

        stbi_image_free(pixels);

        createImage(
            texWidth, texHeight, VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            _textureImage, _textureImageMemory
        );
        transitionImageLayout(
            _textureImage,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        );
        copyBufferToImage(stagingBuffer, _textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
        transitionImageLayout(
            _textureImage,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );

        vkDestroyBuffer(_device, stagingBuffer, nullptr);
        vkFreeMemory(_device, stagingBufferMemory, nullptr);
    }

    void hello_triangle_app::createTextureImageView() {
        _textureImageView = createImageView(_textureImage, VK_FORMAT_R8G8B8A8_UNORM);
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
        samplerInfo.maxLod = 1.0f;

        VkResult result = vkCreateSampler(_device, &samplerInfo, nullptr, &_textureSampler);
        if (result != VK_SUCCESS)
            throw std::runtime_error("failed to create texture sampler");
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
        vkMapMemory(_device, stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, _vertices.data(), static_cast<size_t>(bufferSize));
        vkUnmapMemory(_device, stagingBufferMemory);

        createBuffer(
            bufferSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            _vertexBuffer,
            _vertexBufferMemory
        );
        copyBuffer(stagingBuffer, _vertexBuffer, bufferSize);

        vkDestroyBuffer(_device, stagingBuffer, nullptr);
        vkFreeMemory(_device, stagingBufferMemory, nullptr);
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
        vkWaitForFences(_device, 1u, &_inFlightFences[_currentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(
            _device,
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

        vkResetFences(_device, 1u, &_inFlightFences[_currentFrame]);

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
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || _framebufferResized) {
            recreateSwapchain();
        }
        else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image");
        }

        vkQueueWaitIdle(_presentQueue);

        _currentFrame = (_currentFrame + 1u) % MAX_FRAMES_IN_FLIGHT;
    }

    void hello_triangle_app::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1u;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(_graphicsQueue, 1u, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(_graphicsQueue);

        vkFreeCommandBuffers(_device, _commandPool, 1u, &commandBuffer);
    }

    uint32_t hello_triangle_app::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(_physicalDevice, &memProperties);

        for (uint32_t i = 0u; i < memProperties.memoryTypeCount; ++i) {
            bool matchesFilter = (typeFilter & (1 << i)) != 0u;
            bool hasProperties = (memProperties.memoryTypes[i].propertyFlags & properties) == properties;
            if (matchesFilter && hasProperties) {
                return i;
            }
        }

        throw std::runtime_error("failed to find a suitable memory type");
    }

    queue_family_indices hello_triangle_app::findQueueFamilies(VkPhysicalDevice device) const {
        queue_family_indices indices;

        uint32_t queueFamilyCount = 0u;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        for (int i = 0; i < queueFamilies.size(); ++i) {
            const auto& queueFamily = queueFamilies[i];

            if (queueFamily.queueCount > 0
                && (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) == VK_QUEUE_GRAPHICS_BIT)
            {
                indices.graphicsFamily = i;
            }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, _surface, &presentSupport);
            if (queueFamily.queueCount > 0 && presentSupport) {
                indices.presentFamily = i;
            }

            if (indices.isComplete()) {
                break;
            }
        }

        return indices;
    }

    std::vector<const char*> hello_triangle_app::getRequiredExtensions() const {
        uint32_t glfwExtensionCount = 0u;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (ENABLE_VALIDATION_LAYERS) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
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
        createFramebuffers();
        createCommandPool();
        createTextureImage();
        createTextureImageView();
        createTextureSampler();
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

    void hello_triangle_app::mainLoop() {
        while (glfwWindowShouldClose(_window.get()) == GLFW_FALSE) {
            glfwPollEvents();
            drawFrame();
        }

        vkDeviceWaitIdle(_device);
    }

    void hello_triangle_app::pickPhysicalDevice() {
        uint32_t deviceCount = 0u;
        vkEnumeratePhysicalDevices(_instance, &deviceCount, nullptr);
        if (deviceCount == 0u) {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(_instance, &deviceCount, devices.data());

        std::multimap<int, VkPhysicalDevice> candidates;

        VkPhysicalDeviceProperties deviceProperties;
        for (int i = 0; i < devices.size(); ++i) {
            const auto& device = devices[i];
            vkGetPhysicalDeviceProperties(device, &deviceProperties);

            std::cout << "device #" << i << ": " << deviceProperties.deviceName << std::endl;
            std::cout << "  api      : " << getVersionString(deviceProperties.apiVersion) << std::endl;
            std::cout << "  driver   : " << getVersionString(deviceProperties.driverVersion) << std::endl;
            std::cout << "  vendor_id: " << reinterpret_cast<void*>(deviceProperties.vendorID) << std::endl;
            std::cout << "  device_id: " << reinterpret_cast<void*>(deviceProperties.deviceID) << std::endl;

            int score = rateDeviceSuitability(device);
            candidates.insert(std::make_pair(score, device));
        }

        if (candidates.rbegin()->first > 0) {
            _physicalDevice = candidates.rbegin()->second;
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

    int hello_triangle_app::rateDeviceSuitability(VkPhysicalDevice device) const {
        if (!checkDeviceExtensionsSupport(device)) {
            return 0;
        }

        swap_chain_support_details swapchainSupport = querySwapchainSupport(device);
        if (swapchainSupport.formats.empty()
            || swapchainSupport.presentModes.empty())
        {
            return 0;
        }

        VkPhysicalDeviceFeatures deviceFeatures;
        VkPhysicalDeviceProperties deviceProperties;

        vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
        vkGetPhysicalDeviceProperties(device, &deviceProperties);

        int score = 0;

        if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score += 1000;
        }

        score += deviceProperties.limits.maxImageDimension2D;

        queue_family_indices indices = findQueueFamilies(device);
        if (indices.isComplete())
            score += 1000;


        return score;
    }

    void hello_triangle_app::recreateSwapchain() {
        int width = 0, height = 0;
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(_window.get(), &width, &height);
            glfwWaitEvents();
        }

        vkDeviceWaitIdle(_device);

        cleanupSwapchain();

        createSwapchain();
        createImageViews();
        createRenderPass();
        createGraphicsPipeline();
        createFramebuffers();
        createUniformBuffers();
        createDescriptorPool();
        createDescriptorSets();
        createCommandBuffers();
    }

    void hello_triangle_app::setupDebugMessenger() {
        if (!ENABLE_VALIDATION_LAYERS)
            return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
        populateDebugMessengerCreateInfo(createInfo);

        auto result = CreateDebugUtilsMessengerEXT(_instance, &createInfo, nullptr, &_debugMessenger);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to set up the debug messenger!");
        }
    }

    void hello_triangle_app::transitionImageLayout(
        VkImage image,
        VkFormat format,
        VkImageLayout oldLayout,
        VkImageLayout newLayout
    ) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0u;
        barrier.subresourceRange.levelCount = 1u;
        barrier.subresourceRange.baseArrayLayer = 0u;
        barrier.subresourceRange.layerCount = 1u;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = 0;

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
            1u, &barrier);

        endSingleTimeCommands(commandBuffer);
    }

    void hello_triangle_app::updateUniformBuffer(uint32_t currentImage) {
        static auto startTime = std::chrono::high_resolution_clock::now();

        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        uniform_buffer_object ubo = {};
        ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
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
        vkMapMemory(_device, _uniformBuffersMemory[currentImage], 0, sizeof(ubo), 0, &data);
        memcpy(data, &ubo, sizeof(ubo));
        vkUnmapMemory(_device, _uniformBuffersMemory[currentImage]);
    }
}
