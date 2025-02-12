void vulkan_init(Vulkan_Boilerplate_Objects *vulkan_state, Renderer *renderer, Win32_State *win32_state) {
    Vulkan_Boilerplate_Objects vulkan = *vulkan_state;
    VkResult vulkan_result;  // Store error return codes

    // Create vulkan instance
    {
        VkApplicationInfo vulkan_app_info = {};
        vulkan_app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        vulkan_app_info.pApplicationName = PROGRAM_TITLE;
        vulkan_app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        vulkan_app_info.pEngineName = "No Engine";
        vulkan_app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        vulkan_app_info.apiVersion = VK_API_VERSION_1_0;

#ifdef USE_VULKAN_DEBUG_LAYERS
        char *vulkan_win32_extensions[] = {
            "VK_KHR_surface",
            "VK_KHR_win32_surface",
            "VK_EXT_debug_utils",
        };
        char *vulkan_validation_layers[] = {
            "VK_LAYER_KHRONOS_validation",
        };
        u32 enabled_layer_count = 1;
#else
        char *vulkan_win32_extensions[] = {
            "VK_KHR_surface",
            "VK_KHR_win32_surface",
        };
        char **vulkan_validation_layers = 0;
        u32 enabled_layer_count = 0;
#endif
        VkInstanceCreateInfo instance_ci = {};
        instance_ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instance_ci.pApplicationInfo = &vulkan_app_info;
        instance_ci.enabledExtensionCount = ARRAY_LENGTH(vulkan_win32_extensions);
        instance_ci.ppEnabledExtensionNames = vulkan_win32_extensions;
        instance_ci.enabledLayerCount = enabled_layer_count;
        instance_ci.ppEnabledLayerNames = vulkan_validation_layers;
        vulkan_result = vkCreateInstance(&instance_ci, vulkan.allocator, &vulkan.instance);
        if (vulkan_result != VK_SUCCESS) {
            win32_fatal("Failed to create vulkan instance!\n");
        }
    }

#ifdef USE_VULKAN_DEBUG_LAYERS
    // Create debug messenger
    {
        VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info = {};
        debug_messenger_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debug_messenger_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debug_messenger_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debug_messenger_create_info.pfnUserCallback = vulkan_debug_callback;
        debug_messenger_create_info.pUserData = 0;
        PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vulkan.instance, "vkCreateDebugUtilsMessengerEXT");
        assert(vkCreateDebugUtilsMessengerEXT != 0);
        vulkan_result = vkCreateDebugUtilsMessengerEXT(vulkan.instance, &debug_messenger_create_info, vulkan.allocator, &vulkan.debug_messenger);
        assert(vulkan_result == VK_SUCCESS);
    }
