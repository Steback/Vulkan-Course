#ifndef VULKAN_COURSE_MESHMODEL_HPP
#define VULKAN_COURSE_MESHMODEL_HPP


#include <vector>
#include <string>

#include "assimp/scene.h"
#include "vulkan/vulkan.hpp"

#include "glm/glm.hpp"


class Mesh;

class MeshModel {
public:
    MeshModel();
    explicit MeshModel(std::vector<Mesh> meshList);
    ~MeshModel();
    [[nodiscard]] size_t getMeshCount() const;
    Mesh* getMesh(size_t index);
    [[nodiscard]] const glm::mat4 &getModel() const;
    void setModel(const glm::mat4 &model);
    void clean();
    static std::vector<std::string> loadMaterials(const aiScene* scene);
    static std::vector<Mesh> LoadNode(VkPhysicalDevice physicalDevice, VkDevice device, VkQueue queue,
                                      VkCommandPool commandPool, aiNode* node, const aiScene* scene,
                                      const std::vector<int>& matToTex);
    static Mesh LoadMesh(VkPhysicalDevice physicalDevice, VkDevice device, VkQueue queue,
                         VkCommandPool commandPool, aiMesh* mesh, const aiScene* scene,
                         const std::vector<int>& matToTex);

private:
    std::vector<Mesh> meshList_;
    glm::mat4 model_{};
};


#endif
