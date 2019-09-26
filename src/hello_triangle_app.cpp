#include "hello_triangle_app.h"
#include "scoped_glfw_window.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <GLFW/glfw3.h>

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

    void print_extensions() {
        uint32_t extensionCount = 0u;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> extensions(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

        std::cout << "available extensions:" << std::endl;
        for (const auto& extension : extensions) {
            std::cout << '\t' << extension.extensionName << std::endl;
        }
    }
}

namespace vulkan_tutorial {
    hello_triangle_app::hello_triangle_app()
        : _instance{new VkInstance()}
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

    void hello_triangle_app::cleanup() {
        if (_instance == nullptr)
            return;

        vkDestroyInstance(*_instance.get(), nullptr);
        _instance.reset();
        _window.destroy();
        glfwTerminate();
    }

    void hello_triangle_app::createInstance() {
        auto appInfo = create_vk_application_info();

        uint32_t glfwExtensionCount = 0u;
        const char** glfwExtensions;

        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        print_extensions();

        VkInstanceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = glfwExtensionCount;
        createInfo.ppEnabledExtensionNames = glfwExtensions;
        createInfo.enabledLayerCount = 0;

        VkResult result = vkCreateInstance(&createInfo, nullptr, _instance.get());
        if (result != VK_SUCCESS)
            throw std::runtime_error("Failed to create VkInstance");
    }

    void hello_triangle_app::initVulkan() {
        createInstance();
    }

    void hello_triangle_app::initWindow() {
        if (glfwInit() != GLFW_TRUE)
            throw std::runtime_error("glfwInit failed");

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        _window = scoped_glfw_window();
        _window.init(INITIAL_WIDTH, INITIAL_HEIGHT, "Vulkan Test");
    }

    void hello_triangle_app::mainLoop() {
        while (glfwWindowShouldClose(_window.get()) == GLFW_FALSE) {
            glfwPollEvents();
        }
    }
}
