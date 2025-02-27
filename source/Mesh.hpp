#ifndef VULKAN_COURSE_MESH_HPP
#define VULKAN_COURSE_MESH_HPP


#include <vector>

#include "vulkan/vulkan.h"
#include "GLFW/glfw3.h"
#include "glm/glm.hpp"

#include "Utilities.hpp"


struct Model {
    glm::mat4 model;
};

class Mesh {
    public:
        Mesh();
        Mesh(VkPhysicalDevice physicalDevice, VkDevice device, const std::vector<Vertex>& vertices,
             VkQueue transferQueue, VkCommandPool transferCommandPool, const std::vector<uint32_t>& indices,
             int newTextureID);
        ~Mesh();
        [[nodiscard]] int getVertexCount() const;
        VkBuffer getVertexBuffer();
        void clean();
        [[nodiscard]] int getIndexCount() const;
        VkBuffer getIndexBuffer();
        [[nodiscard]] const Model &getUboModel() const;
        void setUboModel(const Model &uboModel);
        [[nodiscard]] int getTextureId() const;
        void setTextureId(int textureId);

    private:
        void createVertexBuffer(const std::vector<Vertex>& vertices, VkQueue transferQueue,
                                VkCommandPool transferCommandPool);
        void createIndexBuffer(const std::vector<uint32_t>& indices, VkQueue transferQueue,
                               VkCommandPool transferCommandPool);

    private:
        Model model_{};
        int vertexCount_{};
        VkBuffer vertexbuffer_{};
        VkPhysicalDevice physicalDevice_{};
        VkDevice device_{};
        VkDeviceMemory vertexBufferMemory{};
        int indexCount_{};
        VkBuffer indexBuffer_{};
        VkDeviceMemory indexBufferMemory_{};
        int textureID{};
};


#endif
