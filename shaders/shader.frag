#version 450

layout (location = 0) in vec3 fragColor;
layout (location = 1) in vec2 fragTex;

layout (set = 1, binding = 0) uniform sampler2D textureSampler;

layout (location = 0) out vec4 outColour; // Final out colour (must also be have location)

void main() {
    outColour = texture(textureSampler, fragTex);
}