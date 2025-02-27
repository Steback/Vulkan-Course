#include <set>
#include <algorithm>
#include <array>
#include <malloc.h>

#include "spdlog/spdlog.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "vulkan/vulkan.hpp"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

#include "VulkanRenderer.hpp"
#include "Utilities.hpp"
#include "ValidationLayers.hpp"
#include "MeshModel.hpp"

VulkanRenderer::VulkanRenderer(std::unique_ptr<Window> &window) : window_(window) {  }

VulkanRenderer::~VulkanRenderer() = default;

int VulkanRenderer::init() {
    try {
        createInstance();
        createSurface();
        getPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createRenderPass();
        createDescriptorSetLayout();
        createPushConstantRange();
        createGraphicsPipeline();
        createColourBufferImage();
        createDepthBufferImage();
        createFramebuffers();
        createCommandPool();
        createCommandBuffers();
        createTextureSampler();
//        allocateDynamicBufferTransferSpace();
        createUniformBuffers();
        createDescriptorPool();
        createDescriptorSets();
        createInputDescriptorSets();
        createSynchronisation();

        uboViewProjection.projection = glm::perspective(glm::radians(45.0f),
                                                        static_cast<float>(swapChainExtent_.width) / static_cast<float>(swapChainExtent_.height),
                                                        0.1f, 100.0f);

        uboViewProjection.view = glm::lookAt(glm::vec3(10.0f, 0.0f, 20.0f), glm::vec3(0.0f, 0.0f, -2.0f),
                                             glm::vec3(0.0f, 1.0f, 0.0f));

        uboViewProjection.projection[1][1] *= -1;

        // Create our default "no texture" texture
        createTexture("plain.png");
    } catch (const std::runtime_error& error) {
        spdlog::error("[Vulkan-Renderer] {}", error.what());

        return EXIT_FAILURE;
    }

    return 0;
}

void VulkanRenderer::draw() {
    // -- GET NEXT IMAGE --
    // Wait for given fence to signal (open) from last draw before continuing
    vkWaitForFences(device_.logicalDevice, 1, &drawFences[currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());

    // Manually reset (close) fences
    vkResetFences(device_.logicalDevice, 1, &drawFences[currentFrame]);

    // Get index of next image to be drawn to, and signal semaphore when ready to be drawn to
    uint32_t imageIndex;

    vkAcquireNextImageKHR(device_.logicalDevice, swapChain_, std::numeric_limits<uint64_t>::max(),
                          imageAvailable[currentFrame], VK_NULL_HANDLE, &imageIndex);

    recordCommands(imageIndex);
    updateUniformBuffers(imageIndex);

    // -- SUBMIT COMMAND BUFFER TO RENDER --
    // Queue submission information
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;										// Number of semaphores to wait on
    submitInfo.pWaitSemaphores = &imageAvailable[currentFrame];				// List of semaphores to wait on

    VkPipelineStageFlags waitStages[] = {
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };

    submitInfo.pWaitDstStageMask = waitStages;						// Stages to check semaphores at
    submitInfo.commandBufferCount = 1;								// Number of command buffers to submit
    submitInfo.pCommandBuffers = &commandBuffers_[imageIndex];		// Command buffer to submit
    submitInfo.signalSemaphoreCount = 1;							// Number of semaphores to signal
    submitInfo.pSignalSemaphores = &renderFinished[currentFrame];	// Semaphores to signal when command buffer finishes

    // Submit command buffer to queue
    VkResult result = vkQueueSubmit(graphicsQueues_, 1, &submitInfo, drawFences[currentFrame]);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit Command Buffer to Queue");
    }

    // -- PRESENT RENDERED IMAGE TO SCREEN --
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;										// Number of semaphores to wait on
    presentInfo.pWaitSemaphores = &renderFinished[currentFrame];			// Semaphores to wait on
    presentInfo.swapchainCount = 1;											// Number of swapchains to present to
    presentInfo.pSwapchains = &swapChain_;									// Swapchains to present images to
    presentInfo.pImageIndices = &imageIndex;								// Index of images in swapchains to present

    // Present image
    result = vkQueuePresentKHR(presentationQueue_, &presentInfo);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present Image!");
    }

    // Get next frame (use % swapChainImages.size() to keep value below swapChainImages.size())
    currentFrame = (currentFrame + 1) % MAX_FRAME_DRAWS;
}

void VulkanRenderer::clean() {
    spdlog::info("[Vulkan-Renderer] Destroy Device and Instance");

    // Wait until no actions being run on device before destroying
    vkDeviceWaitIdle(device_.logicalDevice);

//    std::free(modelTransferSpace);

    for (auto & model : modelList) {
        model.clean();
    }

    vkDestroyDescriptorPool(device_.logicalDevice, inputDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device_.logicalDevice, inputSetLayout, nullptr);

    vkDestroyDescriptorPool(device_.logicalDevice, samplerDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device_.logicalDevice, samplerSetLayout, nullptr);

    vkDestroySampler(device_.logicalDevice, textureSampler, nullptr);

    for (size_t i = 0; i < textureImages.size(); ++i) {
        vkDestroyImageView(device_.logicalDevice, textureImageViews[i], nullptr);
        vkDestroyImage(device_.logicalDevice, textureImages[i], nullptr);
        vkFreeMemory(device_.logicalDevice, textureImageMemory[i], nullptr);
    }

    for (size_t i = 0; i < colourBufferImages.size(); ++i) {
        vkDestroyImageView(device_.logicalDevice, colourBufferImageView[i], nullptr);
        vkDestroyImage(device_.logicalDevice, colourBufferImages[i], nullptr);
        vkFreeMemory(device_.logicalDevice, colourBufferImageMemory[i], nullptr);
    }

    for (size_t i = 0; i < depthBufferImages.size(); ++i) {
        vkDestroyImageView(device_.logicalDevice, depthBufferImageView[i], nullptr);
        vkDestroyImage(device_.logicalDevice, depthBufferImages[i], nullptr);
        vkFreeMemory(device_.logicalDevice, depthBufferImageMemory[i], nullptr);
    }

    vkDestroyDescriptorPool(device_.logicalDevice, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device_.logicalDevice, descriptorSetLayout, nullptr);

    for (size_t i = 0; i < swapChainImages_.size(); ++i) {
        vkDestroyBuffer(device_.logicalDevice, vpUniformBuffer[i], nullptr);
        vkFreeMemory(device_.logicalDevice, vpUniformBufferMemory[i], nullptr);
//        vkDestroyBuffer(device_.logicalDevice, modelDUniformBuffer[i], nullptr);
//        vkFreeMemory(device_.logicalDevice, modelDUniformBufferMemory[i], nullptr);
    }

    for (size_t i = 0; i < MAX_FRAME_DRAWS; ++i) {
        vkDestroySemaphore(device_.logicalDevice, renderFinished[i], nullptr);
        vkDestroySemaphore(device_.logicalDevice, imageAvailable[i], nullptr);
        vkDestroyFence(device_.logicalDevice, drawFences[i], nullptr);
    }

    vkDestroyCommandPool(device_.logicalDevice, graphicsCommandPool, nullptr);

    for (auto& framebuffer : swapChainFramebuffers_) {
        vkDestroyFramebuffer(device_.logicalDevice, framebuffer, nullptr);
    }

    vkDestroyPipeline(device_.logicalDevice, secondPipeline, nullptr);
    vkDestroyPipelineLayout(device_.logicalDevice, secondPipeLineLayout, nullptr);

    vkDestroyPipeline(device_.logicalDevice, graphicsPipeline_, nullptr);
    vkDestroyPipelineLayout(device_.logicalDevice, pipelineLayout, nullptr);
    vkDestroyRenderPass(device_.logicalDevice, renderPass_, nullptr);

    for (auto& image : swapChainImages_) {
        vkDestroyImageView(device_.logicalDevice, image.imageView, nullptr);
    }

    vkDestroySwapchainKHR(device_.logicalDevice, swapChain_, nullptr);
    vkDestroySurfaceKHR(instance_, surface_, nullptr);
    vkDestroyDevice(device_.logicalDevice, nullptr);
    validationLayers->clean(instance_);
    vkDestroyInstance(instance_, nullptr);
}

void VulkanRenderer::updateModel(int modelID, glm::mat4 newModel) {
    if (modelID >= modelList.size()) return;

    modelList[modelID].setModel({newModel});
}

