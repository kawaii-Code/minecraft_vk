f32 camera_speed = 15.0f;
f32 camera_sprint_speed = 45.0f;
f32 camera_near_plane = 0.1f;
f32 camera_far_plane = 100.0f;
f32 mouse_sensitivity = 0.5f;


void game_init(Game *game, i32 seed) {
    ZoneScoped;

    i32 world_size_x = 128;
    i32 world_size_z = 128;
    game->block_count = world_size_x * world_size_z * 16;
    game->blocks = (glm::vec3 *)allocate_memory(&game->allocator, sizeof(*game->blocks) * game->block_count);
    game->block_types = (i32 *)allocate_memory(&game->allocator, sizeof(*game->block_types) * game->block_count);
    for (i32 x = 0; x < world_size_x; x++) {
        for (i32 z = 0; z < world_size_z; z++) {
            // Overlay noise on top of itself
            f32 final_noise = 0.0f;
            for (i32 k = 0; k < 6; k++) {
                f32 ratio = powf(2, (f32)k);
                final_noise += perlin_noise(
                    seed,
                    ratio * glm::vec2((f32)x / (f32)world_size_x, (f32)z / (f32)world_size_z)
                ) / ratio;
            }

            i32 height = (i32)(16.0f * final_noise);
            for (i32 y = 0; y < 16; y++) {
                i32 index = x*world_size_x*16 + z*16 + y;
                assert(index < game->block_count);
                if (y == 0) {
                    game->block_types[index] = BLOCK_TYPE_GRASS;
                } else if (y < 4) {
                    game->block_types[index] = BLOCK_TYPE_DIRT;
                } else {
                    game->block_types[index] = BLOCK_TYPE_STONE;
                }
                game->blocks[index] = glm::vec3(x, height + y, -z);
            }
        }
    }
}