#endif

    // Create physical device
    {
        u32 vulkan_physical_device_count = 0;
        vkEnumeratePhysicalDevices(vulkan.instance, &vulkan_physical_device_count, 0);
        if (vulkan_physical_device_count == 0) {
            win32_fatal("Failed to find any GPUs compatible with Vulkan!\n");
        }

        VkPhysicalDevice vulkan_physical_devices[8] = {};
        vkEnumeratePhysicalDevices(vulkan.instance, &vulkan_physical_device_count, vulkan_physical_devices);
        for (u32 i = 0; i < vulkan_physical_device_count; i++) {
            VkPhysicalDeviceProperties device_properties;
            vkGetPhysicalDeviceProperties(vulkan_physical_devices[i], &device_properties);
        }
        vulkan.physical_device = vulkan_physical_devices[0];  // Heuristic :)
        vkGetPhysicalDeviceProperties(vulkan.physical_device, &vulkan.physical_device_properties);

        vkGetPhysicalDeviceMemoryProperties(vulkan.physical_device, &vulkan.memory_properties);

        VkSampleCountFlags avaiable_sample_counts = vulkan.physical_device_properties.limits.framebufferColorSampleCounts &
                                                    vulkan.physical_device_properties.limits.framebufferDepthSampleCounts;
        if (avaiable_sample_counts & VK_SAMPLE_COUNT_64_BIT) {
            vulkan.msaa_samples = VK_SAMPLE_COUNT_64_BIT;
        } else if (avaiable_sample_counts & VK_SAMPLE_COUNT_32_BIT) {
            vulkan.msaa_samples = VK_SAMPLE_COUNT_32_BIT;
        } else if (avaiable_sample_counts & VK_SAMPLE_COUNT_16_BIT) {
            vulkan.msaa_samples = VK_SAMPLE_COUNT_16_BIT;
        } else if (avaiable_sample_counts & VK_SAMPLE_COUNT_8_BIT) {
            vulkan.msaa_samples = VK_SAMPLE_COUNT_8_BIT;
        } else if (avaiable_sample_counts & VK_SAMPLE_COUNT_4_BIT) {
            vulkan.msaa_samples = VK_SAMPLE_COUNT_4_BIT;
        } else if (avaiable_sample_counts & VK_SAMPLE_COUNT_2_BIT) {
            vulkan.msaa_samples = VK_SAMPLE_COUNT_2_BIT;
        } else {
            vulkan.msaa_samples = VK_SAMPLE_COUNT_1_BIT;
        }

        VkFormat selected_depth_texture_format = VK_FORMAT_UNDEFINED;
        VkFormat depth_texture_formats[] = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
        VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
        VkFormatFeatureFlags features = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
        for (i32 i = 0; i < ARRAY_LENGTH(depth_texture_formats); i++) {
            VkFormatProperties properties;
            vkGetPhysicalDeviceFormatProperties(vulkan.physical_device, depth_texture_formats[i], &properties);
            if (tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & features) == features) {
                selected_depth_texture_format = depth_texture_formats[i];
                break;
            } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & features) == features) {
                selected_depth_texture_format = depth_texture_formats[i];
                break;
            }
        }
        assert(selected_depth_texture_format != VK_FORMAT_UNDEFINED);

        renderer->depth_texture_format = selected_depth_texture_format;
    }

    // Create surface
    {
        VkWin32SurfaceCreateInfoKHR win32_surface_create_info = {};
        win32_surface_create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        win32_surface_create_info.hwnd = win32_state->window;
        win32_surface_create_info.hinstance = win32_state->instance;
        vulkan_result = vkCreateWin32SurfaceKHR(vulkan.instance, &win32_surface_create_info, vulkan.allocator, &vulkan.surface);
        assert(vulkan_result == VK_SUCCESS);
    }

    vulkan_choose_swapchain_parameters(&vulkan, win32_state);

    // Create logical device
    {
        u32 vulkan_queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(vulkan.physical_device, &vulkan_queue_family_count, 0);
        VkQueueFamilyProperties queue_family_properties[16];
        vkGetPhysicalDeviceQueueFamilyProperties(vulkan.physical_device, &vulkan_queue_family_count, queue_family_properties);

        vulkan.graphics_queue_family = 0xFFFFFFFF;
        vulkan.present_queue_family =  0xFFFFFFFF;
        for (u32 i = 0; i < vulkan_queue_family_count; i++) {
            if ((queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && vulkan.graphics_queue_family == 0xFFFFFFFF) {
                vulkan.graphics_queue_family = i;
            }
            VkBool32 present_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(vulkan.physical_device, i, vulkan.surface, &present_support);
            if (present_support && vulkan.present_queue_family == 0xFFFFFFFF) {
                vulkan.present_queue_family = i;
            }
        }
        assert(vulkan.graphics_queue_family != 0xFFFFFFFF);
        assert(vulkan.present_queue_family  != 0xFFFFFFFF);

        // NOTE: graphics and present queue are the same, but
        // I was lazy to add a check for that. This is incomplete.
        float queue_priority = 1.0f;
        VkDeviceQueueCreateInfo queue_create_infos[1];
        u32 queue_families[1] = {vulkan.graphics_queue_family};
        for (i32 i = 0; i < ARRAY_LENGTH(queue_create_infos); i++) {
            VkDeviceQueueCreateInfo *create_info = &queue_create_infos[i];
            *create_info = {};
            create_info->sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            create_info->queueFamilyIndex = queue_families[i];
            create_info->queueCount = 1;
            create_info->pQueuePriorities = &queue_priority;
        }

        // TODO: Check that these features are actually available on the device
        VkPhysicalDeviceFeatures device_features = {};
        device_features.samplerAnisotropy = VK_TRUE;
        device_features.sampleRateShading = VK_TRUE;
        device_features.fillModeNonSolid = VK_TRUE;
        device_features.wideLines = VK_TRUE;

        char *vulkan_device_extensions[] = {
            "VK_KHR_swapchain",
        };

        VkDeviceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        create_info.queueCreateInfoCount = ARRAY_LENGTH(queue_create_infos),
        create_info.pQueueCreateInfos = queue_create_infos,
        create_info.enabledExtensionCount = ARRAY_LENGTH(vulkan_device_extensions),
        create_info.ppEnabledExtensionNames = vulkan_device_extensions,
        create_info.pEnabledFeatures = &device_features,
        vulkan_result = vkCreateDevice(vulkan.physical_device, &create_info, vulkan.allocator, &vulkan.logical_device);
        assert(vulkan_result == VK_SUCCESS);

        u32 index = 0;
        vkGetDeviceQueue(vulkan.logical_device, vulkan.graphics_queue_family, index, &vulkan.graphics_queue);
        vkGetDeviceQueue(vulkan.logical_device, vulkan.present_queue_family, index, &vulkan.present_queue);
    }

    // Create command pool and main command buffers
    {
        VkCommandPoolCreateInfo command_pool_create_info = {};
        command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        command_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        command_pool_create_info.queueFamilyIndex = vulkan.graphics_queue_family;
        vulkan_result = vkCreateCommandPool(vulkan.logical_device, &command_pool_create_info, vulkan.allocator, &vulkan.command_pool);
        assert(vulkan_result == VK_SUCCESS);

        VkCommandBufferAllocateInfo buffer_allocate_info = {};
        buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        buffer_allocate_info.commandPool = vulkan.command_pool;
        buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        buffer_allocate_info.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
        vulkan_result = vkAllocateCommandBuffers(vulkan.logical_device, &buffer_allocate_info, vulkan.command_buffers);
        assert(vulkan_result == VK_SUCCESS);
    }

    // Create sync primitives
    {
        VkSemaphoreCreateInfo semaphore_create_info = {};
        semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        for (i32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vulkan_result = vkCreateSemaphore(vulkan.logical_device, &semaphore_create_info, vulkan.allocator, &vulkan.image_available_semaphores[i]);
            assert(vulkan_result == VK_SUCCESS);
            vulkan_result = vkCreateSemaphore(vulkan.logical_device, &semaphore_create_info, vulkan.allocator, &vulkan.render_finished_semaphores[i]);
            assert(vulkan_result == VK_SUCCESS);
        }

        VkFenceCreateInfo fence_create_info = {};
        fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        for (i32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vulkan_result = vkCreateFence(vulkan.logical_device, &fence_create_info, vulkan.allocator, &vulkan.in_flight_fences[i]);
            assert(vulkan_result == VK_SUCCESS);
        }
    }

    *vulkan_state = vulkan;
}


