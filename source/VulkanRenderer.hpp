#ifndef VULKAN_COURSE_VULKANRENDERER_HPP
#define VULKAN_COURSE_VULKANRENDERER_HPP


#include <memory>
#include <stdexcept>
#include <vector>

#include "vulkan//vulkan.h"
#include "GLFW/glfw3.h"

#include "Window.hpp"


class ValidationLayers;

struct QueueFamilyIndices;
struct SwapChainDetails;

struct Device {
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice logicalDevice;
};

class VulkanRenderer {
    public:
        explicit VulkanRenderer(std::unique_ptr<Window>& window);
        ~VulkanRenderer();
        int init();
        void clean();

    private:
        // Vulkan function
        // - Create functions
        void createInstance();
        void createLogicalDevice();
        void createSurface();

        // - Get functions
        void getPhysicalDevice();

        // - Support functions
        // -- Checker functions
        bool checkInstanceSupport(std::vector<const char*>* extensions);
        bool checkDeviceExtensionSupport(VkPhysicalDevice device);
        bool checkDeviceSuitable(VkPhysicalDevice device);

        // -- Getter functions
        QueueFamilyIndices getQueueFamilies(VkPhysicalDevice device);
        SwapChainDetails getSwapChainDetails(VkPhysicalDevice device);

    private:
        std::unique_ptr<Window>& window_;
        std::unique_ptr<ValidationLayers> validationLayers;

        // Vulkan components
        VkInstance instance_{};
        Device device_{};
        VkQueue graphicsQueues_{};
        VkQueue presentationQueue_{};
        VkSurfaceKHR surface_{};
};


#endif
