f32 camera_speed = 5.0f;
f32 camera_sprint_speed = 10.0f;
f32 mouse_sensitivity = 0.5f;


void game_init(Game *game) {
    ZoneScoped;

    Vulkan_Boilerplate_Objects *vulkan = game->vulkan;
    Renderer *renderer = game->renderer;

    game->block_count = 4096;
    for (i32 x = 0; x < 16; x++) {
        for (i32 y = 0; y < 16; y++) {
            for (i32 z = 0; z < 16; z++) {
                i32 index = x*256 + y*16 + z;
                assert(index < game->block_count);
                if (y == 0) {
                    game->block_types[index] = BLOCK_TYPE_GRASS;
                } else if (y < 4) {
                    game->block_types[index] = BLOCK_TYPE_DIRT;
                } else {
                    game->block_types[index] = BLOCK_TYPE_STONE;
                }
                game->blocks[index] = glm::vec3(x, y, -z);
            }
        }
    }

    VkDeviceSize ubo_buffer_size = sizeof(Uniform_Buffer_Object);
    for (i32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vulkan_create_buffer(vulkan, ubo_buffer_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &renderer->uniform_buffers[i], &renderer->uniform_buffers_memory[i]);
        vkMapMemory(vulkan->logical_device, renderer->uniform_buffers_memory[i], 0, ubo_buffer_size, 0, &renderer->uniform_buffers_mapped_memory[i]);
    }

    VkDeviceSize model_buffer_size = game->block_count * sizeof(Block_Info);
    for (i32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vulkan_create_buffer(vulkan, model_buffer_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &renderer->model_buffers[i], &renderer->model_buffers_memory[i]);
        vkMapMemory(vulkan->logical_device, renderer->model_buffers_memory[i], 0, model_buffer_size, 0, &renderer->model_buffers_mapped_memory[i]);
    }

    VkDeviceSize block_info_buffer_size = game->block_count * sizeof(Block_Info);
    for (i32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vulkan_create_buffer(vulkan, block_info_buffer_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &renderer->block_info_buffers[i], &renderer->block_info_buffers_memory[i]);
        vkMapMemory(vulkan->logical_device, renderer->block_info_buffers_memory[i], 0, block_info_buffer_size, 0, &renderer->block_info_buffers_mapped_memory[i]);
    }

    // Create descriptor sets from buffers
    // TODO: Move this out
    VkResult vulkan_result;

    i32 uniform_buffers_descriptor_count = 4;
    i32 sampler_descriptor_count = 2;

    VkDescriptorPoolSize pool_sizes[2];
    pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_sizes[0].descriptorCount = uniform_buffers_descriptor_count * (u32)(MAX_FRAMES_IN_FLIGHT);
    pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_sizes[1].descriptorCount = sampler_descriptor_count * (u32)(MAX_FRAMES_IN_FLIGHT);

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = ARRAY_LENGTH(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;
    pool_info.maxSets = (uniform_buffers_descriptor_count + sampler_descriptor_count) * (u32)(MAX_FRAMES_IN_FLIGHT);

    vulkan_result = vkCreateDescriptorPool(vulkan->logical_device, &pool_info, vulkan->allocator, &vulkan->descriptor_pool);
    assert(vulkan_result == VK_SUCCESS);

    {
        VkDescriptorSetLayout layouts[MAX_FRAMES_IN_FLIGHT];
        for (i32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            layouts[i] = renderer->texture_pipeline.descriptor_set_layout;
        }

        VkDescriptorSetAllocateInfo allocation_info = {};
        allocation_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocation_info.descriptorPool = vulkan->descriptor_pool;
        allocation_info.descriptorSetCount = (u32)(MAX_FRAMES_IN_FLIGHT);
        allocation_info.pSetLayouts = layouts;
        vulkan_result = vkAllocateDescriptorSets(vulkan->logical_device, &allocation_info, renderer->texture_pipeline_descriptor_sets);
        assert(vulkan_result == VK_SUCCESS);

        for (i32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorBufferInfo buffer_info = {};
            buffer_info.buffer = renderer->uniform_buffers[i];
            buffer_info.offset = 0;
            buffer_info.range = sizeof(Uniform_Buffer_Object);

            VkDescriptorImageInfo image_info = {};
            image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image_info.imageView = renderer->texture_view;
            image_info.sampler = renderer->texture_sampler;

            VkWriteDescriptorSet descriptor_writes[2] = {};
            descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor_writes[0].dstSet = renderer->texture_pipeline_descriptor_sets[i];
            descriptor_writes[0].dstBinding = 0;
            descriptor_writes[0].dstArrayElement = 0;
            descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptor_writes[0].descriptorCount = 1;
            descriptor_writes[0].pBufferInfo = &buffer_info;
            descriptor_writes[0].pImageInfo = 0;
            descriptor_writes[0].pTexelBufferView = 0;

            descriptor_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor_writes[1].dstSet = renderer->texture_pipeline_descriptor_sets[i];
            descriptor_writes[1].dstBinding = 1;
            descriptor_writes[1].dstArrayElement = 0;
            descriptor_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptor_writes[1].descriptorCount = 1;
            descriptor_writes[1].pBufferInfo = 0;
            descriptor_writes[1].pImageInfo = &image_info;
            descriptor_writes[1].pTexelBufferView = 0;

            vkUpdateDescriptorSets(vulkan->logical_device, ARRAY_LENGTH(descriptor_writes), descriptor_writes, 0, 0);
        }
    }

    {
        VkDescriptorSetLayout layouts[MAX_FRAMES_IN_FLIGHT];
        for (i32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            layouts[i] = renderer->block_pipeline.descriptor_set_layout;
        }

        VkDescriptorSetAllocateInfo allocation_info = {};
        allocation_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocation_info.descriptorPool = vulkan->descriptor_pool;
        allocation_info.descriptorSetCount = (u32)(MAX_FRAMES_IN_FLIGHT);
        allocation_info.pSetLayouts = layouts;
        vulkan_result = vkAllocateDescriptorSets(vulkan->logical_device, &allocation_info, renderer->block_pipeline_descriptor_sets);
        assert(vulkan_result == VK_SUCCESS);

        for (i32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorBufferInfo ubo_buffer_info = {};
            ubo_buffer_info.buffer = renderer->uniform_buffers[i];
            ubo_buffer_info.offset = 0;
            ubo_buffer_info.range = sizeof(Uniform_Buffer_Object);

            VkDescriptorImageInfo block_texture_info = {};
            block_texture_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            block_texture_info.imageView = renderer->texture_view;
            block_texture_info.sampler = renderer->block_texture_sampler;

            VkWriteDescriptorSet descriptor_writes[2] = {};
            i32 index = 0;
            descriptor_writes[index].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor_writes[index].dstSet = renderer->block_pipeline_descriptor_sets[i];
            descriptor_writes[index].dstBinding = 0;
            descriptor_writes[index].dstArrayElement = 0;
            descriptor_writes[index].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptor_writes[index].descriptorCount = 1;
            descriptor_writes[index].pBufferInfo = &ubo_buffer_info;
            index += 1;
            descriptor_writes[index].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor_writes[index].dstSet = renderer->block_pipeline_descriptor_sets[i];
            descriptor_writes[index].dstBinding = 1;
            descriptor_writes[index].dstArrayElement = 0;
            descriptor_writes[index].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptor_writes[index].descriptorCount = 1;
            descriptor_writes[index].pImageInfo = &block_texture_info;
            index += 1;
            assert(index == ARRAY_LENGTH(descriptor_writes));

            vkUpdateDescriptorSets(vulkan->logical_device, ARRAY_LENGTH(descriptor_writes), descriptor_writes, 0, 0);
        }
    }

    {
        VkDescriptorSetLayout layouts[MAX_FRAMES_IN_FLIGHT];
        for (i32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            layouts[i] = renderer->flat_color_pipeline.descriptor_set_layout;
        }

        VkDescriptorSetAllocateInfo allocation_info = {};
        allocation_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocation_info.descriptorPool = vulkan->descriptor_pool;
        allocation_info.descriptorSetCount = (u32)(MAX_FRAMES_IN_FLIGHT);
        allocation_info.pSetLayouts = layouts;
        vulkan_result = vkAllocateDescriptorSets(vulkan->logical_device, &allocation_info, renderer->flat_color_pipeline_descriptor_sets);
        assert(vulkan_result == VK_SUCCESS);

        for (i32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorBufferInfo ubo_buffer_info = {};
            ubo_buffer_info.buffer = renderer->uniform_buffers[i];
            ubo_buffer_info.offset = 0;
            ubo_buffer_info.range = sizeof(Uniform_Buffer_Object);

            VkWriteDescriptorSet descriptor_writes[1] = {};
            descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor_writes[0].dstSet = renderer->flat_color_pipeline_descriptor_sets[i];
            descriptor_writes[0].dstBinding = 0;
            descriptor_writes[0].dstArrayElement = 0;
            descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptor_writes[0].descriptorCount = 1;
            descriptor_writes[0].pBufferInfo = &ubo_buffer_info;
            descriptor_writes[0].pImageInfo = 0;
            descriptor_writes[0].pTexelBufferView = 0;

            vkUpdateDescriptorSets(vulkan->logical_device, ARRAY_LENGTH(descriptor_writes), descriptor_writes, 0, 0);
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
    f32 dy = mouse_sensitivity * (f32)input->mouse_dy * delta_time;
    camera->yaw += -1 * dx;  // counter clockwise
    camera->pitch += dy;

    // Render
    {
        Vulkan_Boilerplate_Objects *vulkan = game->vulkan;
        Renderer *renderer = game->renderer;
        VkResult vulkan_result;

        {
            glm::mat4 view = glm::lookAt(camera->position, camera->position + camera_forward, glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 projection = glm::perspective(glm::radians(45.0f), (f32)vulkan->swapchain_extent.width / (f32)vulkan->swapchain_extent.height, 0.1f, 50.0f);

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
        clear_colors[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
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

        vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdSetViewport(command_buffer, 0, 1, &viewport);
        vkCmdSetScissor(command_buffer, 0, 1, &scissor);
        if (game->wireframe_mode) {
            vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->block_wireframe_pipeline.pipeline);
            vkCmdSetLineWidth(command_buffer, 3.0f);
            vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->block_wireframe_pipeline.layout, 0, 1, &renderer->block_pipeline_descriptor_sets[frame_in_flight_index], 0, 0);
        } else {
            vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->block_pipeline.pipeline);
            vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->block_pipeline.layout, 0, 1, &renderer->block_pipeline_descriptor_sets[frame_in_flight_index], 0, 0);
        }
        vkCmdBindIndexBuffer(command_buffer, renderer->index_buffer, 0, VK_INDEX_TYPE_UINT16);
        vkCmdBindVertexBuffers(command_buffer, 0, ARRAY_LENGTH(vertex_buffers), vertex_buffers, offsets);
        vkCmdDrawIndexed(command_buffer, renderer->indices_count, game->block_count, 0, 0, 0);

        vkCmdEndRenderPass(command_buffer);

        vulkan_result = vkEndCommandBuffer(command_buffer);
        assert(vulkan_result == VK_SUCCESS);
    }
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