void vulkan_choose_swapchain_parameters(Vulkan_Boilerplate_Objects *vulkan, Win32_State *win32_state) {
    VkSurfaceCapabilitiesKHR capabilities;
    VkSurfaceFormatKHR formats[8] = {};
    VkPresentModeKHR present_modes[8] = {};

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vulkan->physical_device, vulkan->surface, &capabilities);

    u32 format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(vulkan->physical_device, vulkan->surface, &format_count, 0);
    if (format_count != 0) {
        vkGetPhysicalDeviceSurfaceFormatsKHR(vulkan->physical_device, vulkan->surface, &format_count, formats);
    }

    u32 present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(vulkan->physical_device, vulkan->surface, &present_mode_count, 0);
    if (present_mode_count != 0) {
        vkGetPhysicalDeviceSurfacePresentModesKHR(vulkan->physical_device, vulkan->surface, &present_mode_count, present_modes);
    }

    assert(format_count > 0 && present_mode_count > 0);

    // Choose format
    vulkan->swapchain_format = formats[0];
    for (u32 i = 0; i < format_count; i++) {
        VkSurfaceFormatKHR format = formats[i];
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            vulkan->swapchain_format = format;
            break;
        }
    }

    // Choose present mode
    vulkan->swapchain_present_mode = VK_PRESENT_MODE_FIFO_KHR;  // Guaranteed to be available
    for (u32 i = 0; i < present_mode_count; i++) {
        VkPresentModeKHR present_mode = present_modes[i];
        if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            vulkan->swapchain_present_mode = present_mode;
            break;
        }
    }

    // Choose extent
    if (capabilities.currentExtent.width != 0xFFFFFFFF) {
        vulkan->swapchain_extent = capabilities.currentExtent;
    } else {
        RECT area;
        GetClientRect(win32_state->window, &area);

        VkExtent2D actual_extent = {
            (u32)area.right,
            (u32)area.bottom,
        };

        actual_extent.width = clamp(actual_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actual_extent.height = clamp(actual_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        vulkan->swapchain_extent = actual_extent;
    }

    vulkan->swapchain_image_count = capabilities.minImageCount + 1;
    assert(capabilities.maxImageCount == 0 || capabilities.maxImageCount >= vulkan->swapchain_image_count);
}


void vulkan_create_swapchain(Vulkan_Boilerplate_Objects *vulkan_state, Renderer *renderer, Win32_State *win32_state) {
    Vulkan_Boilerplate_Objects vulkan = *vulkan_state;
    VkResult vulkan_result;  // For storing error codes

    // TODO: Delete this
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vulkan.physical_device, vulkan.surface, &capabilities);

    vulkan_choose_swapchain_parameters(&vulkan, win32_state);

    VkSwapchainCreateInfoKHR swapchain_create_info = {};
    swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_create_info.surface = vulkan.surface;
    swapchain_create_info.minImageCount = vulkan.swapchain_image_count;
    swapchain_create_info.imageFormat = vulkan.swapchain_format.format;
    swapchain_create_info.imageColorSpace = vulkan.swapchain_format.colorSpace;
    swapchain_create_info.imageExtent = vulkan.swapchain_extent;
    swapchain_create_info.imageArrayLayers = 1;
    swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (vulkan.graphics_queue_family != vulkan.present_queue_family) {
        u32 queue_families[] = {vulkan.graphics_queue_family, vulkan.present_queue_family};
        swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchain_create_info.queueFamilyIndexCount = 2;
        swapchain_create_info.pQueueFamilyIndices = queue_families;
    } else {
        swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchain_create_info.queueFamilyIndexCount = 0;
        swapchain_create_info.pQueueFamilyIndices = 0;
    }
    swapchain_create_info.preTransform = capabilities.currentTransform;
    swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;  // Ignore alpha blending with other windows
    swapchain_create_info.presentMode = vulkan.swapchain_present_mode;
    swapchain_create_info.clipped = VK_TRUE;
    swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;  // Swapchain needs to be recreated from scratch when a window is resized
    vulkan_result = vkCreateSwapchainKHR(vulkan.logical_device, &swapchain_create_info, vulkan.allocator, &vulkan.swapchain);
    if (vulkan_result != VK_SUCCESS) {
        win32_fatal("Failed to create a swapchain!\n");
    }

    vkGetSwapchainImagesKHR(vulkan.logical_device, vulkan.swapchain, &vulkan.swapchain_image_count, vulkan.swapchain_images);

    for (u32 i = 0; i < vulkan.swapchain_image_count; i++) {
        VkImageViewCreateInfo image_view_create_info = {};
        image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image_view_create_info.image = vulkan.swapchain_images[i];
        image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        image_view_create_info.format = vulkan.swapchain_format.format;
        image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_view_create_info.subresourceRange.baseMipLevel = 0;
        image_view_create_info.subresourceRange.levelCount = 1;
        image_view_create_info.subresourceRange.baseArrayLayer = 0;
        image_view_create_info.subresourceRange.layerCount = 1;
        vulkan_result = vkCreateImageView(vulkan.logical_device, &image_view_create_info, vulkan.allocator, &vulkan.swapchain_image_views[i]);
        assert(vulkan_result == VK_SUCCESS);
    }

    // Create depth texture
    {
        vulkan_create_image(
            &vulkan,
            vulkan.swapchain_extent.width,
            vulkan.swapchain_extent.height,
            vulkan.msaa_samples,
            renderer->depth_texture_format,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &renderer->depth_texture,
            &renderer->depth_texture_memory
        );

        VkImageView *image_view = &renderer->depth_texture_view;
        {
            VkImageViewCreateInfo texture_view_create_info = {};
            texture_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            texture_view_create_info.image = renderer->depth_texture;
            texture_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            texture_view_create_info.format = renderer->depth_texture_format;
            texture_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            texture_view_create_info.subresourceRange.baseMipLevel = 0;
            texture_view_create_info.subresourceRange.levelCount = 1;
            texture_view_create_info.subresourceRange.baseArrayLayer = 0;
            texture_view_create_info.subresourceRange.layerCount = 1;
            vulkan_result = vkCreateImageView(vulkan.logical_device, &texture_view_create_info, vulkan.allocator, image_view);
            assert(vulkan_result == VK_SUCCESS);
        }
    }

    // Create multisample texture
    {
        VkFormat multisample_texture_format = vulkan.swapchain_format.format;
        vulkan_create_image(
            &vulkan,
            vulkan.swapchain_extent.width,
            vulkan.swapchain_extent.height,
            vulkan.msaa_samples,
            multisample_texture_format,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &renderer->multisample_texture,
            &renderer->multisample_texture_memory
        );

        VkImageViewCreateInfo texture_view_create_info = {};
        texture_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        texture_view_create_info.image = renderer->multisample_texture;
        texture_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        texture_view_create_info.format = multisample_texture_format;
        texture_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        texture_view_create_info.subresourceRange.baseMipLevel = 0;
        texture_view_create_info.subresourceRange.levelCount = 1;
        texture_view_create_info.subresourceRange.baseArrayLayer = 0;
        texture_view_create_info.subresourceRange.layerCount = 1;
        vulkan_result = vkCreateImageView(vulkan.logical_device, &texture_view_create_info, vulkan.allocator, &renderer->multisample_texture_view);
        assert(vulkan_result == VK_SUCCESS);
    }

    for (u32 i = 0; i < vulkan.swapchain_image_count; i++) {
        VkImageView attachments[] = {
            renderer->multisample_texture_view,
            renderer->depth_texture_view,
            vulkan.swapchain_image_views[i],
        };

        VkFramebufferCreateInfo framebuffer_create_info = {};
        framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_create_info.renderPass = renderer->render_pass;
        framebuffer_create_info.attachmentCount = ARRAY_LENGTH(attachments);
        framebuffer_create_info.pAttachments = attachments;
        framebuffer_create_info.width = vulkan.swapchain_extent.width;
        framebuffer_create_info.height = vulkan.swapchain_extent.height;
        framebuffer_create_info.layers = 1;
        vulkan_result = vkCreateFramebuffer(vulkan.logical_device, &framebuffer_create_info, vulkan.allocator, &vulkan.swapchain_framebuffers[i]);
        assert(vulkan_result == VK_SUCCESS);
    }

    *vulkan_state = vulkan;
}


