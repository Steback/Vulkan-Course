#ifndef VULKAN_COURSE_VALIDATIONLAYERS_HPP
#define VULKAN_COURSE_VALIDATIONLAYERS_HPP


#include <vector>

#include "vulkan/vulkan.h"


#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif


class ValidationLayers {
    public:
        explicit ValidationLayers(std::vector<const char*> layerNames);
        ~ValidationLayers();
        void init(VkInstance& instance);
        void clean(VkInstance& instance);
        bool checkValidationLayerSupport();
        uint32_t getValidationLayersCount();
        const char** getValidationLayers();
        static void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

    private:
        static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
                VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                VkDebugUtilsMessageTypeFlagsEXT messageType,
                const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                void* pUserData);
        static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
                                              const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                              const VkAllocationCallbacks* pAllocator,
                                              VkDebugUtilsMessengerEXT*pDebugMessenger);
        static void DestroyDebugUtilsMessengerEXT(VkInstance instance,
                                           VkDebugUtilsMessengerEXT debugMessenger, const
                                           VkAllocationCallbacks* pAllocator);

    private:
        std::vector<const char*> validationLayers_;
        std::vector<const char*> extensions_;
};


#endif