void VulkanRenderer::createInstance() {
    if (enableValidationLayers) {
        validationLayers = std::make_unique<ValidationLayers>(std::vector<const char*>{
                "VK_LAYER_KHRONOS_validation"
        });

        if (!validationLayers->checkValidationLayerSupport()) {
            throw std::runtime_error("Validation layers requested, but not available!");
        }
    }

    // Information about the application itself
    // Most data here doesn't affect the program and is for development convenience
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan App"; // Custom name of the application
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0); // Custom version of the application
    appInfo.pEngineName = "No Engine"; // Custom engine name
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0); // Custom engine version
    appInfo.apiVersion = VK_API_VERSION_1_2; // The vulkan version

    // Creation information for VkInstance (Vulkan Instance)
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    // Create list to hold instance extensions
    std::vector<const char *> instanceExtensions{};

    // Set up extensions Instances will use
    uint32_t extensionsCount = 0; // GLFW may require multiple extensions
    const char** glfwExtensions; // Extensions passed as array of cstrings, so need pointer (the array) to pointer (the cstrings)

    // Get GLFW extensions
    glfwExtensions = glfwGetRequiredInstanceExtensions(&extensionsCount);

    // Add GLFW extensions to list fo extensions
    instanceExtensions = std::vector<const char*>(glfwExtensions, glfwExtensions + extensionsCount);

    if (enableValidationLayers) {
        instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    // Check Instance extensions supported
    if (!checkInstanceSupport(&instanceExtensions)) {
        throw std::runtime_error("VkInstance does not support require extensions");
    }

    createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
    createInfo.ppEnabledExtensionNames = instanceExtensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;

    if (enableValidationLayers) {
        createInfo.enabledLayerCount = validationLayers->getValidationLayersCount();
        createInfo.ppEnabledLayerNames = validationLayers->getValidationLayers();

        // Get errors messages about create and destroy Vulkan Objects
        validationLayers->populateDebugMessengerCreateInfo(debugCreateInfo);

        createInfo.pNext = &debugCreateInfo;
    } else {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }

    spdlog::info("[Vulkan-Renderer] Create Instance");

    // Create instance
    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance_);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Instance");
    }

    if (enableValidationLayers) {
        validationLayers->init(instance_);
    }
}

void VulkanRenderer::createLogicalDevice() {
    spdlog::info("[Vulkan-Renderer] Create Logical Device");

    // Get the queue family devices for the chosen Physical device
    QueueFamilyIndices indices = getQueueFamilies(device_.physicalDevice);

    // Vector for queue creation information, and set for family indices
    std::vector<VkDeviceQueueCreateInfo> queuesCreateInfos;
    std::set<uint32_t> queueFamilyIndices = {
            indices.graphicsFamily.value(),
            indices.presentationFamily.value()
    };

    // Queues the logical device needs to create and info to do so
    for (auto queueFamilyIndex : queueFamilyIndices) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamilyIndex;  // The index of the family to create a queue form
        queueCreateInfo.queueCount = 1; // Numbers of queues to create
        float priority = 1.0f;
        queueCreateInfo.pQueuePriorities = &priority; // Vulkan needs to know how to handle multiple queue, so decide priority (1 = highest priority)

        queuesCreateInfos.push_back(queueCreateInfo);
    }


    // Information to create logical device(sometimes called "device")
    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queuesCreateInfos.size()); // Number of Queue create info
    deviceCreateInfo.pQueueCreateInfos = queuesCreateInfos.data(); // List of queue create infos so device can create required queues
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()); // number of enable Logical device extensions
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data(); // List of enable Logical device extensions

    // Physical Device Features the logical Device will be using
    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE; // Enable Anisotropy

    deviceCreateInfo.pEnabledFeatures = &deviceFeatures; // Physical Device features Logical Device will use

    // Create the logical device for the given physical device
    VkResult result = vkCreateDevice(device_.physicalDevice, &deviceCreateInfo, nullptr, &device_.logicalDevice);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to created a Logical Device");
    }

    // Queues are created at the same time as the device so we want handle to queues
    // From given logical device, of given Queue Family, of given Queue Index (0 since only one queue), place reference in given vkQueue
    vkGetDeviceQueue(device_.logicalDevice, indices.graphicsFamily.value(), 0, &graphicsQueues_);
    vkGetDeviceQueue(device_.logicalDevice, indices.presentationFamily.value(), 0, &presentationQueue_);
}

void VulkanRenderer::createSurface() {
    // Create surface (creates a surface create info struct, runs the create surface function, returns result)
    VkResult result = glfwCreateWindowSurface(instance_, window_->window_, nullptr, &surface_);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create a surface");
    }
}

void VulkanRenderer::createSwapChain() {
    // Get Swap Chain details so we can pick best settings
    SwapChainDetails swapChainDetails = getSwapChainDetails(device_.physicalDevice);

    // Find optimal values for our swap chain
    VkSurfaceFormatKHR surfaceFormat = chooseBestSurfaceFormat(swapChainDetails.formats);
    VkPresentModeKHR presentMode = chooseBestPresentationMode(swapChainDetails.presentationModes);
    VkExtent2D extent = chooseSwapExtent(swapChainDetails.surfaceCapabilities);

    // How many images are in the swap chain? Get 1 more than the minimum to allow triple buffering
    uint32_t imageCount = swapChainDetails.surfaceCapabilities.minImageCount + 1;

    // If imageCount higher than max, then clamp down to max
    // If 0, then limitless
    if (swapChainDetails.surfaceCapabilities.maxImageCount > 0
            && swapChainDetails.surfaceCapabilities.maxImageCount < imageCount) {
        imageCount = swapChainDetails.surfaceCapabilities.maxImageCount;
    }

    // Create information for swap chain
    VkSwapchainCreateInfoKHR swapChainCreateInfo{};
    swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapChainCreateInfo.surface = surface_;
    swapChainCreateInfo.imageFormat = surfaceFormat.format;
    swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapChainCreateInfo.presentMode = presentMode;
    swapChainCreateInfo.imageExtent = extent;
    swapChainCreateInfo.minImageCount = imageCount;
    swapChainCreateInfo.imageArrayLayers = 1; // Number of layers for each image in chain
    swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // What attachment images will be used as
    swapChainCreateInfo.preTransform = swapChainDetails.surfaceCapabilities.currentTransform; // Transform to perform on swap chain images
    swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // How to handle blending images with external graphics (e.g. other windows)
    swapChainCreateInfo.clipped = VK_TRUE; // Whether to clip parts of image not in view (e.g. behind another window. off screen, etc)

    // Get queue Family indices
    QueueFamilyIndices indices = getQueueFamilies(device_.physicalDevice);

    // If Graphics and Presentation families are different, then swapchain must let images be shared between families
    if (indices.graphicsFamily.value() != indices.presentationFamily.value()) {
        uint32_t queueFamilyIndices[] = {
                indices.graphicsFamily.value(),
                indices.presentationFamily.value()
        };

        swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT; // Images share handling
        swapChainCreateInfo.queueFamilyIndexCount = 2; // Number of queues to shared images between
        swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices; // Array to queues to share between
    } else {
        swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapChainCreateInfo.queueFamilyIndexCount = 0;
        swapChainCreateInfo.pQueueFamilyIndices = nullptr;
    }

    // If old swap chain been destroyed and this one replaces it, the link old one to quickly hand over responsibilities
    swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

    // Create Swapchain
    VkResult result = vkCreateSwapchainKHR(device_.logicalDevice, &swapChainCreateInfo, nullptr, &swapChain_);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Swapchain");
    }

    // Store for later reference
    swapChainImageFormat_ = surfaceFormat.format;
    swapChainExtent_ = extent;

    // Get Swapchain images (first count, then values)
    uint32_t swapChainImageCount = 0;
    vkGetSwapchainImagesKHR(device_.logicalDevice, swapChain_, &swapChainImageCount, nullptr);
    std::vector<VkImage> images(swapChainImageCount);
    vkGetSwapchainImagesKHR(device_.logicalDevice, swapChain_, &swapChainImageCount, images.data());

    for (VkImage image : images) {
        // Store image handle
        SwapChainImage swapChainImage{
            .image = image,
            .imageView = createImageView(image, swapChainImageFormat_, VK_IMAGE_ASPECT_COLOR_BIT)
        };

        // Add to swapcahin images list
        swapChainImages_.push_back(swapChainImage);
    }
}

