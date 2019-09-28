#pragma once

#include "scoped_glfw_window.h"
#include <GLFW/glfw3.h>
#include <memory>
#include <optional>
#include <vector>

namespace vulkan_tutorial {
    struct queue_family_indices {
        std::optional<uint32_t> graphicsFamily;

        bool isComplete() const {
            return graphicsFamily.has_value();
        }
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
        VkDebugUtilsMessengerEXT _debugMessenger;
        VkDevice _device;
        VkQueue _graphicsQueue;
        VkPhysicalDevice _physicalDevice;
        const std::vector<const char*> _validationLayers;

    private:
        static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            VkDebugUtilsMessageTypeFlagsEXT messageType,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            void* pUserData);

        bool checkValidationLayerSupport() const;
        void cleanup();
        void createLogicalDevice();
        void createInstance();
        queue_family_indices findQueueFamilies(VkPhysicalDevice device) const;
        std::vector<const char*> getRequiredExtensions() const;
        void initVulkan();
        void initWindow();
        void mainLoop();
        void pickPhysicalDevice();
        void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
        int rateDeviceSuitability(VkPhysicalDevice device) const;
        void setupDebugMessenger();
   };
}