void game_update_and_render(Game *game, u32 frame_in_flight_index, u32 swapchain_image_index) {
    ZoneScoped;

    Input *input = &game->input;
    f32 time = game->time_passed();
    f32 delta_time = game->delta_time;

    Camera *camera = &game->camera;
    glm::vec3 camera_forward = glm::vec3(sin(camera->yaw) * cos(camera->pitch), sin(camera->pitch), -1 * cos(camera->pitch) * cos(camera->yaw));
    glm::vec3 camera_up = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 camera_right = glm::cross(camera_forward, camera_up);

    f32 horizontal_direction = -1 * (f32)input->buttons[BUTTON_LEFT].is_down + 1 * (f32)input->buttons[BUTTON_RIGHT].is_down;
    f32 forward_direction =    -1 * (f32)input->buttons[BUTTON_DOWN].is_down + 1 * (f32)input->buttons[BUTTON_UP].is_down;

    glm::vec3 move_direction = normalize_or_zero(forward_direction * camera_forward + horizontal_direction * camera_right);
    if (input->buttons[BUTTON_SPRINT].is_down) {
        camera->position += camera_sprint_speed * move_direction * delta_time;
    } else {
        camera->position += camera_speed * move_direction * delta_time;
    }

    if (input->buttons[BUTTON_F2].was_just_pressed()) {
        game->wireframe_mode = !game->wireframe_mode;
    }

    f32 dx = mouse_sensitivity * (f32)input->mouse_dx * delta_time;
    f32 dy =  mouse_sensitivity * (f32)input->mouse_dy * delta_time;
    camera->yaw += -1 * dx;  // counter clockwise
    camera->pitch += dy;

    for (i32 i = 0; i < BUTTON_COUNT; i++) {
        input->buttons[i].was_down = input->buttons[i].is_down;
    }

    // Render
    {
        Vulkan_Boilerplate_Objects *vulkan = game->vulkan;
        Renderer *renderer = game->renderer;
        VkResult vulkan_result;

        {
            glm::mat4 view = glm::lookAt(camera->position, camera->position + camera_forward, glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 projection = glm::perspective(
                glm::radians(45.0f),
                (f32)vulkan->swapchain_extent.width / (f32)vulkan->swapchain_extent.height,
                camera_near_plane,
                camera_far_plane
            );

            Uniform_Buffer_Object ubo = {};
            ubo.view_proj = projection * view;

            *(Uniform_Buffer_Object *)renderer->uniform_buffers_mapped_memory[frame_in_flight_index] = ubo;
            for (i32 i = 0; i < game->block_count; i++) {
                Block_Info info;
                info.world_position = game->blocks[i];
                info.block_type = game->block_types[i];
                *((Block_Info *)renderer->block_info_buffers_mapped_memory[frame_in_flight_index] + i) = info;
            }
        }

        VkClearValue clear_colors[2] = {};
        glm::vec3 sky_color = hex_color_to_rgb("#83AFEE");
        clear_colors[0].color = {{sky_color.r, sky_color.g, sky_color.b, 1.0f}};
        clear_colors[1].depthStencil = {1.0f, 0};
        VkRenderPassBeginInfo render_pass_begin_info = {};
        render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_begin_info.renderPass = renderer->render_pass;
        render_pass_begin_info.framebuffer = vulkan->swapchain_framebuffers[swapchain_image_index];
        render_pass_begin_info.renderArea.offset = {0, 0};
        render_pass_begin_info.renderArea.extent = vulkan->swapchain_extent;
        render_pass_begin_info.clearValueCount = ARRAY_LENGTH(clear_colors);
        render_pass_begin_info.pClearValues = clear_colors;

        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (f32)(vulkan->swapchain_extent.width);
        viewport.height = (f32)(vulkan->swapchain_extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor = {};
        scissor.offset = {0, 0};
        scissor.extent = vulkan->swapchain_extent;

        VkBuffer vertex_buffers[] = {renderer->vertex_buffer, renderer->block_info_buffers[frame_in_flight_index]};
        VkDeviceSize offsets[] = {0, 0};

        glm::vec3 flat_color = glm::vec3(1.0f, 0.0f, 0.0f);

        VkCommandBuffer command_buffer = vulkan->command_buffers[frame_in_flight_index];
        vkResetCommandBuffer(command_buffer, 0);

        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = 0;
        begin_info.pInheritanceInfo = 0;
        vulkan_result = vkBeginCommandBuffer(command_buffer, &begin_info);
        assert(vulkan_result == VK_SUCCESS);


        f32 crosshair_size = ((f32)game->win32->window_height / 60.0f);
        Quad quads[] = {
            {
                glm::vec2((f32)game->win32->window_width / 2.0f - crosshair_size, (f32)game->win32->window_height / 2.0f - crosshair_size),
                glm::vec2((f32)game->win32->window_width / 2.0f + crosshair_size, (f32)game->win32->window_height / 2.0f + crosshair_size),
            },
        };

        i32 quad_buffer_size = 6 * ARRAY_LENGTH(quads) * sizeof(Quad_Vertex);
        VkBuffer *quad_buffer = &renderer->quad_buffers[frame_in_flight_index];
        VkDeviceMemory *quad_buffer_memory = &renderer->quad_buffers_memory[frame_in_flight_index];
        void *quad_buffer_mapped_memory;
        vulkan_create_buffer(vulkan, quad_buffer_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, quad_buffer, quad_buffer_memory);
        vkMapMemory(vulkan->logical_device, *quad_buffer_memory, 0, quad_buffer_size, 0, &quad_buffer_mapped_memory);
        for (i32 i = 0; i < ARRAY_LENGTH(quads); i++) {
            Quad_Vertex bottom_left;
            bottom_left.position = screen_coordinates_to_normalized_coordinates(game->win32, quads[i].bottom_left());
            bottom_left.texture_coordinates = glm::vec2(0.0f, 0.0f);

            Quad_Vertex bottom_right;
            bottom_right.position = screen_coordinates_to_normalized_coordinates(game->win32, quads[i].bottom_right());
            bottom_right.texture_coordinates = glm::vec2(1.0f, 0.0f);

            Quad_Vertex top_left;
            top_left.position = screen_coordinates_to_normalized_coordinates(game->win32, quads[i].top_left());
            top_left.texture_coordinates = glm::vec2(0.0f, 1.0f);

            Quad_Vertex top_right;
            top_right.position = screen_coordinates_to_normalized_coordinates(game->win32, quads[i].top_right());
            top_right.texture_coordinates = glm::vec2(1.0f, 1.0f);

            *((Quad_Vertex *)quad_buffer_mapped_memory + 0) = bottom_left;
            *((Quad_Vertex *)quad_buffer_mapped_memory + 1) = bottom_right;
            *((Quad_Vertex *)quad_buffer_mapped_memory + 2) = top_left;
            *((Quad_Vertex *)quad_buffer_mapped_memory + 3) = top_right;
            *((Quad_Vertex *)quad_buffer_mapped_memory + 4) = top_left;
            *((Quad_Vertex *)quad_buffer_mapped_memory + 5) = bottom_right;
        }
        vkUnmapMemory(vulkan->logical_device, *quad_buffer_memory);

        vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdSetViewport(command_buffer, 0, 1, &viewport);
        vkCmdSetScissor(command_buffer, 0, 1, &scissor);

        if (game->wireframe_mode) {
            glm::vec3 wireframe_color = glm::vec3(0.7f, 0.7f, 0.7f);
            vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->block_wireframe_pipeline.pipeline);
            vkCmdPushConstants(command_buffer, renderer->block_wireframe_pipeline.layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::vec3), &wireframe_color);
            vkCmdSetLineWidth(command_buffer, 3.0f);
            vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->block_wireframe_pipeline.layout, 0, 1, &renderer->block_pipeline.descriptor_sets[frame_in_flight_index], 0, 0);
        } else {
            vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->block_pipeline.pipeline);
            vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->block_pipeline.layout, 0, 1, &renderer->block_pipeline.descriptor_sets[frame_in_flight_index], 0, 0);
        }
        vkCmdBindIndexBuffer(command_buffer, renderer->index_buffer, 0, VK_INDEX_TYPE_UINT16);
        vkCmdBindVertexBuffers(command_buffer, 0, ARRAY_LENGTH(vertex_buffers), vertex_buffers, offsets);
        vkCmdDrawIndexed(command_buffer, renderer->indices_count, game->block_count, 0, 0, 0);

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->quad_pipeline.pipeline);
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->quad_pipeline.layout, 0, 1, &renderer->quad_pipeline.descriptor_sets[frame_in_flight_index], 0, 0);
        vkCmdBindVertexBuffers(command_buffer, 0, 1, quad_buffer, offsets);
        vkCmdDraw(command_buffer, 6, 1, 0, 0);

        vkCmdEndRenderPass(command_buffer);

        vulkan_result = vkEndCommandBuffer(command_buffer);
        assert(vulkan_result == VK_SUCCESS);
    }
}


