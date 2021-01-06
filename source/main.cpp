#include <stdexcept>
#include <memory>

#define GLFW_FORCE_DEPTH_ZERO_TO_ONE
#include "GLFW/glfw3.h"
#include "glm/gtc/matrix_transform.hpp"

#include "Window.hpp"
#include "VulkanRenderer.hpp"


int main() {
    std::unique_ptr<Window> window = std::make_unique<Window>("Vulkan course", 1366, 768);
    std::unique_ptr<VulkanRenderer> renderer = std::make_unique<VulkanRenderer>(window);

    if (renderer->init() == EXIT_FAILURE) return EXIT_FAILURE;

    float angle = 0.0f;
    float deltaTime = 0.0f;
    float lastTime = 0.0f;

    int helicopter = renderer->createMeshModel("../assets/models/uh60.obj");

    while (window->isOpen()) {
        glfwPollEvents();

        auto now = static_cast<float>(glfwGetTime());
        deltaTime = now - lastTime;
        lastTime = now;

        angle += 10.0f * deltaTime;

        if (angle > 350.0f) angle -= 360.0f;

        glm::mat4 testMat = glm::rotate(glm::mat4(1.0f), glm::radians(angle),
                                        glm::vec3(0.0f, 1.0f, 0.0f));
        testMat = glm::rotate(testMat, glm::radians(-90.0f),
                              glm::vec3(1.0f, 0.0f, 0.0f));

        renderer->updateModel(helicopter, testMat);

        renderer->draw();
    }

    renderer->clean();
    window->clean();

    return 0;
}
