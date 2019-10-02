#include "scoped_glfw_window.h"
#include <GLFW/glfw3.h>

namespace vulkan_tutorial {
    scoped_glfw_window::scoped_glfw_window()
        : _owned{false}, _window{nullptr}
    {}

    scoped_glfw_window::scoped_glfw_window(GLFWwindow* window, bool owned)
        : _owned{owned}, _window{window}
    {}

    scoped_glfw_window::~scoped_glfw_window() {
        destroy();
    }

    void scoped_glfw_window::destroy() {
        if (_window == nullptr)
            return;

        if (_owned)
            glfwDestroyWindow(_window);
        _owned = false;
        _window = nullptr;
    }

    void scoped_glfw_window::init(int width, int height, const char* title) {
        destroy();
        _window = glfwCreateWindow(width, height, title, nullptr, nullptr);
        _owned = true;
    }
}
