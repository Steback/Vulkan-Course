#version 450

layout (location = 0) in vec3 fragColour; // Interpolated colour from vertex (location must match)

layout (location = 0) out vec4 outColour; // Final out colour (must also be have location)

void main() {
    outColour = vec4(fragColour, 1.0);
}