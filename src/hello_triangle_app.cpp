#include "hello_triangle_app.h"
#include "scoped_glfw_window.h"

#include <stdexcept>
#include <GLFW/glfw3.h>

namespace vulkan_tutorial {
    void hello_triangle_app::run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

    void hello_triangle_app::initVulkan() {
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

    void hello_triangle_app::cleanup() {
        _window.destroy();
        glfwTerminate();
    }
}
