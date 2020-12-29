#include <stdexcept>
#include <memory>

#include "GLFW/glfw3.h"
#include "glm/gtc/matrix_transform.hpp"

#include "Window.hpp"
#include "VulkanRenderer.hpp"

int main() {
    std::unique_ptr<Window> window = std::make_unique<Window>("Vulkan course", 800, 600);
    std::unique_ptr<VulkanRenderer> renderer = std::make_unique<VulkanRenderer>(window);

    if (renderer->init() == EXIT_FAILURE) return EXIT_FAILURE;

    float angle = 0.0f;
    float deltaTime = 0.0f;
    float lastTime = 0.0f;

    while (window->isOpen()) {
        glfwPollEvents();

        auto now = static_cast<float>(glfwGetTime());
        deltaTime = now - lastTime;
        lastTime = now;

        angle += 10.0f * deltaTime;

        if (angle > 360) angle -= 360.0f;

        renderer->updateModel(glm::rotate(glm::mat4(1.0f), glm::radians(angle),
                                          glm::vec3(0.0f, 0.0f, 1.0f)));

        renderer->draw();
    }

    renderer->clean();
    window->clean();

    return 0;
}
