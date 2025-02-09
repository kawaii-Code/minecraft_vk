#version 450

layout(binding = 0) uniform Uniform_Buffer_Object {
    mat4 view;
    mat4 projection;
} ubo;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_world_position;
layout(location = 2) in vec3 in_world_rotation;
layout(location = 3) in vec3 in_world_scale;

void main() {
    mat4 model = mat4(1.0f);
    model[3] = vec4(in_world_position, 1.0);
    gl_Position = ubo.projection * ubo.view * model * vec4(in_position, 1.0);
}
