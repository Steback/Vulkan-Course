#ifndef VULKAN_COURSE_UTILITIES_HPP
#define VULKAN_COURSE_UTILITIES_HPP


// Indices (locations) of Queue Families (if they exists at all)
struct QueueFamilyIndices {
    int graphicalFamily = -1; // Location of Graphics Queue Family

    // Check if queue families are valid
    [[nodiscard]] bool isValid() const {
        return graphicalFamily >= 0;
    }
};


#endif //VULKAN_COURSE_UTILITIES_HPP