void VulkanRenderer::createGraphicsPipeline() {
    // Read in SPIR-V code of shaders
    auto vertexShaderCode = readFile("../shaders/shader.vert.spv");
    auto fragmentShaderCode = readFile("../shaders/shader.frag.spv");

    // Create shaders module
    VkShaderModule vertShaderModule = createShaderModule(vertexShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragmentShaderCode);

    // -- SHADER STAGE CREATION INFORMATION --
    // Vertex stage creation information
    VkPipelineShaderStageCreateInfo vertexShaderCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT, // Shader stage name
        .module = vertShaderModule, // Shader module to be used by stage
        .pName = "main" // Entry point in to
    };

    // Fragment stage creation information
    VkPipelineShaderStageCreateInfo fragmentShaderCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = fragShaderModule,
        .pName = "main"
    };

    // Put shader stage creation info in to array
    // Graphics Pipeline creation info requires array of shader stage creates
    VkPipelineShaderStageCreateInfo shaderStages[] = {
            vertexShaderCreateInfo,
            fragmentShaderCreateInfo
    };

    // How the data for a single vertex (including info such position, colour, texture, coords, normals, etc) is as a whole
    VkVertexInputBindingDescription bindingDescription{
        .binding = 0, // Can bin multiple streams of data, this defines which one
        .stride = sizeof(Vertex), // Size of a single vertex object
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX // How to move between data after each vertex
    };
    // VK_VERTEX_INPUT_RATE_VERTEX : Move on to the next vertex
    // VK_VERTEX_INPUT_RATE_INSTANCE : Move to a vertex for the next instance

    // How the data for an attribute is defined within a vertex
    std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

    // Position attribute
    attributeDescriptions[0] = {
        .location = 0, // Location in shader where data will be read from
        .binding = 0, // Which binding the data is at (should be same as above)
        .format = VK_FORMAT_R32G32B32_SFLOAT, // Format the data will take (also helps define size of data)
        .offset = offsetof(Vertex, pos) // Where this attribute is defined in the data for a single vertex
    };

    // Color attribute
    attributeDescriptions[1] = {
            .location = 1, // Location in shader where data will be read from
            .binding = 0, // Which binding the data is at (should be same as above)
            .format = VK_FORMAT_R32G32B32_SFLOAT, // Format the data will take (also helps define size of data)
            .offset = offsetof(Vertex, col) // Where this attribute is defined in the data for a single vertex
    };

    // Texture Coords attribute
    attributeDescriptions[2] = {
            .location = 2, // Location in shader where data will be read from
            .binding = 0, // Which binding the data is at (should be same as above)
            .format = VK_FORMAT_R32G32_SFLOAT, // Format the data will take (also helps define size of data)
            .offset = offsetof(Vertex, tex) // Where this attribute is defined in the data for a single vertex
    };

    // -- VERTEX INPUT --
    VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &bindingDescription, // List of Vertex Binding Description (data spacing/stride information)
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
        .pVertexAttributeDescriptions = attributeDescriptions.data(), // List of Vertex Attribute Descriptions (data description and where to bind to/from)
    };

    // -- INPUT ASSEMBLY --
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // Primitive type to assemble vertices as
        .primitiveRestartEnable = VK_FALSE // Allow overriding of "strip" topology to start new primitives
    };

    // -- VIEWPORT & SCISSOR --
    // Create viewport info struct
    VkViewport viewport{
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(swapChainExtent_.width),
        .height = static_cast<float>(swapChainExtent_.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    // Create scissor info struct
    VkRect2D scissor{
        .offset = {0, 0}, // Offset to use region from
        .extent = swapChainExtent_ // Extent to describe region to use, starting on offset
    };

    VkPipelineViewportStateCreateInfo viewportStateCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
    };

    // -- DYNAMIC STATES --
    // Dynamic states to enable
//    std::vector<VkDynamicState> dynamicStateEnables{
//        VK_DYNAMIC_STATE_VIEWPORT, // Dynamic Viewport: Can resize in command buffer with vkCmdSetViewport(commandBuffer, 0, 1, &viewport)
//        VK_DYNAMIC_STATE_SCISSOR // Dynamic Scissor: Can resize in command with vkCmdSetScissor(commandBuffet, 0, 1, &scissor)
//    };

    // Dynamic State create info
//    VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo{
//        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
//        .dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size()),
//        .pDynamicStates = dynamicStateEnables.data()
//    };

    // -- RASTERIZER --
    VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE, // Changes if fragments beyond near/far planes are clipped(default) or clamped to plane
        .rasterizerDiscardEnable = VK_FALSE, // Whether to discard data and skip resterizer. Never creates fragments, only suitable for pipeline without framebuffer output
        .polygonMode = VK_POLYGON_MODE_FILL, // How to handle filling points between vertices
        .cullMode = VK_CULL_MODE_BACK_BIT, // Which face of a tri to cull
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE, // Winding to determine which side is front
        .depthBiasClamp = VK_FALSE, // Whether to add depth bias to fragments "shadow acne" in shadow mapping
        .lineWidth = 1.0f, // How thick lines should be when draw
    };

    // -- MULTISAMPLING --
    VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT, // Number of samples to use per fragment
        .sampleShadingEnable = VK_FALSE, // Enabling multisampling shading or not
    };

    // -- BLENDING --
    // Blending decides how to blend a new colour being written to a fragment, with the old value

    // Blend Attachment State (how blending is handled)
    VkPipelineColorBlendAttachmentState colorBlendAttachmentState{
            .blendEnable = VK_TRUE, // Enable blending
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT // Colours to apply blending to
    };

    // Blending uses equation: (srcColorBlendFactor * new colour) colorBlendOp (dstColorBlendFactor * old colour)
    colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;

    // Summarised: (VK_BLEND_FACTOR_SRC_ALPHA * new colour) + (VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA * old colour)
    //             (new colour alpha * new colour) + ((1 - new colour alpha) * old colour)

    colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
    // Summarised: (1 * new alpha) + (0 * old alpha) = new alpha

    VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE, // Alternative to calculations is to use logical operations
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachmentState
    };

    // -- PIPELINE LAYOUT --
    std::array<VkDescriptorSetLayout, 2> descriptorSetLayouts{
        descriptorSetLayout,
        samplerSetLayout
    };

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

    // Create pipeline layout
    VkResult result = vkCreatePipelineLayout(device_.logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Pipeline Layout");
    }

    // -- DEPTH STENCIL TESTING --
    VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo{};
    depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilStateCreateInfo.depthTestEnable = VK_TRUE; // Enable checking depth to determine fragment write
    depthStencilStateCreateInfo.depthWriteEnable = VK_TRUE; // Enable writing to depth buffer (to replace old values)
    depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS; // Comparison operation that allows an overwrite (is in front)
    depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE; // Depth Bounds Test: Does the depth value exists between two bounds
    depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE; // Enable Stencil Test

    // -- GRAPHICS PIPELINE CREATION --
    VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2, // Number of shader stages
        .pStages = shaderStages, // List of shader stages
        .pVertexInputState = &vertexInputStateCreateInfo, // All the fixed functions pipeline states
        .pInputAssemblyState = &inputAssemblyStateCreateInfo,
        .pViewportState = &viewportStateCreateInfo,
        .pRasterizationState = &rasterizationStateCreateInfo,
        .pMultisampleState = &multisampleStateCreateInfo,
        .pDepthStencilState = &depthStencilStateCreateInfo,
        .pColorBlendState = &colorBlendStateCreateInfo,
        .pDynamicState = nullptr,
        .layout = pipelineLayout, // Pipeline layout pipeline should use
        .renderPass = renderPass_, // Render pass description the pipeline is compatible with
        .subpass = 0, // Subpass of render pass to use witch pipeline
    };

    // Pipeline Derivatives : Can create multiple pipelines that derive from another for optimisation
    graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE; // Existing pipeline to derive from
    graphicsPipelineCreateInfo.basePipelineIndex = -1; // or index of pipeline being created derive from (in case creating multiple at once)

    // Create Graphics Pipeline
    result = vkCreateGraphicsPipelines(device_.logicalDevice, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &graphicsPipeline_);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create a graphics pipeline");
    }

    // Destroy Shader Modules, no longer needed after Pipeline created
    vkDestroyShaderModule(device_.logicalDevice, vertShaderModule, nullptr);
    vkDestroyShaderModule(device_.logicalDevice, fragShaderModule, nullptr);

    // CREATE SECOND PASS PIPELINE
    // Second pass shaders
    auto secondVertexShaderCode = readFile("../shaders/second.vert.spv");
    auto secondFragmentShaderCode = readFile("../shaders/second.frag.spv");

    // Build shaders
    VkShaderModule secondVertexShaderModule = createShaderModule(secondVertexShaderCode);
    VkShaderModule secondFragmentShaderModule = createShaderModule(secondFragmentShaderCode);

    // Set new shaders
    vertexShaderCreateInfo.module = secondVertexShaderModule;
    fragmentShaderCreateInfo.module = secondFragmentShaderModule;

    VkPipelineShaderStageCreateInfo secondShaderStages[] = { vertexShaderCreateInfo, fragmentShaderCreateInfo };

    // No vertex data for second pass
    vertexInputStateCreateInfo.vertexBindingDescriptionCount = 0;
    vertexInputStateCreateInfo.pVertexBindingDescriptions = nullptr;
    vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 0;
    vertexInputStateCreateInfo.pVertexAttributeDescriptions = nullptr;

    // Don't want to write to depth buffer
    depthStencilStateCreateInfo.depthWriteEnable = VK_FALSE;

    // Create new pipleine layout
    VkPipelineLayoutCreateInfo secondPipelineLayoutCreateInfo{};
    secondPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    secondPipelineLayoutCreateInfo.setLayoutCount = 1;
    secondPipelineLayoutCreateInfo.pSetLayouts = &inputSetLayout;
    secondPipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    secondPipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

    result = vkCreatePipelineLayout(device_.logicalDevice, &secondPipelineLayoutCreateInfo, nullptr, &secondPipeLineLayout);

    if (result != VK_SUCCESS) throw std::runtime_error("Failed to created a Pipeline Layout");

    graphicsPipelineCreateInfo.pStages = secondShaderStages;	// Update second shader stage list
    graphicsPipelineCreateInfo.layout = secondPipeLineLayout;	// Change pipeline layout for input attachment descriptor sets
    graphicsPipelineCreateInfo.subpass = 1;						// Use second subpass

    // Create second pipeline
    result = vkCreateGraphicsPipelines(device_.logicalDevice, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &secondPipeline);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a Graphics Pipeline!");
    }

    // Destroy second shader modules
    vkDestroyShaderModule(device_.logicalDevice, secondFragmentShaderModule, nullptr);
    vkDestroyShaderModule(device_.logicalDevice, secondVertexShaderModule, nullptr);
}

