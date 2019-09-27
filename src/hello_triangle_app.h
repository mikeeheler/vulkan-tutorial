#pragma once

#include "scoped_glfw_window.h"
#include <GLFW/glfw3.h>
#include <memory>
#include <vector>

namespace vulkan_tutorial {
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

        VkDebugUtilsMessengerEXT _debugMessenger;
        std::unique_ptr<VkInstance> _instance;
        const std::vector<const char*> _validationLayers = {
            "VK_LAYER_KHRONOS_validation"
        };
        scoped_glfw_window _window;

        static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            VkDebugUtilsMessageTypeFlagsEXT messageType,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            void* pUserData);

        bool checkValidationLayerSupport() const;
        void cleanup();
        void createInstance();
        std::vector<const char*> getRequiredExtensions() const;
        void initVulkan();
        void initWindow();
        void mainLoop();
        void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
        void setupDebugMessenger();
    };
}
