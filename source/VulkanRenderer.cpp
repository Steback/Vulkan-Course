#include <set>

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
    } catch (const std::runtime_error& error) {
        spdlog::error("[Vulkan-Renderer] {}", error.what());

        return EXIT_FAILURE;
    }

    return 0;
}

void VulkanRenderer::clean() {
    spdlog::info("[Vulkan-Renderer] Destroy Device and Instance");
    vkDestroySurfaceKHR(instance_, surface_, nullptr);
    validationLayers->clean(instance_);
    vkDestroyDevice(device_.logicalDevice, nullptr);
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