void VulkanRenderer::createRenderPass() {
    // ATTACHMENTS
    // SUBPASS 1 ATTACHMENTS + REFERENCES (INPUT ATTACHMENTS)

    // Colour Attachment (Input)
    VkAttachmentDescription colourAttachment{};
    colourAttachment.format = chooseSupportedFormat(
            { VK_FORMAT_R8G8B8A8_UNORM },
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    colourAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colourAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colourAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colourAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colourAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colourAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colourAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Depth Attachment (Input)
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = chooseSupportedFormat(
            { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT );
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Color Attachment (Input) Reference
    VkAttachmentReference colourAttachmentReference{};
    colourAttachmentReference.attachment = 1;
    colourAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Depth Attachment (Input) Reference
    VkAttachmentReference depthAttachmentReference{};
    depthAttachmentReference.attachment = 2;
    depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Array of subpasses
    std::array<VkSubpassDescription, 2> subpasses{};

    // Set up Subpass 1
    subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // Pipeline type subpass is to be bound to
    subpasses[0].colorAttachmentCount = 1;
    subpasses[0].pColorAttachments = &colourAttachmentReference;
    subpasses[0].pDepthStencilAttachment = &depthAttachmentReference;

    // SUBPASS 2 ATTACHMENTS + REFERENCES

    // Swapchain Colour attachment
    VkAttachmentDescription swapchainColourAttachment{
        .format = swapChainImageFormat_, // Format to use for attachment
        .samples = VK_SAMPLE_COUNT_1_BIT, // Number of samples to write for multisampling
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, // Describes what to do with attachment before rendering
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE, // Describes what to do with attachment after rendering
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, // Describes what to do with stencil before rendering
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, // Describes what to do with stencil before rendering
    };

    // Framebuffer data will be stored as an image, but images can be given different data layouts
    // to give optimal use for certain operations
    swapchainColourAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Image data layout before render pass starts
    swapchainColourAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // Image data layout after render pass(to change to)

    // Attachment reference uses an attachment index that refers to index in an attachment list passed to renderPassCreateInfo
    VkAttachmentReference swapchainColourAttachmentReference{
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    // References to Attachments that subpass will take input from
    std::array<VkAttachmentReference, 2> inputReferences{};
    inputReferences[0].attachment = 1;
    inputReferences[0].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    inputReferences[1].attachment = 2;
    inputReferences[1].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Set up Subpass 2
    subpasses[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // Pipeline type subpass is to be bound to
    subpasses[1].colorAttachmentCount = 1;
    subpasses[1].pColorAttachments = &swapchainColourAttachmentReference;
    subpasses[1].inputAttachmentCount = static_cast<uint32_t>(inputReferences.size());
    subpasses[1].pInputAttachments = inputReferences.data();

    // SUBPASS DEPENDENCIES

    // Need to determine when layout transitions occur using dependencies
    std::array<VkSubpassDependency, 3> subpassDependencies{};

    // Conversion from VK_IMAGE_LAYOUT_UNDEFINED to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    subpassDependencies[0] = {
            .srcSubpass = VK_SUBPASS_EXTERNAL, // Subpass index (VK_SUBPASS_EXTERNAL = Special value meaning outside of renderpass )
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = 0
    };

    // Subpass 1 layout (colour/depth) to Subpass 2 layout (shader read)
    subpassDependencies[1] = {
            .srcSubpass = 0,
            .dstSubpass = 1,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .dependencyFlags = 0
    };

    // Conversion from VK_IMAGE_LAYOUT_UNDEFINED to VK_IMAGE_LAYOUT_COLOR_SRC_KHR
    subpassDependencies[2] = {
            .srcSubpass = 0,
            .dstSubpass = VK_SUBPASS_EXTERNAL,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
            .dependencyFlags = 0
    };

    std::array<VkAttachmentDescription, 3> renderPassAttachments{
        swapchainColourAttachment, colourAttachment, depthAttachment
    };

    VkRenderPassCreateInfo renderPassCreateInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = static_cast<uint32_t>(renderPassAttachments.size()),
        .pAttachments = renderPassAttachments.data(),
        .subpassCount = static_cast<uint32_t>(subpasses.size()),
        .pSubpasses = subpasses.data(),
        .dependencyCount = static_cast<uint32_t>(subpassDependencies.size()),
        .pDependencies = subpassDependencies.data()
    };

    VkResult result = vkCreateRenderPass(device_.logicalDevice, &renderPassCreateInfo, nullptr, &renderPass_);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass");
    }
}

void VulkanRenderer::createDescriptorSetLayout() {
    // UNIFORM VALUES DESCRIPTOR SET LAYOUT
    // VP Binding info
    VkDescriptorSetLayoutBinding vpLayoutBinding{};
    vpLayoutBinding.binding = 0; // Binding point in shader (designated by binding number in shader)
    vpLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; // Type of descriptor (uniform, dynamic uniform, image sampler, etc)
    vpLayoutBinding.descriptorCount = 1; // Number of descriptors for binding
    vpLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; // Shader stage to bind to
    vpLayoutBinding.pImmutableSamplers = nullptr; // For Texture: Can make sampler data unchangeable (immutable) by specifying in layout

    // Model Binding Info
//    VkDescriptorSetLayoutBinding modelLayoutBinding{};
//    modelLayoutBinding.binding = 1;
//    modelLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
//    modelLayoutBinding.descriptorCount = 1;
//    modelLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
//    modelLayoutBinding.pImmutableSamplers = nullptr;

    std::vector<VkDescriptorSetLayoutBinding> layoutBindings{
            vpLayoutBinding
    };

    // Create Descriptor Set Layout with given bindings
    VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
    layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutCreateInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size()); // Number of binding infos
    layoutCreateInfo.pBindings = layoutBindings.data(); // Array of binding infos

    // Create Descriptor Set Layout
    VkResult result = vkCreateDescriptorSetLayout(device_.logicalDevice, &layoutCreateInfo, nullptr,
                                                  &descriptorSetLayout);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create a Descriptor Set Layout");
    }

    // CREATE TEXTURE SAMPLER DESCRIPTOR SET LAYOUT
    // Texture binding info
    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 0;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    samplerLayoutBinding.pImmutableSamplers = nullptr;

    // Create a Descriptor Set Layout with given binding for texture
    VkDescriptorSetLayoutCreateInfo textureLayoutCreateInfo{};
    textureLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    textureLayoutCreateInfo.bindingCount = 1;
    textureLayoutCreateInfo.pBindings = &samplerLayoutBinding;

    // Create Descriptor Set Layout
    result = vkCreateDescriptorSetLayout(device_.logicalDevice, &textureLayoutCreateInfo, nullptr, &samplerSetLayout);

    if (result != VK_SUCCESS) throw std::runtime_error("Failed to create a Descriptor Set Layout");

    // CREATE INPUT ATTACHMENT IMAGE DESCRIPTOR SET LAYOUT
    // Colour input binding
    VkDescriptorSetLayoutBinding colourInputLayoutBinding{};
    colourInputLayoutBinding.binding = 0;
    colourInputLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    colourInputLayoutBinding.descriptorCount = 1;
    colourInputLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding depthInputLayoutBinding{};
    depthInputLayoutBinding.binding = 1;
    depthInputLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    depthInputLayoutBinding.descriptorCount = 1;
    depthInputLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // Array of input attachments bindings
    std::vector<VkDescriptorSetLayoutBinding> inputBindings = {
            colourInputLayoutBinding,
            depthInputLayoutBinding
    };

    // Create a descriptor set layout for input attachments
    VkDescriptorSetLayoutCreateInfo inputLayoutCreateInfo{};
    inputLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    inputLayoutCreateInfo.bindingCount = static_cast<uint32_t>(inputBindings.size());
    inputLayoutCreateInfo.pBindings = inputBindings.data();

    result = vkCreateDescriptorSetLayout(device_.logicalDevice, &inputLayoutCreateInfo, nullptr, &inputSetLayout);

    if (result != VK_SUCCESS) throw std::runtime_error("Failed to create a Descriptor Set Layout");
}

void VulkanRenderer::createPushConstantRange() {
    // Define push constant values (no 'create' needed)
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; // shader stage push constant will go to
    pushConstantRange.offset = 0; // Offset into given data to pass to push constant
    pushConstantRange.size = sizeof(Model); // size of data being pass
}

