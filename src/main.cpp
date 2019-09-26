#include "hello_triangle_app.h"
#include "scoped_glfw_window.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main() {
    vulkan_tutorial::hello_triangle_app app;

    try {
        app.run();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
