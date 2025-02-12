#version 450

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec2 in_texture_coordinates;

layout(location = 0) out vec2 fragment_texture_coordinates;

void main() {
    gl_Position = vec4(in_position, 0.0, 1.0);
    fragment_texture_coordinates = in_texture_coordinates;
}