void VulkanRenderer::createColourBufferImage() {
    // Resize supported format for colour attachment
    colourBufferImages.resize(swapChainImages_.size());
    colourBufferImageView.resize(swapChainImages_.size());
    colourBufferImageMemory.resize(swapChainImages_.size());

    // Get supported format for colour attachment
    VkFormat colourFormat = chooseSupportedFormat(
            { VK_FORMAT_R8G8B8A8_UNORM },
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            );

    for (size_t i = 0; i < swapChainImages_.size(); ++i) {
        // Create Colour Buffer Image
        colourBufferImages[i] = createImage(swapChainExtent_.width, swapChainExtent_.height,
                                            colourFormat,
                                            VK_IMAGE_TILING_OPTIMAL,
                                            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
                                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                            &colourBufferImageMemory[i]);

        // Create Colour Image View
        colourBufferImageView[i] = createImageView(colourBufferImages[i], colourFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

void VulkanRenderer::createDepthBufferImage() {
    depthBufferImages.resize(swapChainImages_.size());
    depthBufferImageView.resize(swapChainImages_.size());
    depthBufferImageMemory.resize(swapChainImages_.size());

    // Get supported format for depth buffer
    VkFormat depthForamt = chooseSupportedFormat(
            { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT );


    for (size_t i = 0; i < swapChainImages_.size(); ++i) {
        // Create depth buffer image
        depthBufferImages[i] = createImage(swapChainExtent_.width, swapChainExtent_.height,
                                       depthForamt,
                                       VK_IMAGE_TILING_OPTIMAL,
                                       VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                       &depthBufferImageMemory[i]);

        // Create Depth Buffer Image View
        depthBufferImageView[i] = createImageView(depthBufferImages[i], depthForamt, VK_IMAGE_ASPECT_DEPTH_BIT);
    }
}

void VulkanRenderer::createFramebuffers() {
    // Resize framebuffer count to equal swap chain image count
    swapChainFramebuffers_.resize(swapChainImages_.size());

    // Create a framebuffer for each swap chain image
    for (size_t i = 0; i < swapChainFramebuffers_.size(); i++) {
        std::array<VkImageView, 3> attachments = {
                swapChainImages_[i].imageView,
                colourBufferImageView[i],
                depthBufferImageView[i]
        };

        VkFramebufferCreateInfo framebufferCreateInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = renderPass_,
            .attachmentCount = static_cast<uint32_t>(attachments.size()),
            .pAttachments = attachments.data(), // List of attachments (1:1 with Render Pass)
            .width = swapChainExtent_.width,
            .height = swapChainExtent_.height,
            .layers = 1
        };

        VkResult result = vkCreateFramebuffer(device_.logicalDevice, &framebufferCreateInfo, nullptr, &swapChainFramebuffers_[i]);

        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create a Framebuffer");
        }
    }
}

void VulkanRenderer::createCommandPool() {
    // Get indices of queue family from device
    QueueFamilyIndices queueFamilyIndices = getQueueFamilies(device_.physicalDevice);

    VkCommandPoolCreateInfo commandPoolCreateInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamilyIndices.graphicsFamily.value()
    };

    // Create a Graphics Queue Family Command Pool
    VkResult result = vkCreateCommandPool(device_.logicalDevice, &commandPoolCreateInfo, nullptr, &graphicsCommandPool);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create a Command Pool");
    }
}

void VulkanRenderer::createCommandBuffers() {
    // Resize command buffer count to have 1 for each framebuffer
    commandBuffers_.resize(swapChainFramebuffers_.size());

    VkCommandBufferAllocateInfo commandBufferAllocateInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = graphicsCommandPool,
        .commandBufferCount = static_cast<uint32_t>(commandBuffers_.size())
    };

    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;  // VK_COMMAND_BUFFER_LEVEL_PRIMARY : Buffer you submit directly to queue. Cant be called by another buffers.
                                                                        // VK_COMMAND_BUFFER_LEVEL_SECONDARY : Buffer cant be called directly. Can be called from other buffers via "vkCmdExecuteCommands" when recording commands in primary buffer

    VkResult result = vkAllocateCommandBuffers(device_.logicalDevice, &commandBufferAllocateInfo, commandBuffers_.data());

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate Command Buffers");
    }
}

void VulkanRenderer::createSynchronisation() {
    imageAvailable.resize(MAX_FRAME_DRAWS);
    renderFinished.resize(MAX_FRAME_DRAWS);
    drawFences.resize(MAX_FRAME_DRAWS);

    // Semaphore creation information
    VkSemaphoreCreateInfo semaphoreCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    VkFenceCreateInfo fenceCreateInfo{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    for (size_t i = 0; i < MAX_FRAME_DRAWS; i++) {
        if (vkCreateSemaphore(device_.logicalDevice, &semaphoreCreateInfo, nullptr, &imageAvailable[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device_.logicalDevice, &semaphoreCreateInfo, nullptr, &renderFinished[i]) != VK_SUCCESS ||
            vkCreateFence(device_.logicalDevice, &fenceCreateInfo, nullptr, &drawFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create a Semaphore and/or Fence");
        }
    }
}

void VulkanRenderer::createUniformBuffers() {
    // ViewProjection buffer size
    VkDeviceSize vpBufferSize = sizeof(UboViewProjection);

    // Model buffer size
//    VkDeviceSize modelBufferSize = modelUniformAlignment * MAX_OBJECTS;

    // One uniform buffer for each image (and by extension, command buffer)
    vpUniformBuffer.resize(swapChainImages_.size());
    vpUniformBufferMemory.resize(swapChainImages_.size());

    modelDUniformBuffer.resize(swapChainImages_.size());
    modelDUniformBufferMemory.resize(swapChainImages_.size());

    // Create Uniform buffers
    for (size_t i = 0; i < swapChainImages_.size(); ++i) {
        createBuffer(device_.physicalDevice, device_.logicalDevice, vpBufferSize,
                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     &vpUniformBuffer[i], &vpUniformBufferMemory[i]);

//        createBuffer(device_.physicalDevice, device_.logicalDevice, modelBufferSize,
//                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
//                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
//                     &modelDUniformBuffer[i], &modelDUniformBufferMemory[i]);
    }
}

void VulkanRenderer::createDescriptorPool() {
    // CREATE UNIFORM DESCRIPTOR POOL
    // Type of descriptors + how many DESCRIPTORS, not Descriptor Sets (combined makes the pool size)
    // ViewProjectionPool
    VkDescriptorPoolSize vpPoolSize{};
    vpPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    vpPoolSize.descriptorCount = static_cast<uint32_t>(vpUniformBuffer.size());

    // Model Pool (DYNAMIC)
//    VkDescriptorPoolSize modelPoolSize{};
//    modelPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
//    modelPoolSize.descriptorCount = static_cast<uint32_t>(modelDUniformBuffer.size());

    std::vector<VkDescriptorPoolSize> descriptorPoolSizes{
        vpPoolSize,
    };

    // Data to create Descriptor Pool
    VkDescriptorPoolCreateInfo poolCreateInfo{};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCreateInfo.maxSets = static_cast<uint32_t>(swapChainImages_.size()); // Maximum number of Descriptor Sets that can be created from pool
    poolCreateInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size()); // Amount of Pool Sizes being passed
    poolCreateInfo.pPoolSizes = descriptorPoolSizes.data(); // Pool Sizes to create pool with

    // Create Descriptor Pool
    VkResult result = vkCreateDescriptorPool(device_.logicalDevice, &poolCreateInfo, nullptr, &descriptorPool);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create a Descriptor Pool");
    }

    // CREATE SAMPLER DESCRIPTOR POOL
    // Texture sampler pool
    VkDescriptorPoolSize samplerPoolSizer{};
    samplerPoolSizer.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerPoolSizer.descriptorCount = MAX_OBJECTS;

    VkDescriptorPoolCreateInfo samplerPoolCreateInfo{};
    samplerPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    samplerPoolCreateInfo.maxSets = MAX_OBJECTS;
    samplerPoolCreateInfo.poolSizeCount = 1;
    samplerPoolCreateInfo.pPoolSizes = &samplerPoolSizer;

    result = vkCreateDescriptorPool(device_.logicalDevice, &samplerPoolCreateInfo, nullptr, &samplerDescriptorPool);

    if (result != VK_SUCCESS) throw std::runtime_error("Failed to create a Descriptor Pool");

    // CREATE INPUT ATTACHMENT DESCRIPTOR POOL
    // Colour Attachment Pool Size
    VkDescriptorPoolSize colourInputPoolSize = {};
    colourInputPoolSize.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    colourInputPoolSize.descriptorCount = static_cast<uint32_t>(colourBufferImageView.size());

    // Depth Attachment Pool Size
    VkDescriptorPoolSize depthInputPoolSize = {};
    depthInputPoolSize.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    depthInputPoolSize.descriptorCount = static_cast<uint32_t>(depthBufferImageView.size());

    std::vector<VkDescriptorPoolSize> inputPoolSizes = { colourInputPoolSize, depthInputPoolSize };

    // Create input attachment pool
    VkDescriptorPoolCreateInfo inputPoolCreateInfo = {};
    inputPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    inputPoolCreateInfo.maxSets = swapChainImages_.size();
    inputPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(inputPoolSizes.size());
    inputPoolCreateInfo.pPoolSizes = inputPoolSizes.data();

    result = vkCreateDescriptorPool(device_.logicalDevice, &inputPoolCreateInfo, nullptr, &inputDescriptorPool);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a Descriptor Pool!");
    }
}

