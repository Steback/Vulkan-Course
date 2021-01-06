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
class MeshModel;

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
        int createMeshModel(const std::string& modelFile);

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
        void createColourBufferImage();
        void createDepthBufferImage();
        void createFramebuffers();
        void createCommandPool();
        void createCommandBuffers();
        void createSynchronisation();
        void createUniformBuffers();
        void createDescriptorPool();
        void createDescriptorSets();
        void createTextureSampler();
        void createInputDescriptorSets();

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
        int createTextureImage(const std::string& fileName);
        int createTexture(const std::string& fileName);
        int createTextureDescriptor(VkImageView textureImage);

        // -- Loader Functions
        stbi_uc* loadTextureFile(const std::string& fileName, int* width, int* height, VkDeviceSize* imageSize);

    private:
        int currentFrame{0};

        std::unique_ptr<Window>& window_;
        std::unique_ptr<ValidationLayers> validationLayers;

        // Scene objects
        std::vector<MeshModel> modelList;

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
        std::vector<VkImage> depthBufferImages;
        std::vector<VkDeviceMemory> depthBufferImageMemory;
        std::vector<VkImageView> depthBufferImageView;
        std::vector<VkImage> colourBufferImages;
        std::vector<VkDeviceMemory> colourBufferImageMemory;
        std::vector<VkImageView> colourBufferImageView;
        VkSampler textureSampler{};

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
        VkDescriptorPool samplerDescriptorPool{};
        VkDescriptorSetLayout samplerSetLayout{};
        std::vector<VkDescriptorSet> samplerDescriptorSets;
        VkDescriptorSetLayout inputSetLayout{};
        VkDescriptorPool inputDescriptorPool{};
        std::vector<VkDescriptorSet> inputDescriptorSets{};

        // - Assets
        std::vector<VkImage> textureImages;
        std::vector<VkDeviceMemory> textureImageMemory;
        std::vector<VkImageView> textureImageViews;

        // - Pipeline
        VkPipeline graphicsPipeline_{};
        VkPipelineLayout pipelineLayout{};
        VkRenderPass renderPass_{};
        VkPipeline secondPipeline{};
        VkPipelineLayout secondPipeLineLayout{};

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
