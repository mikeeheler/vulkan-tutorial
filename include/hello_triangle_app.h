#pragma once

#include "scoped_glfw_window.h"

namespace vulkan_tutorial {
    class hello_triangle_app {
    public:
        void run();

    private:
        const int INITIAL_HEIGHT = 600;
        const int INITIAL_WIDTH = 800;

        scoped_glfw_window _window;

        void cleanup();
        void initVulkan();
        void initWindow();
        void mainLoop();
    };
}