void VulkanRenderer::createDescriptorSets() {
    // Resize Descriptor Set list so one for every buffer
    descriptorSets.resize(swapChainImages_.size());

    std::vector<VkDescriptorSetLayout> setLayouts(swapChainImages_.size(), descriptorSetLayout);

    // Descriptor Set Allocation Info
    VkDescriptorSetAllocateInfo setAllocInfo{};
    setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setAllocInfo.descriptorPool = descriptorPool; // Pool to allocate Descriptor Set from
    setAllocInfo.descriptorSetCount = static_cast<uint32_t>(swapChainImages_.size()); // Number of sets to allocate
    setAllocInfo.pSetLayouts = setLayouts.data(); // Layouts to use to allocate sets (1:1 relationship)

    // Allocate descriptor sets (multiple)
    VkResult result = vkAllocateDescriptorSets(device_.logicalDevice, &setAllocInfo, descriptorSets.data());

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate Descriptor Sets");
    }

    // Update all of descriptor set buffer bindings
    for (size_t i = 0; i < swapChainImages_.size(); ++i) {
        // VIEWPROJECTIOON DESCRIPTOR
        // Buffer info and data offset info
        VkDescriptorBufferInfo vpBufferInfo{};
        vpBufferInfo.buffer = vpUniformBuffer[i]; // Buffer to get data from
        vpBufferInfo.offset = 0; // Position of start of data
        vpBufferInfo.range = sizeof(UboViewProjection); // Size of data

        // Data about connection between binding and buffer
        VkWriteDescriptorSet vpSetWrite{};
        vpSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        vpSetWrite.dstSet = descriptorSets[i];	// Descriptor Set to update
        vpSetWrite.dstBinding = 0;	// Binding to update (matches with binding on layout/shader)
        vpSetWrite.dstArrayElement = 0; // Index in array to update
        vpSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; // Type of descriptor
        vpSetWrite.descriptorCount = 1; // Amount to update
        vpSetWrite.pBufferInfo = &vpBufferInfo; // Information about buffer data to bind

        // MODEL DESCRIPTOR
//        VkDescriptorBufferInfo modelBufferInfo{};
//        modelBufferInfo.buffer = modelDUniformBuffer[i];
//        modelBufferInfo.offset = 0;
//        modelBufferInfo.range = modelUniformAlignment;
//
//        VkWriteDescriptorSet modelSetWrite{};
//        modelSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
//        modelSetWrite.dstSet = descriptorSets[i];
//        modelSetWrite.dstBinding = 1;
//        modelSetWrite.dstArrayElement = 0;
//        modelSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
//        modelSetWrite.descriptorCount = 1;
//        modelSetWrite.pBufferInfo = &modelBufferInfo;

        std::vector<VkWriteDescriptorSet> setWrites{
                vpSetWrite
        };

        // Update the descriptor sets with new buffer/binding info
        vkUpdateDescriptorSets(device_.logicalDevice, static_cast<uint32_t>(setWrites.size()), setWrites.data(),
                               0, nullptr);
    }
}

void VulkanRenderer::createTextureSampler() {
    // Sampler creation info
    VkSamplerCreateInfo samplerCreateInfo{};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR; // How to render when image is magnified on screen
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR; // How to render when image is minified on screen
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT; // How to handle texture wrap in U (x) direction
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT; // How to handle texture wrap in V (y) direction
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT; // How to handle texture wrap in W (z) direction
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK; // Border beyond texture (only works for border clamp)
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE; // Whether coords should be normalize (between 0 and 1)
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR; // Mipmap interpolation mode
    samplerCreateInfo.mipLodBias = 0.0f; // Level of details bias for mip level
    samplerCreateInfo.minLod = 0.0f; // Minimum Level of Detail to pick mip level
    samplerCreateInfo.maxLod = 0.0f; // Maximum Level of Detail to pick mip level
    samplerCreateInfo.anisotropyEnable = VK_TRUE; // Enable Anisotropy
    samplerCreateInfo.maxAnisotropy = 16; // Anisotropy sample level

    VkResult result = vkCreateSampler(device_.logicalDevice, &samplerCreateInfo, nullptr, &textureSampler);

    if (result != VK_SUCCESS) throw std::runtime_error("Failed to crate a Texture Sampler");
}

void VulkanRenderer::createInputDescriptorSets() {
    // Resize array to hold descriptor set for each swap chain image
    inputDescriptorSets.resize(swapChainImages_.size());

    // Fill array of layouts ready for set creation
    std::vector<VkDescriptorSetLayout> setLayouts(swapChainImages_.size(), inputSetLayout);

    // Input Attachment Descriptor Set Allocation Info
    VkDescriptorSetAllocateInfo setAllocInfo = {};
    setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setAllocInfo.descriptorPool = inputDescriptorPool;
    setAllocInfo.descriptorSetCount = static_cast<uint32_t>(swapChainImages_.size());
    setAllocInfo.pSetLayouts = setLayouts.data();

    // Allocate Descriptor Sets
    VkResult result = vkAllocateDescriptorSets(device_.logicalDevice, &setAllocInfo, inputDescriptorSets.data());
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate Input Attachment Descriptor Sets!");
    }

    // Update each descriptor set with input attachment
    for (size_t i = 0; i < swapChainImages_.size(); i++)
    {
        // Colour Attachment Descriptor
        VkDescriptorImageInfo colourAttachmentDescriptor = {};
        colourAttachmentDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        colourAttachmentDescriptor.imageView = colourBufferImageView[i];
        colourAttachmentDescriptor.sampler = VK_NULL_HANDLE;

        // Colour Attachment Descriptor Write
        VkWriteDescriptorSet colourWrite = {};
        colourWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        colourWrite.dstSet = inputDescriptorSets[i];
        colourWrite.dstBinding = 0;
        colourWrite.dstArrayElement = 0;
        colourWrite.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        colourWrite.descriptorCount = 1;
        colourWrite.pImageInfo = &colourAttachmentDescriptor;

        // Depth Attachment Descriptor
        VkDescriptorImageInfo depthAttachmentDescriptor = {};
        depthAttachmentDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        depthAttachmentDescriptor.imageView = depthBufferImageView[i];
        depthAttachmentDescriptor.sampler = VK_NULL_HANDLE;

        // Depth Attachment Descriptor Write
        VkWriteDescriptorSet depthWrite = {};
        depthWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        depthWrite.dstSet = inputDescriptorSets[i];
        depthWrite.dstBinding = 1;
        depthWrite.dstArrayElement = 0;
        depthWrite.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        depthWrite.descriptorCount = 1;
        depthWrite.pImageInfo = &depthAttachmentDescriptor;

        // List of input descriptor set writes
        std::vector<VkWriteDescriptorSet> setWrites = { colourWrite, depthWrite };

        // Update descriptor sets
        vkUpdateDescriptorSets(device_.logicalDevice, static_cast<uint32_t>(setWrites.size()), setWrites.data(), 0, nullptr);
    }
}

void VulkanRenderer::updateUniformBuffers(uint32_t imageIndex) {
    // Copy VP data
    void* data;
    vkMapMemory(device_.logicalDevice, vpUniformBufferMemory[imageIndex], 0, sizeof(UboViewProjection),
                0, &data);
    memcpy(data, &uboViewProjection, sizeof(UboViewProjection));
    vkUnmapMemory(device_.logicalDevice, vpUniformBufferMemory[imageIndex]);

    // Copy Model data
    /*for (size_t i = 0; i < meshList.size(); ++i) {
        auto* model_ = (UboModel*)((uint64_t)modelTransferSpace + (i * modelUniformAlignment));
        *model_ = meshList[i].getUboModel();
    }

    // Map the list of model_ data
    vkMapMemory(device_.logicalDevice, modelDUniformBufferMemory[imageIndex], 0,
                modelUniformAlignment * meshList.size(), 0, &data);
    memcpy(data, modelTransferSpace, modelUniformAlignment * meshList.size());
    vkUnmapMemory(device_.logicalDevice, modelDUniformBufferMemory[imageIndex]); */
}