void game_post_render_cleanup(Vulkan_Boilerplate_Objects *vulkan, Renderer *renderer, i32 frame_in_flight_index) {
    vkDestroyBuffer(vulkan->logical_device, renderer->quad_buffers[frame_in_flight_index], vulkan->allocator);
    vkFreeMemory(vulkan->logical_device, renderer->quad_buffers_memory[frame_in_flight_index], vulkan->allocator);
}


void *allocate_memory(Bump_Allocator *allocator, i32 requested_size) {
    void *result = ((char *)allocator->memory + allocator->occupied_size);
    allocator->occupied_size += requested_size;
    assert(allocator->occupied_size <= allocator->total_size);
    return result;
}


glm::vec3 hex_color_to_rgb(char *hex) {
    i32 i = 0;
    if (hex[i] == '#') {
        i++;
    }

    glm::vec3 result;

    result.x = (f32)(hex_char_to_byte(hex[i]) * 16 + hex_char_to_byte(hex[i + 1])) / 256.0f;
    i += 2;
    result.y = (f32)(hex_char_to_byte(hex[i]) * 16 + hex_char_to_byte(hex[i + 1])) / 256.0f;
    i += 2;
    result.z = (f32)(hex_char_to_byte(hex[i]) * 16 + hex_char_to_byte(hex[i + 1])) / 256.0f;

    return result;
}


glm::vec2 screen_coordinates_to_normalized_coordinates(Win32_State *win32, glm::vec2 coordinates) {
    return 2.0f * (coordinates / glm::vec2(win32->window_width, win32->window_height)) - glm::vec2(1.0f);
}


i8 hex_char_to_byte(char c) {
    assert(('0' <= c && c <= '9') || ('A' <= c && c <= 'F'));
    if (c >= 'A') {
        return 10 + c - 'A';
    }
    return c - '0';
}


glm::vec3 normalize_or_zero(glm::vec3 vector) {
    if (glm::length(vector) == 0) {
        return glm::vec3(0, 0, 0);
    }
    return glm::normalize(vector);
}


u32 clamp(u32 a, u32 low, u32 high) {
    if (a < low) {
        return low;
    }
    if (a > high) {
        return high;
    }
    return a;
}


