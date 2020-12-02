#ifndef VULKAN_COURSE_UTILITIES_HPP
#define VULKAN_COURSE_UTILITIES_HPP


// Indices (locations) of Queue Families (if they exists at all)
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily; // Location of Graphics Queue Family

    [[nodiscard]] bool isValid() const {
        return graphicsFamily.has_value();
    }
};


#endif