void vulkan_re_create_swapchain(Vulkan_Boilerplate_Objects *vulkan, Renderer *renderer, Win32_State *win32_state) {
    vkDeviceWaitIdle(vulkan->logical_device);

    vulkan_teardown_swapchain(vulkan, renderer);
    vulkan_create_swapchain(vulkan, renderer, win32_state);
}


void vulkan_teardown_swapchain(Vulkan_Boilerplate_Objects *vulkan, Renderer *renderer) {
    VkAllocationCallbacks *allocator = vulkan->allocator;
    VkDevice device = vulkan->logical_device;

    vkDestroyImageView(device, renderer->multisample_texture_view, allocator);
    vkDestroyImage(device, renderer->multisample_texture, allocator);
    vkFreeMemory(device, renderer->multisample_texture_memory, allocator);

    vkDestroyImageView(device, renderer->depth_texture_view, allocator);
    vkDestroyImage(device, renderer->depth_texture, allocator);
    vkFreeMemory(device, renderer->depth_texture_memory, allocator);

    for (u32 i = 0; i < vulkan->swapchain_image_count; i++) {
        vkDestroyFramebuffer(device, vulkan->swapchain_framebuffers[i], allocator);
    }
    for (u32 i = 0; i < vulkan->swapchain_image_count; i++) {
        vkDestroyImageView(device, vulkan->swapchain_image_views[i], allocator);
    }

    vkDestroySwapchainKHR(device, vulkan->swapchain, allocator);
}


