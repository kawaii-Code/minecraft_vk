#ifndef MINECRAFT_HPP_
#define MINECRAFT_HPP_


#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include <tracy/Tracy.hpp>

#include <stdio.h>


#define PROGRAM_TITLE "Game clone with vulkan"
#define ARRAY_LENGTH(a) (sizeof(a) / sizeof(*a))
#define MAX_FRAMES_IN_FLIGHT 4


typedef char           i8;
typedef short          i16;
typedef int            i32;
typedef long long      i64;
typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef unsigned long  u64;
typedef float          f32;
typedef double         f64;
typedef i8             b8;
typedef i32            b32;


struct String {
    char *ptr;
    i32   length;
};

struct Vulkan_Boilerplate_Objects {
    VkInstance instance;
    VkAllocationCallbacks *allocator;
    VkDebugUtilsMessengerEXT debug_messenger;

    VkDevice logical_device;
    VkPhysicalDevice physical_device;
    VkPhysicalDeviceProperties physical_device_properties;
    VkPhysicalDeviceMemoryProperties memory_properties;
    VkSampleCountFlagBits msaa_samples;

    VkDescriptorPool descriptor_pool;
    VkCommandPool command_pool;
    VkCommandBuffer command_buffers[MAX_FRAMES_IN_FLIGHT];

    VkSurfaceKHR surface;

    VkSwapchainKHR swapchain;
    u32 swapchain_image_count;
    VkImage swapchain_images[8];
    VkImageView swapchain_image_views[8];
    VkFramebuffer swapchain_framebuffers[8];
    VkSurfaceFormatKHR swapchain_format;
    VkPresentModeKHR swapchain_present_mode;
    VkExtent2D swapchain_extent;

    VkQueue graphics_queue;
    VkQueue present_queue;
    u32 graphics_queue_family;
    u32 present_queue_family;

    VkSemaphore image_available_semaphores[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore render_finished_semaphores[MAX_FRAMES_IN_FLIGHT];
    VkFence in_flight_fences[MAX_FRAMES_IN_FLIGHT];
};

struct Renderer_Pipeline {
    VkPipeline pipeline;
    VkPipelineLayout layout;
    VkDescriptorSetLayout descriptor_set_layout;
};

struct Renderer {
    VkRenderPass render_pass;

    VkDescriptorSet texture_pipeline_descriptor_sets[MAX_FRAMES_IN_FLIGHT];
    VkDescriptorSet flat_color_pipeline_descriptor_sets[MAX_FRAMES_IN_FLIGHT];

    Renderer_Pipeline texture_pipeline;

    VkSampler block_texture_sampler;
    Renderer_Pipeline block_pipeline;
    Renderer_Pipeline block_wireframe_pipeline;

    Renderer_Pipeline flat_color_pipeline;

    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_buffer_memory;
    u32 vertex_count;
    VkBuffer index_buffer;
    VkDeviceMemory index_buffer_memory;
    u32 indices_count;

    VkImage texture;
    VkImageView texture_view;
    VkDeviceMemory texture_memory;
    VkSampler texture_sampler;

    VkImage depth_texture;
    VkDeviceMemory depth_texture_memory;
    VkImageView depth_texture_view;
    VkFormat depth_texture_format;

    VkBuffer uniform_buffers[MAX_FRAMES_IN_FLIGHT];
    VkDeviceMemory uniform_buffers_memory[MAX_FRAMES_IN_FLIGHT];
    void *uniform_buffers_mapped_memory[MAX_FRAMES_IN_FLIGHT];

    VkBuffer model_buffers[MAX_FRAMES_IN_FLIGHT];
    VkDeviceMemory model_buffers_memory[MAX_FRAMES_IN_FLIGHT];
    void *model_buffers_mapped_memory[MAX_FRAMES_IN_FLIGHT];

    VkBuffer block_info_buffers[MAX_FRAMES_IN_FLIGHT];
    VkDeviceMemory block_info_buffers_memory[MAX_FRAMES_IN_FLIGHT];
    void *block_info_buffers_mapped_memory[MAX_FRAMES_IN_FLIGHT];
    VkDescriptorSet block_pipeline_descriptor_sets[MAX_FRAMES_IN_FLIGHT];

    VkImage multisample_texture;
    VkDeviceMemory multisample_texture_memory;
    VkImageView multisample_texture_view;
};

struct Win32_State {
    HINSTANCE instance;
    HWND window;
    b32 should_quit;
    b32 resize_happened;
    i32 window_width;
    i32 window_height;
    Vulkan_Boilerplate_Objects *vulkan;
};

struct Vertex {
    glm::vec3 position;
    glm::vec3 color;
    glm::vec2 texture_coordinates;
};

struct Vertex_With_Position {
    glm::vec3 position;
};

struct Uniform_Buffer_Object {
    alignas(16) glm::mat4 view_proj;
};

struct Camera {
    glm::vec3 position;
    f32 pitch;
    f32 yaw;
};

enum Buttons {
    BUTTON_LEFT = 0,
    BUTTON_RIGHT,
    BUTTON_UP,
    BUTTON_DOWN,
    BUTTON_SPRINT,
    BUTTON_F1,
    BUTTON_F2,
    BUTTON_F3,
    BUTTON_F4,
    BUTTON_COUNT,
};

struct Input_Button {
    b32 was_down;
    b32 is_down;

    b32 was_just_pressed() {
        return this->is_down && !this->was_down;
    }
};

struct Input {
    Input_Button buttons[BUTTON_COUNT];

    i32 mouse_dx;
    i32 mouse_dy;

