cmake_minimum_required(VERSION 3.17)
project(Vulkan-course)

set(CMAKE_CXX_STANDARD 17)

file(GLOB_RECURSE HEADER_FILES source/*.hpp)
file(GLOB_RECURSE SOURCE_FILES source/*.cpp)

add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${HEADER_FILES})

# Vulkan
set(SPIRV_HEADERS_ENABLE_EXAMPLES OFF)
add_subdirectory(thirdparty/vulkan/source/Vulkan-Headers)
add_subdirectory(thirdparty/vulkan/source/glslang)
add_subdirectory(thirdparty/vulkan/source/Vulkan-ValidationLayers)

# GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(thirdparty/glfw)

# GLM
add_subdirectory(thirdparty/glm)
add_library(glm::glm ALIAS glm)

# spdlog
add_subdirectory(thirdparty/spdlog)

target_link_libraries(${PROJECT_NAME} glfw glm vulkan spdlog)

### COMPILE SHADERS ###
set(GLSL_VALIDATOR ${PROJECT_SOURCE_DIR}/thirdparty/vulkan/x86_64/bin/glslangValidator)
file(GLOB_RECURSE shaders_source shaders/*.vert shaders/*.frag)

foreach(shader ${shaders_source})
    execute_process(COMMAND ${GLSL_VALIDATOR} -V ${shader}
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/shaders/)
endforeach()