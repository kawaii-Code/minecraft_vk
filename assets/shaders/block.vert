#version 450

layout(binding = 0) uniform Uniform_Buffer_Object {
    mat4 view_proj;
} ubo;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec2 in_texture_coordinates;
layout(location = 2) in vec3 in_world_position;
layout(location = 3) in int in_block_type;

layout(location = 0) out vec2 fragment_texture_coordinates;
layout(location = 1) out flat int fragment_block_type;

void main() {
    gl_Position = ubo.view_proj * vec4(in_world_position + in_position, 1.0);
    fragment_texture_coordinates = in_texture_coordinates;
    fragment_block_type = in_block_type;
}
