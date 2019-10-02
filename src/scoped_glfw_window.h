#pragma once

class GLFWwindow;

namespace vulkan_tutorial {
    class scoped_glfw_window {
    public:
        scoped_glfw_window();
        scoped_glfw_window(GLFWwindow* window, bool owned = true);
        ~scoped_glfw_window();

        const GLFWwindow* get() const { return _window; }
        GLFWwindow* get() { return _window; }

        void destroy();
        void init(int width, int height, const char* title);

        operator const GLFWwindow*() const { return _window; }
        operator GLFWwindow*() { return _window; }

    private:
        bool _owned;
        GLFWwindow* _window;
    };
}
