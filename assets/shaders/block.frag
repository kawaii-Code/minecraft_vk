#version 450

layout(binding = 1) uniform sampler2D texture_sampler;

layout(location = 0) in vec2 in_texture_coordinates;
layout(location = 1) in flat int block_type;

layout(location = 0) out vec4 out_color;

void main() {
    int texture_y = block_type;

    vec2 texture_coordinates;
    texture_coordinates.x = (in_texture_coordinates.x) / 16.0f;
    texture_coordinates.y = (in_texture_coordinates.y + texture_y) / 16.0f;

    out_color = texture(texture_sampler, texture_coordinates);
}
