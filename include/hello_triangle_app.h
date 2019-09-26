#pragma once

#include "scoped_glfw_window.h"
#include <GLFW/glfw3.h>
#include <memory>

namespace vulkan_tutorial {
    class hello_triangle_app {
    public:
        hello_triangle_app();
        ~hello_triangle_app();

        void run();

    private:
        const int INITIAL_HEIGHT = 600;
        const int INITIAL_WIDTH = 800;

        std::unique_ptr<VkInstance> _instance;
        scoped_glfw_window _window;

        void cleanup();
        void createInstance();
        void initVulkan();
        void initWindow();
        void mainLoop();
    };
}
