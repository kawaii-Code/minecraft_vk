#version 450

layout(binding = 0) uniform Uniform_Buffer_Object {
    mat4 model;
    mat4 view;
    mat4 projection;
} ubo;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec2 in_texture_coordinates;

layout(location = 0) out vec2 fragment_texture_coordinates;

void main() {
    gl_Position = ubo.projection * ubo.view * ubo.model * vec4(in_position, 1.0);
    fragment_texture_coordinates = in_texture_coordinates;
}
