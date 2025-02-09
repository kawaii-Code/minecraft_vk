float block_extent = 0.5f;

float uv_min = 0.0f;
float uv_max = 1.0f;

Block_Vertex single_block_vertex_positions[] = {
    // Front
    { {-block_extent, -block_extent, -block_extent}, { uv_min, uv_min } },
    { {-block_extent,  block_extent, -block_extent}, { uv_min, uv_max } },
    { { block_extent, -block_extent, -block_extent}, { uv_max, uv_min } },
    { { block_extent,  block_extent, -block_extent}, { uv_max, uv_max } },
    // Top
    { {-block_extent, -block_extent, -block_extent}, { 2 + uv_min, uv_min } },
    { {-block_extent, -block_extent,  block_extent}, { 2 + uv_min, uv_max } },
    { { block_extent, -block_extent, -block_extent}, { 2 + uv_max, uv_min } },
    { { block_extent, -block_extent,  block_extent}, { 2 + uv_max, uv_max } },
    // Right
    { { block_extent, -block_extent, -block_extent}, { uv_max, uv_min } },
    { { block_extent, -block_extent,  block_extent}, { uv_min, uv_min } },
    { { block_extent,  block_extent, -block_extent}, { uv_max, uv_max } },
    { { block_extent,  block_extent,  block_extent}, { uv_min, uv_max } },
    // Back
    { {-block_extent, -block_extent,  block_extent}, { uv_min, uv_min } },
    { {-block_extent,  block_extent,  block_extent}, { uv_min, uv_max } },
    { { block_extent, -block_extent,  block_extent}, { uv_max, uv_min } },
    { { block_extent,  block_extent,  block_extent}, { uv_max, uv_max } },
    // Bottom
    { {-block_extent,  block_extent, -block_extent}, { 1 + uv_min, uv_min } },
    { {-block_extent,  block_extent,  block_extent}, { 1 + uv_min, uv_max } },
    { { block_extent,  block_extent, -block_extent}, { 1 + uv_max, uv_min } },
    { { block_extent,  block_extent,  block_extent}, { 1 + uv_max, uv_max } },
    // Left
    { {-block_extent, -block_extent, -block_extent}, { uv_max, uv_min } },
    { {-block_extent, -block_extent,  block_extent}, { uv_min, uv_min } },
    { {-block_extent,  block_extent, -block_extent}, { uv_max, uv_max } },
    { {-block_extent,  block_extent,  block_extent}, { uv_min, uv_max } },
};

u16 single_block_indices[] = {
    0,  1,  2,  1,  3,  2,
    4,  6,  5,  5,  6,  7,
    8,  10, 9,  9,  10, 11,
    12, 14, 13, 13, 14, 15,
    16, 17, 18, 17, 19, 18,
    20, 21, 22, 21, 23, 22,
};


