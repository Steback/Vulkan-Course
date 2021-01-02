#ifndef VULKAN_COURSE_VULKANRENDERER_HPP
#define VULKAN_COURSE_VULKANRENDERER_HPP


#include <memory>
#include <stdexcept>
#include <vector>

#include "vulkan//vulkan.h"
#include "GLFW/glfw3.h"
#include <stb_image.h>

#include "Window.hpp"
#include "Mesh.hpp"


class ValidationLayers;
class Mesh;

struct QueueFamilyIndices;
struct SwapChainDetails;
struct SwapChainImage;
struct MVP;

struct Device {
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice logicalDevice{};
};

class VulkanRenderer {
    public:
        explicit VulkanRenderer(std::unique_ptr<Window>& window);
        ~VulkanRenderer();
        int init();
        void clean();
        void draw();
        void updateModel(int modelID, glm::mat4 newModel);

    private:
        // Vulkan function
        // - Create functions
        void createInstance();
        void createLogicalDevice();
        void createSurface();
        void createSwapChain();
        void createGraphicsPipeline();
        void createRenderPass();
        void createDescriptorSetLayout();
        void createPushConstantRange();
        void createDepthBufferImage();
        void createFramebuffers();
        void createCommandPool();
        void createCommandBuffers();
        void createSynchronisation();
        void createUniformBuffers();
        void createDescriptorPool();
        void createDescriptorSets();

        void updateUniformBuffers(uint32_t imageIndex);

        // - Record Functions
        void recordCommands(uint32_t currentImage);

        // - Get functions
        void getPhysicalDevice();

        // - Allocate functions
        void allocateDynamicBufferTransferSpace();

        // - Support functions
        // -- Checker functions
        bool checkInstanceSupport(std::vector<const char*>* extensions);
        bool checkDeviceExtensionSupport(VkPhysicalDevice device);
        bool checkDeviceSuitable(VkPhysicalDevice device);

        // -- Getter functions
        QueueFamilyIndices getQueueFamilies(VkPhysicalDevice device);
        SwapChainDetails getSwapChainDetails(VkPhysicalDevice device);

        // -- Choose functions
        VkSurfaceFormatKHR chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
        VkPresentModeKHR chooseBestPresentationMode(const std::vector<VkPresentModeKHR>& presentationModes);
        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities);
        VkFormat chooseSupportedFormat(const std::vector<VkFormat> &formats, VkImageTiling tiling,
                                       VkFormatFeatureFlags featureFlags);

        // -- Create functions
        VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
        VkShaderModule createShaderModule(const std::vector<char>& code);
        VkImage createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                            VkImageUsageFlags usageFlags, VkMemoryPropertyFlags propertyFlags,
                            VkDeviceMemory* imageMemory);
        int createTexture(const std::string& fileName);

        // -- Loader Functions
        stbi_uc* loadTextureFile(const std::string& fileName, int* width, int* height, VkDeviceSize* imageSize);

    private:
        int currentFrame{0};

        std::unique_ptr<Window>& window_;
        std::unique_ptr<ValidationLayers> validationLayers;

        // Scene objects
        std::vector<Mesh> meshList;

        // Scene Settings
        UboViewProjection uboViewProjection{};

        // Vulkan components
        // - Main
        VkInstance instance_{};
        Device device_{};
        VkQueue graphicsQueues_{};
        VkQueue presentationQueue_{};
        VkSurfaceKHR surface_{};
        VkSwapchainKHR swapChain_{};
        std::vector<SwapChainImage> swapChainImages_;
        std::vector<VkFramebuffer> swapChainFramebuffers_;
        std::vector<VkCommandBuffer> commandBuffers_;
        VkImage depthBufferImage{};
        VkDeviceMemory depthBufferImageMemory{};
        VkImageView depthBufferImageView{};

        // - Descriptors
        VkDescriptorSetLayout descriptorSetLayout{};
        VkDescriptorPool descriptorPool{};
        std::vector<VkBuffer> vpUniformBuffer;
        std::vector<VkDeviceMemory> vpUniformBufferMemory;
        std::vector<VkBuffer> modelDUniformBuffer;
        std::vector<VkDeviceMemory> modelDUniformBufferMemory;
        std::vector<VkDescriptorSet> descriptorSets;
//        VkDeviceSize minUniformBufferOffset_{};
//        size_t modelUniformAlignment{};
//        UboModel* modelTransferSpace{};
        VkPushConstantRange pushConstantRange{};

        // - Assets
        std::vector<VkImage> textureImages;
        std::vector<VkDeviceMemory> textureImageMemory;

        // - Pipeline
        VkPipeline graphicsPipeline_{};
        VkPipelineLayout pipelineLayout{};
        VkRenderPass renderPass_{};

        // Pools
        VkCommandPool graphicsCommandPool{};

        // - Utility
        VkFormat swapChainImageFormat_{};
        VkExtent2D swapChainExtent_{};

        // - Synchronisation
        std::vector<VkSemaphore> imageAvailable;
        std::vector<VkSemaphore> renderFinished;
        std::vector<VkFence> drawFences;
};


#endif