void vulkan_create_buffer(Vulkan_Boilerplate_Objects *vulkan, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer *out_buffer, VkDeviceMemory *out_buffer_memory) {
    VkResult vulkan_result;

    VkBufferCreateInfo buffer_create_info = {};
    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.size = size;
    buffer_create_info.usage = usage;
    buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vulkan_result = vkCreateBuffer(vulkan->logical_device, &buffer_create_info, vulkan->allocator, out_buffer);
    assert(vulkan_result == VK_SUCCESS);

    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(vulkan->logical_device, *out_buffer, &memory_requirements);

    u32 memory_type = vulkan_choose_memory_type(vulkan, memory_requirements.memoryTypeBits, properties);

    VkMemoryAllocateInfo allocation_info = {};
    allocation_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation_info.allocationSize = memory_requirements.size;
    allocation_info.memoryTypeIndex = memory_type;

    vulkan_result = vkAllocateMemory(vulkan->logical_device, &allocation_info, vulkan->allocator, out_buffer_memory);
    assert(vulkan_result == VK_SUCCESS);

    vkBindBufferMemory(vulkan->logical_device, *out_buffer, *out_buffer_memory, 0);
}


VkCommandBuffer vulkan_begin_one_time_command_buffer(Vulkan_Boilerplate_Objects *vulkan) {
    VkCommandBuffer command_buffer;
    VkCommandBufferAllocateInfo allocation_info = {};
    allocation_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocation_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocation_info.commandPool = vulkan->command_pool;
    allocation_info.commandBufferCount = 1;
    vkAllocateCommandBuffers(vulkan->logical_device, &allocation_info, &command_buffer);

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(command_buffer, &begin_info);

    return command_buffer;
}