    i32 last_mouse_x;
    i32 last_mouse_y;
};

enum Block_Type {
    BLOCK_TYPE_GRASS = 0,
    BLOCK_TYPE_DIRT = 1,
    BLOCK_TYPE_STONE = 2,
};

struct Block_Vertex {
    glm::vec3 position;
    glm::vec2 texture_coordinates;
};

struct Block_Info {
    glm::vec3 world_position;
    int block_type;
};

struct Game {
    Vulkan_Boilerplate_Objects *vulkan;
    Win32_State *win32;
    Input input;
    Renderer *renderer;

    b32 wireframe_mode;

    glm::vec3 camera_position;
    glm::vec3 camera_look_direction;
    Camera camera;

    i32 block_types[4096];
    glm::vec3 blocks[4096];
    i32 block_count;

    i64 total_ticks;
    i64 ticks_frequency;
    f32 delta_time;

    // Storing total time passed in a float is not
    // optimal because of floating point rounding.
    //
    // Doing
    //
    //   time_passed += delta_time
    //
    // every frame will lead to a loss of precision
    f32 time_passed() {
        return (f32)((f64)this->total_ticks / (f64)this->ticks_frequency);
    }
};


void     win32_open_console_for_debugging();
HWND     win32_create_window(Win32_State *win32_state);
LRESULT  win32_window_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
void     win32_capture_cursor(HWND window);
void     win32_release_cursor();
void     win32_set_cursor_position(Game *game, i32 x, i32 y);
String   win32_read_entire_file(char *filename);
void     win32_display_fps_in_window_title(HWND window, i32 fps);
void     win32_fatal(const char *message);

void            vulkan_init(Vulkan_Boilerplate_Objects *vulkan, Renderer *renderer, Win32_State *win32_state);
void            vulkan_choose_swapchain_parameters(Vulkan_Boilerplate_Objects *vulkan, Win32_State *win32_state);
void            vulkan_create_swapchain(Vulkan_Boilerplate_Objects *vulkan, Renderer *renderer, Win32_State *win32_state);
void            vulkan_re_create_swapchain(Vulkan_Boilerplate_Objects *vulkan, Renderer *renderer, Win32_State *win32_state);
void            vulkan_teardown_swapchain(Vulkan_Boilerplate_Objects *vulkan, Renderer *renderer);
void            vulkan_create_buffer(Vulkan_Boilerplate_Objects *vulkan, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer *out_buffer, VkDeviceMemory *out_buffer_memory);
VkCommandBuffer vulkan_begin_one_time_command_buffer(Vulkan_Boilerplate_Objects *vulkan);
void            vulkan_end_and_execute_one_time_command_buffer(Vulkan_Boilerplate_Objects *vulkan, VkCommandBuffer command_buffer);
void            vulkan_load_texture(Vulkan_Boilerplate_Objects *vulkan, const char *texture_path, VkImage *out_image, VkDeviceMemory *out_image_memory, VkImageView *out_image_view, VkSampler *out_sampler);
void            vulkan_transition_image_layout(Vulkan_Boilerplate_Objects *vulkan, VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout);
void            vulkan_create_image(Vulkan_Boilerplate_Objects *vulkan, u32 width, u32 height, VkSampleCountFlagBits sample_count, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage_flags, VkMemoryPropertyFlags properties, VkImage *out_image, VkDeviceMemory *out_image_memory);
VkShaderModule  vulkan_create_shader_module(VkDevice device, String source_code, VkAllocationCallbacks *allocator);
u32             vulkan_choose_memory_type(Vulkan_Boilerplate_Objects *vulkan, u32 type, VkMemoryPropertyFlags properties);
void            vulkan_copy_buffer(Vulkan_Boilerplate_Objects *vulkan, VkBuffer source_buffer, VkBuffer destination_buffer, VkDeviceSize size);
void            vulkan_teardown(Vulkan_Boilerplate_Objects *vulkan, Renderer *renderer);
VkBool32        vulkan_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity, VkDebugUtilsMessageTypeFlagsEXT message_type, const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data);

void              vulkan_renderer_init(Vulkan_Boilerplate_Objects *vulkan, Renderer *renderer);
// Monster function
Renderer_Pipeline vulkan_renderer_create_pipeline(
    Vulkan_Boilerplate_Objects *vulkan,
    VkRenderPass render_pass,
    VkDescriptorSetLayoutBinding *descriptor_set_layout_bindings, u32 descriptor_set_layout_bindings_count,
    VkPushConstantRange *push_constant_ranges, u32 push_constant_ranges_count,
    VkPipelineShaderStageCreateInfo *shader_stages, u32 shader_stages_count,
    VkPipelineInputAssemblyStateCreateInfo *input_assembly_stage,
    VkPipelineVertexInputStateCreateInfo *vertex_input_stage,
    VkPipelineDynamicStateCreateInfo *dynamic_states,
    VkPipelineViewportStateCreateInfo *viewport_stage,
    VkPipelineRasterizationStateCreateInfo *rasterization_stage,
    VkPipelineMultisampleStateCreateInfo *multisampling_stage,
    VkPipelineDepthStencilStateCreateInfo *depth_stencil_stage,
    VkPipelineColorBlendStateCreateInfo *color_blend_stage
);


void      game_init(Game *game);
void      game_update_and_renderer(Game *game, u32 frame_in_flight_index, u32 swapchain_image_index);
glm::vec3 normalize_or_zero(glm::vec3 vector);
u32       clamp(u32 a, u32 low, u32 high);


#endif // MINECRAFT_HPP_
