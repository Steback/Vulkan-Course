cmake_minimum_required(VERSION 3.17)
project(Vulkan-course)

set(CMAKE_CXX_STANDARD 17)

set(build_types Debug Release)

file(GLOB_RECURSE HEADER_FILES source/*.hpp)
file(GLOB_RECURSE SOURCE_FILES source/*.cpp)

# GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(thirdparty/glfw)

# Vulkan
include_directories($ENV{VULKAN_SDK}/include)
file(GLOB_RECURSE VALIDATION_LAYERS $ENV{LD_LIBRARY_PATH}/libVkLayer_*.so)
file(GLOB_RECURSE VULKAN $ENV{LD_LIBRARY_PATH}/libvulkan.so)

# GLM
add_subdirectory(thirdparty/glm)
add_library(glm::glm ALIAS glm)

# spdlog
add_subdirectory(thirdparty/spdlog)

add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${HEADER_FILES})
target_link_libraries(${PROJECT_NAME} glfw glm ${VULKAN} ${VALIDATION_LAYERS} spdlog)

### COMPILE SHADERS ###
set(GLSL_VALIDATOR $ENV{VULKAN_SDK}/bin/glslangValidator)
file(GLOB_RECURSE shaders_source shaders/*.vert shaders/*.frag)

foreach(shader ${shaders_source})
    execute_process(COMMAND ${GLSL_VALIDATOR} -V ${shader}
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/shaders/)
endforeach()