void vulkan_end_and_execute_one_time_command_buffer(Vulkan_Boilerplate_Objects *vulkan, VkCommandBuffer command_buffer) {
    vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;
    vkQueueSubmit(vulkan->graphics_queue, 1, &submit_info, VK_NULL_HANDLE);

    vkQueueWaitIdle(vulkan->graphics_queue);

    vkFreeCommandBuffers(vulkan->logical_device, vulkan->command_pool, 1, &command_buffer);
}


void vulkan_load_texture(Vulkan_Boilerplate_Objects *vulkan, const char *texture_path, VkImage *out_image, VkDeviceMemory *out_image_memory, VkImageView *out_image_view, VkSampler *out_sampler) {
    i32 texture_width, texture_height, texture_channels;
    stbi_uc *pixels = stbi_load(texture_path, &texture_width, &texture_height, &texture_channels, STBI_rgb_alpha);
    assert(pixels);

    VkDeviceSize image_size = texture_width * texture_height * 4;

    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;

    vulkan_create_buffer(vulkan, image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staging_buffer, &staging_buffer_memory);

    void *mapped_memory;
    vkMapMemory(vulkan->logical_device, staging_buffer_memory, 0, image_size, 0, &mapped_memory);
    for (u32 i = 0; i < image_size; i++) {
        *((stbi_uc *)mapped_memory + i) = pixels[i];
    }
    vkUnmapMemory(vulkan->logical_device, staging_buffer_memory);

    stbi_image_free(pixels);
    VkFormat image_format = VK_FORMAT_R8G8B8A8_SRGB;

    vulkan_create_image(
        vulkan,
        texture_width,
        texture_height,
        VK_SAMPLE_COUNT_1_BIT,
        image_format,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        out_image,
        out_image_memory
    );

    vulkan_transition_image_layout(vulkan, *out_image, image_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkCommandBuffer command_buffer = vulkan_begin_one_time_command_buffer(vulkan);
    {
        VkBufferImageCopy region = {};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {(u32)texture_width, (u32)texture_height, 1};
        vkCmdCopyBufferToImage(
            command_buffer,
            staging_buffer,
            *out_image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region
        );
    }
    vulkan_end_and_execute_one_time_command_buffer(vulkan, command_buffer);
    vkDestroyBuffer(vulkan->logical_device, staging_buffer, vulkan->allocator);
    vkFreeMemory(vulkan->logical_device, staging_buffer_memory, vulkan->allocator);

    vulkan_transition_image_layout(vulkan, *out_image, image_format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkImageView *image_view = out_image_view;
    {
        VkImageViewCreateInfo texture_view_create_info = {};
        texture_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        texture_view_create_info.image = *out_image;
        texture_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        texture_view_create_info.format = image_format;
        texture_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        texture_view_create_info.subresourceRange.baseMipLevel = 0;
        texture_view_create_info.subresourceRange.levelCount = 1;
        texture_view_create_info.subresourceRange.baseArrayLayer = 0;
        texture_view_create_info.subresourceRange.layerCount = 1;
        VkResult vulkan_result = vkCreateImageView(vulkan->logical_device, &texture_view_create_info, vulkan->allocator, image_view);
        assert(vulkan_result == VK_SUCCESS);
    }

    VkSampler *texture_sampler = out_sampler;
    {
        VkSamplerCreateInfo sampler_create_info = {};
        sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_create_info.magFilter = VK_FILTER_NEAREST;
        sampler_create_info.minFilter = VK_FILTER_NEAREST;
        sampler_create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        sampler_create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        sampler_create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        sampler_create_info.anisotropyEnable = VK_TRUE;
        sampler_create_info.maxAnisotropy = vulkan->physical_device_properties.limits.maxSamplerAnisotropy;
        sampler_create_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_WHITE;
        sampler_create_info.unnormalizedCoordinates = VK_FALSE;
        sampler_create_info.compareEnable = VK_FALSE;
        sampler_create_info.compareOp = VK_COMPARE_OP_ALWAYS;
        sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler_create_info.mipLodBias = 0.0f;
        sampler_create_info.minLod = 0.0f;
        sampler_create_info.maxLod = 0.0f;
        VkResult vulkan_result = vkCreateSampler(vulkan->logical_device, &sampler_create_info, vulkan->allocator, texture_sampler);
        assert(vulkan_result == VK_SUCCESS);
    }
}


void vulkan_transition_image_layout(Vulkan_Boilerplate_Objects *vulkan, VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout) {
    (void)format;
    VkCommandBuffer command_buffer = vulkan_begin_one_time_command_buffer(vulkan);

    VkImageMemoryBarrier image_memory_barrier = {};
    image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_memory_barrier.oldLayout = old_layout;
    image_memory_barrier.newLayout = new_layout;
    image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.image = image;
    image_memory_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_memory_barrier.subresourceRange.baseMipLevel = 0;
    image_memory_barrier.subresourceRange.levelCount = 1;
    image_memory_barrier.subresourceRange.baseArrayLayer = 0;
    image_memory_barrier.subresourceRange.layerCount = 1;
    image_memory_barrier.srcAccessMask = 0;
    image_memory_barrier.dstAccessMask = 0;
    VkPipelineStageFlags source_stage = 0;
    VkPipelineStageFlags destination_stage = 0;
    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        image_memory_barrier.srcAccessMask = 0;
        image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        assert(false);
    }

    vkCmdPipelineBarrier(
        command_buffer,
        source_stage,
        destination_stage,
        0,
        0, 0,
        0, 0,
        1, &image_memory_barrier
    );

    vulkan_end_and_execute_one_time_command_buffer(vulkan, command_buffer);
}


void vulkan_create_image(Vulkan_Boilerplate_Objects *vulkan, u32 width, u32 height, VkSampleCountFlagBits sample_count, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage_flags, VkMemoryPropertyFlags properties, VkImage *out_image, VkDeviceMemory *out_image_memory) {
    VkImageCreateInfo image_create_info = {};
    image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.imageType = VK_IMAGE_TYPE_2D;
    image_create_info.extent.width = width;
    image_create_info.extent.height = height;
    image_create_info.extent.depth = 1;
    image_create_info.mipLevels = 1;
    image_create_info.arrayLayers = 1;
    image_create_info.format = format;
    image_create_info.tiling = tiling;
    image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_create_info.usage = usage_flags;
    image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_create_info.samples = sample_count;
    image_create_info.flags = 0;

    VkResult vulkan_result = vkCreateImage(vulkan->logical_device, &image_create_info, vulkan->allocator, out_image);
    assert(vulkan_result == VK_SUCCESS);

    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(vulkan->logical_device, *out_image, &memory_requirements);

    VkMemoryAllocateInfo allocation_info = {};
    allocation_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation_info.allocationSize = memory_requirements.size;
    allocation_info.memoryTypeIndex = vulkan_choose_memory_type(vulkan, memory_requirements.memoryTypeBits, properties);

    vulkan_result = vkAllocateMemory(vulkan->logical_device, &allocation_info, vulkan->allocator, out_image_memory);
    assert(vulkan_result == VK_SUCCESS);

    vkBindImageMemory(vulkan->logical_device, *out_image, *out_image_memory, 0);
}


u32 vulkan_choose_memory_type(Vulkan_Boilerplate_Objects *vulkan, u32 type, VkMemoryPropertyFlags properties) {
    for (u32 i = 0; i < vulkan->memory_properties.memoryTypeCount; i++) {
        if ((type & (1 << i)) && (vulkan->memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    assert(false);
    return 0xFFFFFFFF;
}


void vulkan_copy_buffer(Vulkan_Boilerplate_Objects *vulkan, VkBuffer source_buffer, VkBuffer destination_buffer, VkDeviceSize size) {
    VkCommandBufferAllocateInfo allocation_info = {};
    allocation_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocation_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocation_info.commandPool = vulkan->command_pool;
    allocation_info.commandBufferCount = 1;

    VkCommandBuffer transfer_command_buffer;
    vkAllocateCommandBuffers(vulkan->logical_device, &allocation_info, &transfer_command_buffer);

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(transfer_command_buffer, &begin_info);
    VkBufferCopy copy_region = {};
    copy_region.srcOffset = 0;
    copy_region.dstOffset = 0;
    copy_region.size = size;
    vkCmdCopyBuffer(transfer_command_buffer, source_buffer, destination_buffer, 1, &copy_region);
    vkEndCommandBuffer(transfer_command_buffer);

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &transfer_command_buffer;
    vkQueueSubmit(vulkan->graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(vulkan->graphics_queue);  // Block until the transfer is complete
    vkFreeCommandBuffers(vulkan->logical_device, vulkan->command_pool, 1, &transfer_command_buffer);
}


void vulkan_teardown(Vulkan_Boilerplate_Objects *vulkan, Renderer *renderer) {
    VkAllocationCallbacks *allocator = vulkan->allocator;
    VkDevice device = vulkan->logical_device;

    for (i32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device, vulkan->image_available_semaphores[i], allocator);
        vkDestroySemaphore(device, vulkan->render_finished_semaphores[i], allocator);
        vkDestroyFence(device, vulkan->in_flight_fences[i], allocator);
    }

    vulkan_renderer_teardown(vulkan, renderer);
    vulkan_teardown_swapchain(vulkan, renderer);
    vkDestroySurfaceKHR(vulkan->instance, vulkan->surface, allocator);
    vkDestroyDevice(device, allocator);
#ifdef USE_VULKAN_DEBUG_LAYERS
    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vulkan->instance, "vkDestroyDebugUtilsMessengerEXT");
    vkDestroyDebugUtilsMessengerEXT(vulkan->instance, vulkan->debug_messenger, allocator);
#endif
    vkDestroyInstance(vulkan->instance, allocator);
}


VKAPI_ATTR VkBool32 VKAPI_CALL vulkan_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data
) {
    (void)message_severity;
    (void)message_type;
    (void)user_data;
    fprintf(stderr, "validation layer: %s\n", callback_data->pMessage);
    return VK_FALSE;
}