void VulkanRenderer::recordCommands(uint32_t currentImage) {
    // Information about how to begin each command buffer
    VkCommandBufferBeginInfo bufferBeginInfo = {};
    bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    // Information about how to begin a render pass (only needed for graphical applications)
    VkRenderPassBeginInfo renderPassBeginInfo = {};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.renderPass = renderPass_;							// Render Pass to begin
    renderPassBeginInfo.renderArea.offset = { 0, 0 };						// Start point of render pass in pixels
    renderPassBeginInfo.renderArea.extent = swapChainExtent_;				// Size of region to run render pass on (starting at offset)

    std::array<VkClearValue, 3> clearValues = {};
    clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
    clearValues[1].color = { 0.6f, 0.65f, 0.4f, 1.0f };
    clearValues[2].depthStencil.depth = 1.0f;

    renderPassBeginInfo.pClearValues = clearValues.data();					// List of clear values
    renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());

    renderPassBeginInfo.framebuffer = swapChainFramebuffers_[currentImage];

    // Start recording commands to command buffer!
    VkResult result = vkBeginCommandBuffer(commandBuffers_[currentImage], &bufferBeginInfo);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to start recording a Command Buffer!");
    }

    // Begin Render Pass
    vkCmdBeginRenderPass(commandBuffers_[currentImage], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        // Bind Pipeline to be used in render pass
        vkCmdBindPipeline(commandBuffers_[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline_);

        for (size_t j = 0; j < modelList.size(); j++) {
            MeshModel thisModel = modelList[j];

            vkCmdPushConstants(
                    commandBuffers_[currentImage],
                    pipelineLayout,
                    VK_SHADER_STAGE_VERTEX_BIT,		// Stage to push constants to
                    0,								// Offset of push constants to update
                    sizeof(Model),					// Size of data being pushed
                    &thisModel.getModel());			// Actual data being pushed (can be array)

            for (size_t k = 0; k < thisModel.getMeshCount(); k++) {
                VkBuffer vertexBuffers[] = { thisModel.getMesh(k)->getVertexBuffer() };					// Buffers to bind
                VkDeviceSize offsets[] = { 0 };												// Offsets into buffers being bound
                vkCmdBindVertexBuffers(commandBuffers_[currentImage], 0, 1, vertexBuffers, offsets);	// Command to bind vertex buffer before drawing with them

                // Bind mesh index buffer, with 0 offset and using the uint32 type
                vkCmdBindIndexBuffer(commandBuffers_[currentImage], thisModel.getMesh(k)->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

                // Dynamic Offset Amount
                // uint32_t dynamicOffset = static_cast<uint32_t>(modelUniformAlignment) * j;

                // "Push" constants to given shader stage directly (no buffer)


                std::array<VkDescriptorSet, 2> descriptorSetGroup = { descriptorSets[currentImage],
                                                                      samplerDescriptorSets[thisModel.getMesh(k)->getTextureId()] };

                // Bind Descriptor Sets
                vkCmdBindDescriptorSets(commandBuffers_[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
                                        0, static_cast<uint32_t>(descriptorSetGroup.size()), descriptorSetGroup.data(), 0, nullptr);

                // Execute pipeline
                vkCmdDrawIndexed(commandBuffers_[currentImage], thisModel.getMesh(k)->getIndexCount(), 1, 0, 0, 0);
            }
        }

        // Start second subpass
        vkCmdNextSubpass(commandBuffers_[currentImage], VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(commandBuffers_[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, secondPipeline);
        vkCmdBindDescriptorSets(commandBuffers_[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, secondPipeLineLayout,
                                0, 1, &inputDescriptorSets[currentImage], 0, nullptr);

        vkCmdDraw(commandBuffers_[currentImage], 3, 1, 0, 0);

    // End Render Pass
    vkCmdEndRenderPass(commandBuffers_[currentImage]);

    // Stop recording to command buffer
    result = vkEndCommandBuffer(commandBuffers_[currentImage]);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to stop recording a Command Buffer!");
    }
}

void VulkanRenderer::getPhysicalDevice() {
    spdlog::info("[Vulkan-Renderer] Get Physical Device");

    // Enumerate physical devices the VkInstances can access
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);

    // If no devices are available, then none support vulkan
    if (deviceCount == 0) {
        throw std::runtime_error("Can't find GPU that support instance");
    }

    // Get list of physical devices
    std::vector<VkPhysicalDevice> deviceList(deviceCount);
    vkEnumeratePhysicalDevices(instance_, &deviceCount, deviceList.data());

    for (const auto& device : deviceList) {
        if (checkDeviceSuitable(device)) {
            device_.physicalDevice = device; break;
        }
    }

    if (device_.physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to find a suitable GPU!");
    }

    // Get properties of our new device
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device_.physicalDevice, &deviceProperties);

//    minUniformBufferOffset_ = deviceProperties.limits.minUniformBufferOffsetAlignment;
}

void VulkanRenderer::allocateDynamicBufferTransferSpace() {
    // Calculate alignment of model_ data
//    modelUniformAlignment = (sizeof(UboModel) + minUniformBufferOffset_ - 1)
//                            & ~(minUniformBufferOffset_ - 1);

    // Create space in memory to hold dynamic buffer that is aligned to our required alignment and holds MAX_OBJECTS
//    modelTransferSpace = (UboModel*)std::aligned_alloc(modelUniformAlignment, modelUniformAlignment * MAX_OBJECTS);
}

bool VulkanRenderer::checkInstanceSupport(std::vector<const char *> *checkExtensions) {
    uint32_t extensionCount = 0;

    // Need to get number of extensions to create array of correct size to hold extensions
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    // Create a list of VkExtensionsProperties using count
    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

    // Check if extensions are in list of available extensions
    for (const auto& checkExtension : *checkExtensions ) {
        bool hasExtension = false;

        for (const auto& extension : extensions) {
            if (std::strcmp(checkExtension, extension.extensionName) != 0) {
                hasExtension = true; break;
            }
        }

        if (!hasExtension) return false;
    }

    return true;
}

bool VulkanRenderer::checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionsCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionsCount, nullptr);

    if (extensionsCount == 0) return false;

    std::vector<VkExtensionProperties> extensions(extensionsCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionsCount, extensions.data());

    bool hasExtension = false;

    for (const auto& deviceExtension : deviceExtensions) {
        for (const auto& extension : extensions) {
            if (strcmp(deviceExtension, extension.extensionName) == 0) {
                hasExtension = true; break;
            }
        }
    }

    return hasExtension;
}

bool VulkanRenderer::checkDeviceSuitable(VkPhysicalDevice device) {
    // Information about the device itself (ID, name. type, vendor, etc)
//    VkPhysicalDeviceProperties deviceProperties;
//    vkGetPhysicalDeviceProperties(device, &deviceProperties);

    // Information about what the device can do (geo shader, tess shader, wide lines, etc)
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

    QueueFamilyIndices indices = getQueueFamilies(device);
    bool extensionSupported = checkDeviceExtensionSupport(device);
    bool swapChainValid = false;

    if (extensionSupported) {
        SwapChainDetails swapChainDetails = getSwapChainDetails(device);
        swapChainValid = !swapChainDetails.presentationModes.empty() && !swapChainDetails.formats.empty();
    }

    return indices.isValid() && extensionSupported && swapChainValid && deviceFeatures.samplerAnisotropy;
}

QueueFamilyIndices VulkanRenderer::getQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices{};

    // Get all Queue Family Property info for the given device
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilyList(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilyList.data());

    // Go through each family and check if it has a least 1 of the required types of queue
    int i = 0;

    for (const auto& queueFamily : queueFamilyList) {
        // First check if queue family has ay least 1 queue in that family (cloud have no queues)
        // Queue can be multiply types defined through bitfield. Need to bitwise AND with VK_QUEUE_*_BIT to check if
        // has required type
        if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i; // If finally queue is valid, then get index

            // Check if Family supports presentation
            VkBool32 presentationSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &presentationSupport);

            indices.presentationFamily = i;

            // Check if queue family indices are in valid state, stop searching if so
            // Check if queue is presentation type (can be both graphics and presentation)
            if (indices.isValid()){
                break;
            }
        }

        i++;
    }

    return indices;
}

SwapChainDetails VulkanRenderer::getSwapChainDetails(VkPhysicalDevice device) {
    SwapChainDetails swapChainDetails;

    // -- CAPABILITIES --
    // Get the surface capabilities for the given surface on the given physical device
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_, &swapChainDetails.surfaceCapabilities);

    // -- FORMATS --
    uint32_t formatsCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatsCount, nullptr);

    // If formats returned, get list of formats
    if (formatsCount != 0) {
        swapChainDetails.formats.resize(formatsCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatsCount, swapChainDetails.formats.data());
    }

    // -- PRESENTATION MODES --
    uint32_t presentationModesCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentationModesCount, nullptr);

    // If presentation modes is returned, get list of presentation modes
    if (presentationModesCount != 0) {
        swapChainDetails.presentationModes.resize(presentationModesCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentationModesCount, swapChainDetails.presentationModes.data());
    }

    return swapChainDetails;
}

// Best format is subjective, but ours will be:
// Format: VK_FORMAT_R8G8B8A8_UNORM
// colorSpace : VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
VkSurfaceFormatKHR VulkanRenderer::chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &formats) {
    // If only one format is available and is undefined, then this mean ALL formats are available(no restrictions)
    if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
        return {VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    }

    // If restricted, search for optimal format
    for (const auto& format : formats) {
        if ((format.format == VK_FORMAT_R8G8B8A8_UNORM || format.format == VK_FORMAT_B8G8R8A8_UNORM)
                && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR ) {
            return format;
        }
    }

    // If can't find optimal format, then just return first format
    return formats[0];
}

VkPresentModeKHR VulkanRenderer::chooseBestPresentationMode(const std::vector<VkPresentModeKHR> &presentationModes) {
    // Look for Mailbox presentation mode
    for (const auto& presentationMode : presentationModes) {
        if (presentationMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return  presentationMode;
        }
    }

    // If can't find, use FIFO as Vulkan spec says it must be present
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanRenderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR &surfaceCapabilities) {
    // If current extent is at numeric limits, then extent can vary. Otherwise, it is the size of the window.
    if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return surfaceCapabilities.currentExtent;
    } else {
        // If value can vary, need to set manually
        // Get window size
        int width, height;
        glfwGetFramebufferSize(window_->window_, &width, &height);

        // Create new extent using window size
        VkExtent2D newExtent{};
        newExtent.width = static_cast<uint32_t>(width);
        newExtent.height = static_cast<uint32_t>(height);

        // Surface also defines max and min, so make sure within boundaries bu clamping value
        newExtent.width = std::max(surfaceCapabilities.minImageExtent.width,
                                   std::min(surfaceCapabilities.maxImageExtent.width, newExtent.width));
        newExtent.height = std::max(surfaceCapabilities.minImageExtent.height,
                                   std::min(surfaceCapabilities.maxImageExtent.height, newExtent.height));

        return newExtent;
    }
}

VkFormat VulkanRenderer::chooseSupportedFormat(const std::vector<VkFormat> &formats, VkImageTiling tiling,
                                               VkFormatFeatureFlags featureFlags) {
    // Loop through options and find compatible one
    for (VkFormat format : formats) {
        // Get the properties for a given format on this device
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(device_.physicalDevice, format, &properties);

        // Depending of tiling choise, need to check for different bit flag
        if (tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & featureFlags) == featureFlags) {
            return format;
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & featureFlags) == featureFlags) {
            return format;
        }
    }

    throw std::runtime_error("Failed to find a matching format");
}

