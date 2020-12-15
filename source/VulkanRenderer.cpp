#include <set>
#include <algorithm>
#include <array>

#include "spdlog/spdlog.h"

#include "VulkanRenderer.hpp"
#include "Utilities.hpp"
#include "ValidationLayers.hpp"


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
        createGraphicsPipeline();
    } catch (const std::runtime_error& error) {
        spdlog::error("[Vulkan-Renderer] {}", error.what());

        return EXIT_FAILURE;
    }

    return 0;
}

void VulkanRenderer::clean() {
    spdlog::info("[Vulkan-Renderer] Destroy Device and Instance");

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

void VulkanRenderer::createInstance() {
    if (enableValidationLayers) {
        validationLayers = std::make_unique<ValidationLayers>(std::vector<const char*>{
                "VK_LAYER_KHRONOS_validation"
        });

        if (!validationLayers->checkValidationLayerSupport()) {
            throw std::runtime_error("validation layers requested, but not available!");
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
    auto vertexShaderCode = readFile("../shaders/vert.spv");
    auto fragmentShaderCode = readFile("../shaders/frag.spv");

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

    // -- VERTEX INPUT (TODO: Put in vertex descriptions when resources created) --
    VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 0,
        .pVertexBindingDescriptions = nullptr, // List of Vertex Binding Description (data spacing/stride information)
        .vertexAttributeDescriptionCount = 0,
        .pVertexAttributeDescriptions = nullptr, // List of Vertex Attribute Descriptions (data description and where to bind to/from)
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
        .frontFace = VK_FRONT_FACE_CLOCKWISE, // Winding to determine which side is front
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

    // -- PIPELINE LAYOUT (TODO: Apply future descriptor set layouts) --
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 0,
        .pSetLayouts = nullptr,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = nullptr
    };

    // Create pipeline layout
    VkResult result = vkCreatePipelineLayout(device_.logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Pipeline Layout");
    }

    // -- DEPTH STENCIL TESTING --
    // TODO: Set up depth stencil testing

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
        .pDepthStencilState = nullptr,
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
}

void VulkanRenderer::createRenderPass() {
    // Colour attachment of render pass
    VkAttachmentDescription attachmentDescription{
        .format = swapChainImageFormat_, // Format to use for attachment
        .samples = VK_SAMPLE_COUNT_1_BIT, // Number of samples to write for multisampling
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, // Describes what to do with attachment before rendering
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE, // Describes what to do with attachment after rendering
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, // Describes what to do with stencil before rendering
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, // Describes what to do with stencil before rendering
    };

    // Framebuffer data will be stored as an image, but images can be given different data layouts
    // to give optimal use for certain operations
    attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Image data layout before render pass starts
    attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // Image data layout after render pass(to change to)

    // Attachment reference uses an attachment index that refers to index in an attachment list passed to renderPassCreateInfo
    VkAttachmentReference attachmentReference{
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    // Information about a particular subpass the Render Pass is using
    VkSubpassDescription subpass{
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS, // Pipeline type subpass is to be bound to
        .colorAttachmentCount = 1,
        .pColorAttachments = &attachmentReference,
    };

    // Need to determine when layout transitions occur using dependencies
    std::array<VkSubpassDependency, 2> subpassDependencies{};

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

    // Conversion from VK_IMAGE_LAYOUT_UNDEFINED to VK_IMAGE_LAYOUT_COLOR_SRC_KHR
    subpassDependencies[1] = {
            .srcSubpass = 0,
            .dstSubpass = VK_SUBPASS_EXTERNAL,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
            .dependencyFlags = 0
    };

    VkRenderPassCreateInfo renderPassCreateInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &attachmentDescription,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = static_cast<uint32_t>(subpassDependencies.size()),
        .pDependencies = subpassDependencies.data()
    };

    VkResult result = vkCreateRenderPass(device_.logicalDevice, &renderPassCreateInfo, nullptr, &renderPass_);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass");
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
//    VkPhysicalDeviceFeatures deviceFeatures;
//    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

    QueueFamilyIndices indices = getQueueFamilies(device);
    bool extensionSupported = checkDeviceExtensionSupport(device);
    bool swapChainValid = false;

    if (extensionSupported) {
        SwapChainDetails swapChainDetails = getSwapChainDetails(device);
        swapChainValid = !swapChainDetails.presentationModes.empty() && !swapChainDetails.formats.empty();
    }

    return indices.isValid() && extensionSupported && swapChainValid;
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