void vulkan_renderer_init(Vulkan_Boilerplate_Objects *vulkan, Renderer *renderer) {
    ZoneScoped;

    VkResult vulkan_result;

    // Create render pass
    VkRenderPass render_pass;
    {
        // This attachment is where the out_color from the fragment shader goes
        VkAttachmentDescription color_attachment = {};
        color_attachment.format = vulkan->swapchain_format.format;
        color_attachment.samples = vulkan->msaa_samples;
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color_attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription depth_attachment = {};
        depth_attachment.format = renderer->depth_texture_format;
        depth_attachment.samples = vulkan->msaa_samples;
        depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription color_resolve_attachment = {};
        color_resolve_attachment.format = vulkan->swapchain_format.format;
        color_resolve_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        color_resolve_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_resolve_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_resolve_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_resolve_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_resolve_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color_resolve_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference color_attachment_reference = {};
        color_attachment_reference.attachment = 0;
        color_attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depth_attachment_reference = {};
        depth_attachment_reference.attachment = 1;
        depth_attachment_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference color_resolve_attachment_reference = {};
        color_resolve_attachment_reference.attachment = 2;
        color_resolve_attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment_reference;
        subpass.pDepthStencilAttachment = &depth_attachment_reference;
        subpass.pResolveAttachments = &color_resolve_attachment_reference;

        VkSubpassDependency dependency = {};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        VkAttachmentDescription attachments[] = {
            color_attachment,
            depth_attachment,
            color_resolve_attachment,
        };

        VkRenderPassCreateInfo render_pass_create_info = {};
        render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_create_info.attachmentCount = ARRAY_LENGTH(attachments);
        render_pass_create_info.pAttachments = attachments;
        render_pass_create_info.subpassCount = 1;
        render_pass_create_info.pSubpasses = &subpass;
        render_pass_create_info.dependencyCount = 1;
        render_pass_create_info.pDependencies = &dependency;
        vulkan_result = vkCreateRenderPass(vulkan->logical_device, &render_pass_create_info, vulkan->allocator, &render_pass);
        assert(vulkan_result == VK_SUCCESS);
    }

    renderer->render_pass = render_pass;

    // Create graphics pipelines
    //
    // First listed are the fixed function stages
    // which are shared between multiple pipelines.
    //
    // Then the pipelines are separately created along
    // with their unique stages.

    i32 index;  // For occasional indexing

    VkPipelineInputAssemblyStateCreateInfo input_assembly_stage = {};
    input_assembly_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_stage.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly_stage.primitiveRestartEnable = VK_FALSE;

    VkDynamicState dynamic_win32_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_LINE_WIDTH,
    };
    VkPipelineDynamicStateCreateInfo dynamic_states = {};
    dynamic_states.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_states.dynamicStateCount = (u32)(ARRAY_LENGTH(dynamic_win32_states));
    dynamic_states.pDynamicStates = dynamic_win32_states;

    // Ignored, as viewport is dynamic
    VkPipelineViewportStateCreateInfo viewport_stage = {};
    viewport_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_stage.viewportCount = 1;
    viewport_stage.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterization_stage = {};
    rasterization_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_stage.depthClampEnable = VK_FALSE;
    rasterization_stage.rasterizerDiscardEnable = VK_FALSE;
    rasterization_stage.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization_stage.lineWidth = 1.0f;
    rasterization_stage.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization_stage.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterization_stage.depthBiasEnable = VK_FALSE;
    rasterization_stage.depthBiasConstantFactor = 0.0f;
    rasterization_stage.depthBiasClamp = 0.0f;
    rasterization_stage.depthBiasSlopeFactor = 0.0f;

    VkPipelineMultisampleStateCreateInfo multisampling_stage = {};
    multisampling_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling_stage.sampleShadingEnable = VK_TRUE;
    multisampling_stage.rasterizationSamples = vulkan->msaa_samples;
    multisampling_stage.minSampleShading = 0.2f;
    multisampling_stage.pSampleMask = 0;
    multisampling_stage.alphaToCoverageEnable = VK_FALSE;
    multisampling_stage.alphaToOneEnable = VK_FALSE;

    VkPipelineDepthStencilStateCreateInfo depth_stencil_stage = {};
    depth_stencil_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil_stage.depthTestEnable = VK_TRUE;
    depth_stencil_stage.depthWriteEnable = VK_TRUE;
    depth_stencil_stage.depthCompareOp = VK_COMPARE_OP_LESS;
    depth_stencil_stage.depthBoundsTestEnable = VK_FALSE;
    depth_stencil_stage.minDepthBounds = 0.0f;
    depth_stencil_stage.maxDepthBounds = 1.0f;
    depth_stencil_stage.stencilTestEnable = VK_FALSE;
    depth_stencil_stage.front = {};
    depth_stencil_stage.back = {};

    // Enable alpha blending
    VkPipelineColorBlendAttachmentState color_blend_attachment = {};
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_TRUE;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    VkPipelineColorBlendStateCreateInfo color_blend_stage = {};
    color_blend_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_stage.logicOpEnable = VK_FALSE;
    color_blend_stage.logicOp = VK_LOGIC_OP_COPY;
    color_blend_stage.attachmentCount = 1;
    color_blend_stage.pAttachments = &color_blend_attachment;
    color_blend_stage.blendConstants[0] = 0.0f;
    color_blend_stage.blendConstants[1] = 0.0f;
    color_blend_stage.blendConstants[2] = 0.0f;
    color_blend_stage.blendConstants[3] = 0.0f;

    // Configure texture pipeline
    {
        Renderer_Pipeline texture_pipeline;

        String vertex_shader_source = win32_read_entire_file("texture_vertex.spv");
        String fragment_shader_source = win32_read_entire_file("texture_fragment.spv");
        VkShaderModule vertex_shader_module = vulkan_create_shader_module(vulkan->logical_device, vertex_shader_source, vulkan->allocator);
        VkShaderModule fragment_shader_module = vulkan_create_shader_module(vulkan->logical_device, fragment_shader_source, vulkan->allocator);

        VkVertexInputBindingDescription binding_description = {};
        binding_description.binding = 0;
        binding_description.stride = sizeof(Vertex);
        binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription attribute_descriptions[3] = {};
        index = 0;
        attribute_descriptions[index].binding = 0;
        attribute_descriptions[index].location = 0;
        attribute_descriptions[index].format = VK_FORMAT_R32G32B32_SFLOAT;
        attribute_descriptions[index].offset = offsetof(Vertex, position);
        index += 1;
        attribute_descriptions[index].binding = 0;
        attribute_descriptions[index].location = 1;
        attribute_descriptions[index].format = VK_FORMAT_R32G32B32_SFLOAT;
        attribute_descriptions[index].offset = offsetof(Vertex, color);
        index += 1;
        attribute_descriptions[index].binding = 0;
        attribute_descriptions[index].location = 2;
        attribute_descriptions[index].format = VK_FORMAT_R32G32_SFLOAT;
        attribute_descriptions[index].offset = offsetof(Vertex, texture_coordinates);
        index += 1;
        assert(index == ARRAY_LENGTH(attribute_descriptions));

        VkPipelineVertexInputStateCreateInfo vertex_input_stage = {};
        vertex_input_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_stage.vertexBindingDescriptionCount = 1;
        vertex_input_stage.pVertexBindingDescriptions = &binding_description;
        vertex_input_stage.vertexAttributeDescriptionCount = ARRAY_LENGTH(attribute_descriptions);
        vertex_input_stage.pVertexAttributeDescriptions = attribute_descriptions;

        VkPipelineShaderStageCreateInfo vertex_shader_stage_create_info = {};
        vertex_shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertex_shader_stage_create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertex_shader_stage_create_info.module = vertex_shader_module;
        vertex_shader_stage_create_info.pName = "main";

        VkPipelineShaderStageCreateInfo fragment_shader_stage_create_info = {};
        fragment_shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragment_shader_stage_create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragment_shader_stage_create_info.module = fragment_shader_module;
        fragment_shader_stage_create_info.pName = "main";

        VkPipelineShaderStageCreateInfo shader_stages[2] = {
            vertex_shader_stage_create_info,
            fragment_shader_stage_create_info,
        };

        VkDescriptorSetLayoutBinding layout_bindings[2] = {};
        index = 0;
        layout_bindings[index].binding = 0;
        layout_bindings[index].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        layout_bindings[index].descriptorCount = 1;
        layout_bindings[index].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        layout_bindings[index].pImmutableSamplers = 0;
        index += 1;
        layout_bindings[index].binding = 1;
        layout_bindings[index].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        layout_bindings[index].descriptorCount = 1;
        layout_bindings[index].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        layout_bindings[index].pImmutableSamplers = 0;
        index += 1;
        assert(index == ARRAY_LENGTH(layout_bindings));

        texture_pipeline = vulkan_renderer_create_pipeline(
            vulkan,
            renderer->render_pass,
            layout_bindings, ARRAY_LENGTH(layout_bindings),
            0, 0,
            shader_stages, ARRAY_LENGTH(shader_stages),
            &input_assembly_stage,
            &vertex_input_stage,
            &dynamic_states,
            &viewport_stage,
            &rasterization_stage,
            &multisampling_stage,
            &depth_stencil_stage,
            &color_blend_stage
        );

        renderer->texture_pipeline = texture_pipeline;

        VirtualFree(vertex_shader_source.ptr, 0, MEM_RELEASE);
        VirtualFree(fragment_shader_source.ptr, 0, MEM_RELEASE);
        vkDestroyShaderModule(vulkan->logical_device, fragment_shader_module, vulkan->allocator);
        vkDestroyShaderModule(vulkan->logical_device, vertex_shader_module, vulkan->allocator);
    }

    // Configure block pipeline
    {
        Renderer_Pipeline block_pipeline;

        VkSampler block_texture_sampler;
        VkSamplerCreateInfo sampler_create_info = {};
        sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_create_info.magFilter = VK_FILTER_NEAREST;
        sampler_create_info.minFilter = VK_FILTER_NEAREST;
        sampler_create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        sampler_create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        sampler_create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        sampler_create_info.anisotropyEnable = VK_FALSE;
        sampler_create_info.maxAnisotropy = 0;
        sampler_create_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_WHITE;
        sampler_create_info.unnormalizedCoordinates = VK_FALSE;
        sampler_create_info.compareEnable = VK_FALSE;
        sampler_create_info.compareOp = VK_COMPARE_OP_ALWAYS;
        sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler_create_info.mipLodBias = 0.0f;
        sampler_create_info.minLod = 0.0f;
        sampler_create_info.maxLod = 0.0f;
        vulkan_result = vkCreateSampler(vulkan->logical_device, &sampler_create_info, vulkan->allocator, &block_texture_sampler);
        assert(vulkan_result == VK_SUCCESS);

        String vertex_shader_source = win32_read_entire_file("block_vertex.spv");
        String fragment_shader_source = win32_read_entire_file("block_fragment.spv");
        VkShaderModule vertex_shader_module = vulkan_create_shader_module(vulkan->logical_device, vertex_shader_source, vulkan->allocator);
        VkShaderModule fragment_shader_module = vulkan_create_shader_module(vulkan->logical_device, fragment_shader_source, vulkan->allocator);

        VkVertexInputBindingDescription binding_descriptions[2] = {};
        index = 0;
        binding_descriptions[index].binding = 0;
        binding_descriptions[index].stride = sizeof(Block_Vertex);
        binding_descriptions[index].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        index += 1;
        binding_descriptions[index].binding = 1;
        binding_descriptions[index].stride = sizeof(Block_Info);
        binding_descriptions[index].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
        index += 1;
        assert(index == ARRAY_LENGTH(binding_descriptions));

        VkVertexInputAttributeDescription attribute_descriptions[4] = {};
        index = 0;
        attribute_descriptions[index].binding = 0;
        attribute_descriptions[index].location = 0;
        attribute_descriptions[index].format = VK_FORMAT_R32G32B32_SFLOAT;
        attribute_descriptions[index].offset = offsetof(Block_Vertex, position);
        index += 1;
        attribute_descriptions[index].binding = 0;
        attribute_descriptions[index].location = 1;
        attribute_descriptions[index].format = VK_FORMAT_R32G32_SFLOAT;
        attribute_descriptions[index].offset = offsetof(Block_Vertex, texture_coordinates);
        index += 1;
        attribute_descriptions[index].binding = 1;
        attribute_descriptions[index].location = 2;
        attribute_descriptions[index].format = VK_FORMAT_R32G32B32_SFLOAT;
        attribute_descriptions[index].offset = offsetof(Block_Info, world_position);
        index += 1;
        attribute_descriptions[index].binding = 1;
        attribute_descriptions[index].location = 3;
        attribute_descriptions[index].format = VK_FORMAT_R32_SINT;
        attribute_descriptions[index].offset = offsetof(Block_Info, block_type);
        index += 1;
        assert(index == ARRAY_LENGTH(attribute_descriptions));

        VkPipelineVertexInputStateCreateInfo vertex_input_stage = {};
        vertex_input_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_stage.vertexBindingDescriptionCount = ARRAY_LENGTH(binding_descriptions);
        vertex_input_stage.pVertexBindingDescriptions = binding_descriptions;
        vertex_input_stage.vertexAttributeDescriptionCount = ARRAY_LENGTH(attribute_descriptions);
        vertex_input_stage.pVertexAttributeDescriptions = attribute_descriptions;

        VkPipelineShaderStageCreateInfo vertex_shader_stage_create_info = {};
        vertex_shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertex_shader_stage_create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertex_shader_stage_create_info.module = vertex_shader_module;
        vertex_shader_stage_create_info.pName = "main";

        VkPipelineShaderStageCreateInfo fragment_shader_stage_create_info = {};
        fragment_shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragment_shader_stage_create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragment_shader_stage_create_info.module = fragment_shader_module;
        fragment_shader_stage_create_info.pName = "main";

        VkPipelineShaderStageCreateInfo shader_stages[] = {
            vertex_shader_stage_create_info,
            fragment_shader_stage_create_info,
        };

        VkDescriptorSetLayoutBinding layout_bindings[2] = {};
        index = 0;
        layout_bindings[index].binding = 0;
        layout_bindings[index].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        layout_bindings[index].descriptorCount = 1;
        layout_bindings[index].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        index += 1;
        layout_bindings[index].binding = 1;
        layout_bindings[index].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        layout_bindings[index].descriptorCount = 1;
        layout_bindings[index].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        layout_bindings[index].pImmutableSamplers = &block_texture_sampler;
        index += 1;
        assert(index == ARRAY_LENGTH(layout_bindings));

        block_pipeline = vulkan_renderer_create_pipeline(
            vulkan,
            renderer->render_pass,
            layout_bindings, ARRAY_LENGTH(layout_bindings),
            0, 0,
            shader_stages, ARRAY_LENGTH(shader_stages),
            &input_assembly_stage,
            &vertex_input_stage,
            &dynamic_states,
            &viewport_stage,
            &rasterization_stage,
            &multisampling_stage,
            &depth_stencil_stage,
            &color_blend_stage
        );

        rasterization_stage.polygonMode = VK_POLYGON_MODE_LINE;
        renderer->block_wireframe_pipeline = vulkan_renderer_create_pipeline(
            vulkan,
            renderer->render_pass,
            layout_bindings, ARRAY_LENGTH(layout_bindings),
            0, 0,
            shader_stages, ARRAY_LENGTH(shader_stages),
            &input_assembly_stage,
            &vertex_input_stage,
            &dynamic_states,
            &viewport_stage,
            &rasterization_stage,
            &multisampling_stage,
            &depth_stencil_stage,
            &color_blend_stage
        );
        rasterization_stage.polygonMode = VK_POLYGON_MODE_FILL;

        renderer->block_pipeline = block_pipeline;
        renderer->block_texture_sampler = block_texture_sampler;

        VirtualFree(vertex_shader_source.ptr, 0, MEM_RELEASE);
        VirtualFree(fragment_shader_source.ptr, 0, MEM_RELEASE);
        vkDestroyShaderModule(vulkan->logical_device, fragment_shader_module, vulkan->allocator);
        vkDestroyShaderModule(vulkan->logical_device, vertex_shader_module, vulkan->allocator);
    }


    // Configure flat color pipeline
    {
        Renderer_Pipeline flat_color_pipeline;

        String vertex_shader_source = win32_read_entire_file("flat_color_vertex.spv");
        String fragment_shader_source = win32_read_entire_file("flat_color_fragment.spv");

        VkShaderModule vertex_shader_module = vulkan_create_shader_module(vulkan->logical_device, vertex_shader_source, vulkan->allocator);
        VkShaderModule fragment_shader_module = vulkan_create_shader_module(vulkan->logical_device, fragment_shader_source, vulkan->allocator);

        VkVertexInputBindingDescription binding_descriptions[2] = {};
        index = 0;
        binding_descriptions[index].binding = 0;
        binding_descriptions[index].stride = sizeof(Vertex_With_Position);
        binding_descriptions[index].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        index += 1;
        binding_descriptions[index].binding = 1;
        binding_descriptions[index].stride = sizeof(Block_Info);
        binding_descriptions[index].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
        index += 1;
        assert(index == ARRAY_LENGTH(binding_descriptions));

        VkVertexInputAttributeDescription attribute_descriptions[4] = {};
        index = 0;
        attribute_descriptions[index].binding = 0;
        attribute_descriptions[index].location = 0;
        attribute_descriptions[index].format = VK_FORMAT_R32G32B32_SFLOAT;
        attribute_descriptions[index].offset = offsetof(Vertex_With_Position, position);
        index += 1;
        attribute_descriptions[index].binding = 1;
        attribute_descriptions[index].location = 1;
        attribute_descriptions[index].format = VK_FORMAT_R32G32B32_SFLOAT;
        attribute_descriptions[index].offset = 0;
        index += 1;
        attribute_descriptions[index].binding = 1;
        attribute_descriptions[index].location = 2;
        attribute_descriptions[index].format = VK_FORMAT_R32G32B32_SFLOAT;
        attribute_descriptions[index].offset = 12;
        index += 1;
        attribute_descriptions[index].binding = 1;
        attribute_descriptions[index].location = 3;
        attribute_descriptions[index].format = VK_FORMAT_R32G32B32_SFLOAT;
        attribute_descriptions[index].offset = 24;
        index += 1;
        assert(index == ARRAY_LENGTH(attribute_descriptions));

        VkPipelineVertexInputStateCreateInfo vertex_input_stage = {};
        vertex_input_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_stage.vertexBindingDescriptionCount = ARRAY_LENGTH(binding_descriptions);
        vertex_input_stage.pVertexBindingDescriptions = binding_descriptions;
        vertex_input_stage.vertexAttributeDescriptionCount = ARRAY_LENGTH(attribute_descriptions);
        vertex_input_stage.pVertexAttributeDescriptions = attribute_descriptions;

        VkPipelineShaderStageCreateInfo vertex_shader_stage_create_info = {};
        vertex_shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertex_shader_stage_create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertex_shader_stage_create_info.module = vertex_shader_module;
        vertex_shader_stage_create_info.pName = "main";

        VkPipelineShaderStageCreateInfo fragment_shader_stage_create_info = {};
        fragment_shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragment_shader_stage_create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragment_shader_stage_create_info.module = fragment_shader_module;
        fragment_shader_stage_create_info.pName = "main";

        VkPipelineShaderStageCreateInfo shader_stages[] = {
            vertex_shader_stage_create_info,
            fragment_shader_stage_create_info,
        };

        VkDescriptorSetLayoutBinding layout_bindings[1] = {};
        index = 0;
        layout_bindings[index].binding = 0;
        layout_bindings[index].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        layout_bindings[index].descriptorCount = 1;
        layout_bindings[index].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        layout_bindings[index].pImmutableSamplers = 0;
        index += 1;
        assert(index == ARRAY_LENGTH(layout_bindings));

        VkPushConstantRange push_constant_range = {};
        push_constant_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        push_constant_range.offset = 0;
        push_constant_range.size = sizeof(glm::vec3);

        flat_color_pipeline = vulkan_renderer_create_pipeline(
            vulkan,
            renderer->render_pass,
            layout_bindings, ARRAY_LENGTH(layout_bindings),
            &push_constant_range, 1,
            shader_stages, ARRAY_LENGTH(shader_stages),
            &input_assembly_stage,
            &vertex_input_stage,
            &dynamic_states,
            &viewport_stage,
            &rasterization_stage,
            &multisampling_stage,
            &depth_stencil_stage,
            &color_blend_stage
        );

        renderer->flat_color_pipeline = flat_color_pipeline;

        VirtualFree(vertex_shader_source.ptr, 0, MEM_RELEASE);
        VirtualFree(fragment_shader_source.ptr, 0, MEM_RELEASE);
        vkDestroyShaderModule(vulkan->logical_device, fragment_shader_module, vulkan->allocator);
        vkDestroyShaderModule(vulkan->logical_device, vertex_shader_module, vulkan->allocator);
    }

    // Send single block vertex data to GPU
    {
        Block_Vertex *vertices = single_block_vertex_positions;
        i32 vertex_count = ARRAY_LENGTH(single_block_vertex_positions);
        VkDeviceSize vertex_buffer_size = vertex_count * sizeof(*vertices);
        VkBuffer vertex_buffer;
        VkDeviceMemory vertex_buffer_memory;
        {
            VkBuffer staging_buffer;
            VkDeviceMemory staging_buffer_memory;
            vulkan_create_buffer(vulkan, vertex_buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staging_buffer, &staging_buffer_memory);

            void *mapped_memory;
            vkMapMemory(vulkan->logical_device, staging_buffer_memory, 0, vertex_buffer_size, 0, &mapped_memory);
            for (i32 i = 0; i < vertex_count; i++) {
                *((Block_Vertex *)mapped_memory + i) = vertices[i];
            }
            vkUnmapMemory(vulkan->logical_device, staging_buffer_memory);

            vulkan_create_buffer(vulkan, vertex_buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &vertex_buffer, &vertex_buffer_memory);

            vulkan_copy_buffer(vulkan, staging_buffer, vertex_buffer, vertex_buffer_size);

            vkDestroyBuffer(vulkan->logical_device, staging_buffer, vulkan->allocator);
            vkFreeMemory(vulkan->logical_device, staging_buffer_memory, vulkan->allocator);
        }

        u16 *indices = single_block_indices;
        i32 indices_count = ARRAY_LENGTH(single_block_indices);
        VkDeviceSize index_buffer_size = indices_count * sizeof(*indices);
        VkBuffer index_buffer;
        VkDeviceMemory index_buffer_memory;
        {
            VkBuffer staging_buffer;
            VkDeviceMemory staging_buffer_memory;
            vulkan_create_buffer(vulkan, index_buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staging_buffer, &staging_buffer_memory);

            void *mapped_memory;
            vkMapMemory(vulkan->logical_device, staging_buffer_memory, 0, index_buffer_size, 0, &mapped_memory);
            for (i32 i = 0; i < indices_count; i++) {
                *((u16 *)mapped_memory + i) = indices[i];
            }
            vkUnmapMemory(vulkan->logical_device, staging_buffer_memory);

            vulkan_create_buffer(vulkan, index_buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &index_buffer, &index_buffer_memory);

            vulkan_copy_buffer(vulkan, staging_buffer, index_buffer, index_buffer_size);

            vkDestroyBuffer(vulkan->logical_device, staging_buffer, vulkan->allocator);
            vkFreeMemory(vulkan->logical_device, staging_buffer_memory, vulkan->allocator);
        }

        renderer->vertex_buffer = vertex_buffer;
        renderer->vertex_buffer_memory = vertex_buffer_memory;
        renderer->vertex_count = vertex_count;
        renderer->index_buffer = index_buffer;
        renderer->index_buffer_memory = index_buffer_memory;
        renderer->indices_count = indices_count;
    }
}


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
) {
    Renderer_Pipeline result;
    VkResult vulkan_result;

    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info = {};
    descriptor_set_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptor_set_layout_create_info.bindingCount = descriptor_set_layout_bindings_count;
    descriptor_set_layout_create_info.pBindings = descriptor_set_layout_bindings;
    vulkan_result = vkCreateDescriptorSetLayout(vulkan->logical_device, &descriptor_set_layout_create_info, vulkan->allocator, &result.descriptor_set_layout);
    assert(vulkan_result == VK_SUCCESS);

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {};
    pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_create_info.setLayoutCount = 1;
    pipeline_layout_create_info.pSetLayouts = &result.descriptor_set_layout;
    pipeline_layout_create_info.pushConstantRangeCount = push_constant_ranges_count;
    pipeline_layout_create_info.pPushConstantRanges = push_constant_ranges;
    vulkan_result = vkCreatePipelineLayout(vulkan->logical_device, &pipeline_layout_create_info, vulkan->allocator, &result.layout);
    assert(vulkan_result == VK_SUCCESS);

    VkGraphicsPipelineCreateInfo pipeline_create_info = {};
    pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_create_info.stageCount = shader_stages_count;
    pipeline_create_info.pStages = shader_stages;
    pipeline_create_info.pVertexInputState = vertex_input_stage;
    pipeline_create_info.pInputAssemblyState = input_assembly_stage;
    pipeline_create_info.pViewportState = viewport_stage;
    pipeline_create_info.pRasterizationState = rasterization_stage;
    pipeline_create_info.pMultisampleState = multisampling_stage;
    pipeline_create_info.pDepthStencilState = depth_stencil_stage;
    pipeline_create_info.pColorBlendState = color_blend_stage;
    pipeline_create_info.pDynamicState = dynamic_states;
    pipeline_create_info.layout = result.layout;
    pipeline_create_info.renderPass = render_pass;
    pipeline_create_info.subpass = 0;
    pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_create_info.basePipelineIndex = -1;

    vkCreateGraphicsPipelines(vulkan->logical_device, 0, 1, &pipeline_create_info, vulkan->allocator, &result.pipeline);

    return result;
}