VkImageView VulkanRenderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
    VkImageViewCreateInfo viewCreateInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image, // Image to create view for
        .viewType = VK_IMAGE_VIEW_TYPE_2D, // Type of image (1D, 2D, 3D, Cube, etc)
        .format = format, // Formats of image data
    };

    // Allows remapping of rgba components to other rgba values
    viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    // Subresources allow the view to view only a part of an image
    viewCreateInfo.subresourceRange.aspectMask = aspectFlags; // Which aspect of image to view (e.g. COLOR_BIT for viewing colour)
    viewCreateInfo.subresourceRange.baseMipLevel = 0; // Start mipmap level to view from
    viewCreateInfo.subresourceRange.levelCount = 1; // Number of mipmap levels to view
    viewCreateInfo.subresourceRange.baseArrayLayer = 0; // Start array level to view from
    viewCreateInfo.subresourceRange.layerCount = 1; // Number of array levels to view

    // Create image view and return it
    VkImageView imageview;
    VkResult result = vkCreateImageView(device_.logicalDevice, &viewCreateInfo, nullptr, &imageview);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create an Image View");
    }

    return imageview;
}

VkShaderModule VulkanRenderer::createShaderModule(const std::vector<char> &code) {
    VkShaderModuleCreateInfo shaderModuleCreateInfo{
        .sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize=code.size(),
        .pCode=reinterpret_cast<const uint32_t*>(code.data())
    };

    VkShaderModule shaderModule;
    VkResult result = vkCreateShaderModule(device_.logicalDevice, &shaderModuleCreateInfo, nullptr, &shaderModule);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create a shader module");
    }

    return shaderModule;
}

VkImage VulkanRenderer::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                                    VkImageUsageFlags usageFlags, VkMemoryPropertyFlags propertyFlags,
                                    VkDeviceMemory* imageMemory) {
    // CREATE IMAGE
    // Image create info
    VkImageCreateInfo imageCreateInfo{};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D; // Type of image (1D, 2D or 3D)
    imageCreateInfo.extent.width = width; // Width of Image extent
    imageCreateInfo.extent.height = height; // Height of Image extent
    imageCreateInfo.extent.depth = 1; // Depth of image (just 1, no 3D aspect)
    imageCreateInfo.mipLevels = 1; // Number of mipmap levels
    imageCreateInfo.arrayLayers = 1; // Number of levels in image array
    imageCreateInfo.format = format; // Format type of image
    imageCreateInfo.tiling = tiling; // How image data should be "tiled" (arranged for optimal reading)
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Layout of image data on creation
    imageCreateInfo.usage = usageFlags; // Bit flags defining what image will be use for
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT; // Number of samples for multi-sampling
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // Whether image can be shared between queues

    // Create Image
    VkImage image;
    VkResult result = vkCreateImage(device_.logicalDevice, &imageCreateInfo, nullptr, &image);

    if (result != VK_SUCCESS) throw std::runtime_error("Failed to create an Image");

    // CREATE MEMORY FOR IMAGE

    // Get memory requirements for a type of image
    VkMemoryRequirements memoryRequirements{};
    vkGetImageMemoryRequirements(device_.logicalDevice, image, &memoryRequirements);

    // Allocate Memory using image requirements and user defined properties
    VkMemoryAllocateInfo memoryAllocateInfo{};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = findMemoryTypeIndex(device_.physicalDevice, memoryRequirements.memoryTypeBits,
                                                             propertyFlags);

    result = vkAllocateMemory(device_.logicalDevice, &memoryAllocateInfo, nullptr, imageMemory);

    if (result != VK_SUCCESS) throw std::runtime_error("Failed to allocate memory for image");

    // Connect memory to image
    vkBindImageMemory(device_.logicalDevice, image, *imageMemory, 0);

    return image;
}

int VulkanRenderer::createTextureImage(const std::string &fileName) {
    // Load image file
    int width, height;
    VkDeviceSize imageSize;
    stbi_uc* imageData = loadTextureFile(fileName, &width, &height, &imageSize);

    // Create staging buffer to hold loaded data, ready to copy to device
    VkBuffer imageStagingBuffer;
    VkDeviceMemory imageStagingBufferMemory;

    createBuffer(device_.physicalDevice, device_.logicalDevice, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 &imageStagingBuffer, &imageStagingBufferMemory);

    // Copy image data to staging buffer
    void* data;
    vkMapMemory(device_.logicalDevice, imageStagingBufferMemory, 0, imageSize, 0, &data);
    std::memcpy(data, imageData, static_cast<size_t>(imageSize));
    vkUnmapMemory(device_.logicalDevice, imageStagingBufferMemory);

    // Free original image data
    stbi_image_free(imageData);

    // Create image to hold final texture
    VkImage texImage;
    VkDeviceMemory texImageMemory;

    texImage = createImage(width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                           VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &texImageMemory);

    // COPY DATA TO IMAGE
    // Transition image to be DST for copy operation
    transitionImageLayout(device_.logicalDevice, graphicsQueues_, graphicsCommandPool, texImage,
                          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Copy image data
    copyImageBuffer(device_.logicalDevice, graphicsQueues_, graphicsCommandPool, imageStagingBuffer, texImage,
                    width, height);

    // Transition image to be shader readable for shader usage
    transitionImageLayout(device_.logicalDevice, graphicsQueues_, graphicsCommandPool,
                          texImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // Add texture data to vector for reference
    textureImages.push_back(texImage);
    textureImageMemory.push_back(texImageMemory);

    // Destroy staging buffers
    vkDestroyBuffer(device_.logicalDevice, imageStagingBuffer, nullptr);
    vkFreeMemory(device_.logicalDevice, imageStagingBufferMemory, nullptr);

    // Return index of new texture image
    return static_cast<int>(textureImages.size()) - 1;
}

int VulkanRenderer::createTexture(const std::string &fileName) {
    // Create Texture Image and get its location in array
    int textureImageLoc = createTextureImage(fileName);

    VkImageView imageView = createImageView(textureImages[textureImageLoc], VK_FORMAT_R8G8B8A8_UNORM,
                                            VK_IMAGE_ASPECT_COLOR_BIT);

    // Create Image View and add to list
    textureImageViews.push_back(imageView);

    // Create Texture Descriptor Set
    int descriptorLoc = createTextureDescriptor(imageView);

    // Return location of set witrh texture
    return descriptorLoc;
}

int VulkanRenderer::createTextureDescriptor(VkImageView textureImage) {
    VkDescriptorSet descriptorSet{};

    // Descriptor Set Allocation Info
    VkDescriptorSetAllocateInfo setAllocateInfo{};
    setAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setAllocateInfo.descriptorPool = samplerDescriptorPool;
    setAllocateInfo.descriptorSetCount = 1;
    setAllocateInfo.pSetLayouts = &samplerSetLayout;

    // Allocate Descriptor Set
    VkResult result = vkAllocateDescriptorSets(device_.logicalDevice, &setAllocateInfo, &descriptorSet);

    if (result != VK_SUCCESS) throw std::runtime_error("Failed to allocate Texture Descriptor Sets");

    // Texture Imaghe Info
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // Image Layout when in use
    imageInfo.imageView = textureImage; // Image to bind to set
    imageInfo.sampler = textureSampler; // Samplet to use for set

    // Descriptor Write Info
    VkWriteDescriptorSet writeDescriptorSet{};
    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.dstSet = descriptorSet;
    writeDescriptorSet.dstBinding = 0;
    writeDescriptorSet.dstArrayElement = 0;
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writeDescriptorSet.descriptorCount = 1;
    writeDescriptorSet.pImageInfo = &imageInfo;

    // Update new descriptor set
    vkUpdateDescriptorSets(device_.logicalDevice, 1, &writeDescriptorSet, 0, nullptr);

    // Add descriptor set to list
    samplerDescriptorSets.push_back(descriptorSet);

    // Return descriptor set location
    return static_cast<int>(samplerDescriptorSets.size()) - 1;
}

int VulkanRenderer::createMeshModel(const std::string &modelFile) {
    // Import model "scene"
    Assimp::Importer importer;
    const aiScene *scene = importer.ReadFile(modelFile, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices);

    if (!scene) {
        throw std::runtime_error("Failed to load model! (" + modelFile + ")");
    }

    // Get vector of all materials with 1:1 ID placement
    std::vector<std::string> textureNames = MeshModel::loadMaterials(scene);

    // Conversion from the materials list IDs to our Descriptor Array IDs
    std::vector<int> matToTex(textureNames.size());

    // Loop over textureNames and create textures for them
    for (size_t i = 0; i < textureNames.size(); i++) {
        // If material had no texture, set '0' to indicate no texture, texture 0 will be reserved for a default texture
        if (textureNames[i].empty()) {
            matToTex[i] = 0;
        } else {
            // Otherwise, create texture and set value to index of new texture
            matToTex[i] = createTexture(textureNames[i]);
        }
    }

    // Load in all our meshes
    std::vector<Mesh> modelMeshes = MeshModel::LoadNode(device_.physicalDevice, device_.logicalDevice, graphicsQueues_,
                                                        graphicsCommandPool,
                                                        scene->mRootNode, scene, matToTex);

    // Create mesh model and add to list
    MeshModel meshModel = MeshModel(modelMeshes);
    modelList.push_back(meshModel);

    return modelList.size() - 1;
}

stbi_uc* VulkanRenderer::loadTextureFile(const std::string &fileName, int *width, int *height, VkDeviceSize *imageSize) {
    // Number of channels image usage
    int channels;

    // Load pixel data for image
    std::string fileLoc = "../assets/images/" + fileName;
    stbi_uc* image = stbi_load(fileLoc.c_str(), width, height, &channels, STBI_rgb_alpha);

    if (!image) throw std::runtime_error("Failed to load a Texture file: " + fileName);

    // Calculate image size using given and known data
    *imageSize = *width * *height * static_cast<int>(STBI_rgb_alpha);

    return image;
}
