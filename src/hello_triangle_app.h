#pragma once

#include "scoped_glfw_window.h"
#include <GLFW/glfw3.h>
#include <memory>
#include <optional>
#include <vector>

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

    class hello_triangle_app {
    public:
        hello_triangle_app();
        ~hello_triangle_app();

        void run();

    private:
        static const int INITIAL_HEIGHT = 600;
        static const int INITIAL_WIDTH = 800;

        #ifdef NDEBUG
        static const bool ENABLE_VALIDATION_LAYERS = false;
        #else
        static const bool ENABLE_VALIDATION_LAYERS = true;
        #endif

        scoped_glfw_window _window;

        std::unique_ptr<VkInstance> _instance;
        VkSurfaceKHR _surface;
        VkDevice _device;
        VkPhysicalDevice _physicalDevice;

        VkQueue _graphicsQueue;
        VkQueue _presentQueue;

        // TODO first candidate for first pass refactor into own class
        VkSwapchainKHR _swapchain;
        VkExtent2D _swapchainExtent;
        VkFormat _swapchainImageFormat;
        std::vector<VkImage> _swapchainImages;
        std::vector<VkImageView> _swapchainImageViews;

        VkDebugUtilsMessengerEXT _debugMessenger;
        const std::vector<const char*> _deviceExtensions;
        const std::vector<const char*> _validationLayers;

    private:
        static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            VkDebugUtilsMessageTypeFlagsEXT messageType,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            void* pUserData);

        bool checkDeviceExtensionsSupport(VkPhysicalDevice device) const;
        bool checkValidationLayerSupport() const;
        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;
        VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const;
        VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const;
        void cleanup();
        void createLogicalDevice();
        void createImageViews();
        void createInstance();
        void createSurface();
        void createSwapChain();
        queue_family_indices findQueueFamilies(VkPhysicalDevice device) const;
        std::vector<const char*> getRequiredExtensions() const;
        void initVulkan();
        void initWindow();
        void mainLoop();
        void pickPhysicalDevice();
        void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) const;
        swap_chain_support_details querySwapChainSupport(VkPhysicalDevice device) const;
        int rateDeviceSuitability(VkPhysicalDevice device) const;
        void setupDebugMessenger();
   };
}
