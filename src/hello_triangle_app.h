#pragma once

#include "scoped_glfw_window.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include <array>
#include <experimental/propagate_const>
#include <memory>
#include <optional>
#include <vector>

#ifndef NDEBUG
#define ENABLE_VALIDATION_LAYERS 1
#endif

namespace vkf {
    class VulkanBuffer;
}

namespace vulkan_tutorial {
    struct queue_family_indices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete() const {
            return graphicsFamily.has_value()
                && presentFamily.has_value();
        }
    };

    struct swap_chain_support_details {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    struct uniform_buffer_object {
        alignas(16) glm::mat4 model;
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
    };

    struct vertex {
        glm::vec3 pos;
        glm::vec3 color;
        glm::vec2 texCoord;

        static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();
        static VkVertexInputBindingDescription getBindingDescription();

        bool operator==(const vertex& other) const {
            return pos == other.pos && texCoord == other.texCoord && color == other.color;
        }
    };

    class hello_triangle_app {
    public:
        hello_triangle_app();
        ~hello_triangle_app();

        void run();

    private:
        struct Impl; std::experimental::propagate_const<std::unique_ptr<Impl>> _p;

        static const int INITIAL_HEIGHT = 600;
        static const int INITIAL_WIDTH = 800;
        static const int MAX_FRAMES_IN_FLIGHT = 3;

        const std::string MODEL_PATH = "models/chalet.obj";
        const std::string TEXTURE_PATH = "textures/chalet.jpg";

        bool _fullscreenToggleRequested;

        scoped_glfw_window _window;

        VkInstance _instance;
        VkSurfaceKHR _surface;
        const std::vector<const char*> _instanceExtensions;
        const std::vector<const char*> _validationLayers;

        VkDebugUtilsMessengerEXT _debugMessenger;
        VkSampleCountFlagBits _msaaSamples;

        VkPipeline _graphicsPipeline;
        VkDescriptorPool _descriptorPool;
        VkDescriptorSetLayout _descriptorSetLayout;
        std::vector<VkDescriptorSet> _descriptorSets;
        VkPipelineLayout _pipelineLayout;
        VkRenderPass _renderPass;
        VkSwapchainKHR _swapchain;
        VkExtent2D _swapchainExtent;
        std::vector<VkFramebuffer> _swapchainFramebuffers;
        VkFormat _swapchainImageFormat;
        std::vector<VkImage> _swapchainImages;
        std::vector<VkImageView> _swapchainImageViews;

        std::vector<VkCommandBuffer> _commandBuffers;
        std::vector<VkSemaphore> _imageAvailableSemaphores;
        std::vector<VkSemaphore> _renderFinishedSemaphores;
        std::vector<VkFence> _inFlightFences;
        uint32_t _currentFrame;
        bool _framebufferResized;

        VkImage _colorImage;
        VkDeviceMemory _colorImageMemory;
        VkImageView _colorImageView;

        VkImage _depthImage;
        VkDeviceMemory _depthImageMemory;
        VkImageView _depthImageView;

        uint32_t _mipLevels;
        VkImage _textureImage;
        VkDeviceMemory _textureImageMemory;
        VkImageView _textureImageView;
        VkSampler _textureSampler;

        std::vector<uint32_t> _indices;
        std::vector<vertex> _vertices;

    private:
        static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            VkDebugUtilsMessageTypeFlagsEXT messageType,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            void* pUserData
        );
        static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

        VkCommandBuffer beginSingleTimeCommands();
        bool checkDeviceExtensionsSupport(VkPhysicalDevice device) const;
        bool checkValidationLayerSupport() const;
        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;
        VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const;
        VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const;
        void cleanup();
        void cleanupSwapchain();
        void copyBuffer(vkf::VulkanBuffer* src_buffer, vkf::VulkanBuffer* dst_buffer, VkDeviceSize size);
        void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
        void createColorResources();
        void createCommandBuffers();
        void createCommandPools();
        void createDepthResources();
        void createDescriptorPool();
        void createDescriptorSetLayout();
        void createDescriptorSets();
        void createFramebuffers();
        void createGraphicsPipeline();
        void createImage(
            uint32_t width,
            uint32_t height,
            uint32_t mipLevels,
            VkSampleCountFlagBits numSamples,
            VkFormat format,
            VkImageTiling tiling,
            VkImageUsageFlags usage,
            VkMemoryPropertyFlags properties,
            VkImage& image,
            VkDeviceMemory& imageMemory);
        VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);
        void createImageViews();
        void createIndexBuffer();
        void createInstance();
        void createLogicalDevice();
        void createRenderPass();
        void createSyncObjects();
        VkShaderModule createShaderModule(const std::vector<char>& code);
        void createSurface();
        void createSwapchain();
        void createTextureImage();
        void createTextureImageView();
        void createTextureSampler();
        void createUniformBuffers();
        void createVertexBuffer();
        void drawFrame();
        void endSingleTimeCommands(VkCommandBuffer commandBuffer);
        VkFormat findDepthFormat() const;
        VkFormat findSupportedFormat(
            const std::vector<VkFormat>& candidates,
            VkImageTiling tiling,
            VkFormatFeatureFlags features) const;
        void generateMipmaps(VkImage image, VkFormat format, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);
        VkSampleCountFlagBits getMaxUsableSampleCount() const;
        std::vector<const char*> getRequiredExtensions() const;
        static void handleGlfwKeyPress(GLFWwindow* window, int key, int scancode, int action, int mods);
        void handleKeyPress(int32_t key, int32_t scancode, int32_t action, int32_t mods);
        bool hasStencilComponent(VkFormat format) const;
        void initInputHandlers();
        void initVulkan();
        void initWindow();
        bool isFullscreen() const;
        void loadModel();
        void mainLoop();
        void pickPhysicalDevice();
        void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) const;
        swap_chain_support_details querySwapchainSupport(VkPhysicalDevice device) const;
        int32_t rateDeviceSuitability(VkPhysicalDevice device) const;
        void recreateSwapchain();
        void setupDebugMessenger();
        void toggleFullscreen();
        void transitionImageLayout(
            VkImage image,
            VkFormat format,
            VkImageLayout oldLayout,
            VkImageLayout newLayout,
            uint32_t mipLevels);
        void updateUniformBuffer(uint32_t imageIndex);
   };
}