i32 perlin_table[] = {
    151, 160, 137,  91,  90,  15, 131,  13, 201,  95,  96,  53, 194, 233,   7, 225,
    140,  36, 103,  30,  69, 142,   8,  99,  37, 240,  21,  10,  23, 190,   6, 148,
    247, 120, 234,  75,   0,  26, 197,  62,  94, 252, 219, 203, 117,  35,  11,  32,
    57, 177,  33,  88, 237, 149,  56,  87, 174,  20, 125, 136, 171, 168,  68, 175,
    74, 165,  71, 134, 139,  48,  27, 166,  77, 146, 158, 231,  83, 111, 229, 122,
    60, 211, 133, 230, 220, 105,  92,  41,  55,  46, 245,  40, 244, 102, 143,  54,
    65,  25,  63, 161,   1, 216,  80,  73, 209,  76, 132, 187, 208,  89,  18, 169,
    200, 196, 135, 130, 116, 188, 159,  86, 164, 100, 109, 198, 173, 186,   3,  64,
    52, 217, 226, 250, 124, 123,   5, 202,  38, 147, 118, 126, 255,  82,  85, 212,
    207, 206,  59, 227,  47,  16,  58,  17, 182, 189,  28,  42, 223, 183, 170, 213,
    119, 248, 152,   2,  44, 154, 163,  70, 221, 153, 101, 155, 167,  43, 172,   9,
    129,  22,  39, 253,  19,  98, 108, 110,  79, 113, 224, 232, 178, 185, 112, 104,
    218, 246,  97, 228, 251,  34, 242, 193, 238, 210, 144,  12, 191, 179, 162, 241,
    81,  51, 145, 235, 249,  14, 239, 107,  49, 192, 214,  31, 181, 199, 106, 157,
    184,  84, 204, 176, 115, 121,  50,  45, 127,   4, 150, 254, 138, 236, 205,  93,
    222, 114,  67,  29,  24,  72, 243, 141, 128, 195,  78,  66, 215,  61, 156, 180,
    // Repeat
    151, 160, 137,  91,  90,  15, 131,  13, 201,  95,  96,  53, 194, 233,   7, 225,
    140,  36, 103,  30,  69, 142,   8,  99,  37, 240,  21,  10,  23, 190,   6, 148,
    247, 120, 234,  75,   0,  26, 197,  62,  94, 252, 219, 203, 117,  35,  11,  32,
    57, 177,  33,  88, 237, 149,  56,  87, 174,  20, 125, 136, 171, 168,  68, 175,
    74, 165,  71, 134, 139,  48,  27, 166,  77, 146, 158, 231,  83, 111, 229, 122,
    60, 211, 133, 230, 220, 105,  92,  41,  55,  46, 245,  40, 244, 102, 143,  54,
    65,  25,  63, 161,   1, 216,  80,  73, 209,  76, 132, 187, 208,  89,  18, 169,
    200, 196, 135, 130, 116, 188, 159,  86, 164, 100, 109, 198, 173, 186,   3,  64,
    52, 217, 226, 250, 124, 123,   5, 202,  38, 147, 118, 126, 255,  82,  85, 212,
    207, 206,  59, 227,  47,  16,  58,  17, 182, 189,  28,  42, 223, 183, 170, 213,
    119, 248, 152,   2,  44, 154, 163,  70, 221, 153, 101, 155, 167,  43, 172,   9,
    129,  22,  39, 253,  19,  98, 108, 110,  79, 113, 224, 232, 178, 185, 112, 104,
    218, 246,  97, 228, 251,  34, 242, 193, 238, 210, 144,  12, 191, 179, 162, 241,
    81,  51, 145, 235, 249,  14, 239, 107,  49, 192, 214,  31, 181, 199, 106, 157,
    184,  84, 204, 176, 115, 121,  50,  45, 127,   4, 150, 254, 138, 236, 205,  93,
    222, 114,  67,  29,  24,  72, 243, 141, 128, 195,  78,  66, 215,  61, 156, 180
};


f32 perlin_noise_fade(f32 t) {
    return t*t*t * (6.0f * t*t - 15.0f * t + 10.0f);
}


glm::vec2 perlin_noise_gradient(i32 seed, glm::vec2 p) {
    f32 v1 = (f32)perlin_table[perlin_table[(*((i32 *)&p.x) + seed) & 0xFF] + ((*(i32 *)&p.y) + seed) & 0xFF];
    f32 v2 = (f32)perlin_table[perlin_table[(*((i32 *)&p.y) + seed) & 0xFF] + ((*(i32 *)&p.x) + seed) & 0xFF];
    return glm::normalize(glm::vec2(v1, v2)*2.0f - glm::vec2(128));
}


f32 perlin_noise(i32 seed, glm::vec2 p) {
    glm::vec2 p0 = floor(p);
    glm::vec2 p1 = p0 + glm::vec2(1.0f, 0.0f);
    glm::vec2 p2 = p0 + glm::vec2(0.0f, 1.0f);
    glm::vec2 p3 = p0 + glm::vec2(1.0f, 1.0f);

    glm::vec2 g0 = perlin_noise_gradient(seed, p0);
    glm::vec2 g1 = perlin_noise_gradient(seed, p1);
    glm::vec2 g2 = perlin_noise_gradient(seed, p2);
    glm::vec2 g3 = perlin_noise_gradient(seed, p3);

    f32 t0 = p.x - p0.x;
    f32 fade_t0 = perlin_noise_fade(t0);

    f32 t1 = p.y - p0.y;
    f32 fade_t1 = perlin_noise_fade(t1);

    f32 p0p1 = (1.0f - fade_t0) * glm::dot(g0, (p - p0)) + fade_t0 * glm::dot(g1, (p - p1));
    f32 p2p3 = (1.0f - fade_t0) * glm::dot(g2, (p - p2)) + fade_t0 * glm::dot(g3, (p - p3));

    return (1.0f - fade_t1) * p0p1 + fade_t1 * p2p3;
}
