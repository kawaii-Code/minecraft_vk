#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "minecraft_vk.hpp"
#include "minecraft_vk_vulkan_boilerplate.cpp"
#include "minecraft_vk_vulkan_renderer.cpp"
#include "minecraft_vk_gameplay.cpp"


int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous_instance, LPSTR command, int show_command) {
    (void)previous_instance;
    (void)command;
    (void)show_command;

    Game game = {};
    Win32_State win32_state = {};
    Vulkan_Boilerplate_Objects vulkan = {};
    Renderer renderer = {};
    game.win32 = &win32_state;
    game.vulkan = &vulkan;
    game.renderer = &renderer;
    game.camera.position = glm::vec3(0.0f, 0.0f, 2.0f);
    win32_state.vulkan = &vulkan;
    {
        ZoneScopedN("Initialization");

        {
            ZoneScopedN("Win32 initialization");

#ifdef USE_VULKAN_DEBUG_LAYERS
            win32_open_console_for_debugging();
#endif

            win32_state.instance = instance;
            win32_state.window_width = 800;
            win32_state.window_height = 600;
            win32_state.window = win32_create_window(&win32_state);

            LARGE_INTEGER performance_frequency;
            QueryPerformanceFrequency(&performance_frequency);
            game.ticks_frequency = performance_frequency.QuadPart;

            SetWindowLongPtr(win32_state.window, GWLP_USERDATA, (LONG_PTR)&game);

            ShowWindow(win32_state.window, SW_SHOW);
            SetWindowPos(win32_state.window, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);  // Move window on top
            SetFocus(win32_state.window);
            SetForegroundWindow(win32_state.window);
            UpdateWindow(win32_state.window);  // Send a WM_PAINT message

            win32_capture_cursor(win32_state.window);
            win32_set_cursor_position(&game, win32_state.window_width / 2, win32_state.window_height / 2);
            game.input.last_mouse_x = win32_state.window_width / 2;
            game.input.last_mouse_y = win32_state.window_height / 2;
            ShowCursor(FALSE);

            i32 game_memory_size = 64 * 1024 * 1024; // 64 Mib
            game.allocator.total_size = game_memory_size;
            game.allocator.memory = VirtualAlloc(0, (SIZE_T)game_memory_size, MEM_COMMIT, PAGE_READWRITE);
        }

        {
            ZoneScopedN("Vulkan initialization");
            vulkan.allocator = 0;
            vulkan_init(&vulkan, &renderer, &win32_state);
            vulkan_renderer_init(&vulkan, &renderer);
            vulkan_create_swapchain(&vulkan, &renderer, &win32_state);
            vulkan_load_texture(
                &vulkan,
                "minecraft_blocks.png",
                &renderer.texture,
                &renderer.texture_memory,
                &renderer.texture_view,
                &renderer.texture_sampler
            );
            vulkan_load_texture(
                &vulkan,
                "crosshair.png",
                &renderer.crosshair_texture,
                &renderer.crosshair_texture_memory,
                &renderer.crosshair_texture_view,
                &renderer.crosshair_texture_sampler
            );
            vulkan_renderer_create_buffers_and_descriptor_sets(&vulkan, &renderer);
        }

        {
            SYSTEMTIME system_time;
            GetSystemTime(&system_time);
            i32 seed = system_time.wDay * 24 * 3600 + system_time.wHour * 3600 + system_time.wMinute * 60 + system_time.wSecond;
            game_init(&game, seed);
        }
    }

    LARGE_INTEGER first_ticks;
    QueryPerformanceCounter(&first_ticks);
    game.total_ticks = first_ticks.QuadPart;
    game.delta_time = 0.016f;  // 60 FPS as a default for the first frame

    u32 frame_in_flight_index = 0;
    while (!win32_state.should_quit) {
        MSG message;
        u32 message_filter_min = 0;
        u32 message_filter_max = 0;
        if (PeekMessage(&message, win32_state.window, message_filter_min, message_filter_max, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessage(&message);
        }

        if (win32_state.window_width == 0 || win32_state.window_height == 0) {
            // Window is minimized, skip rendering entirely
            continue;
        }

        // From glfw:
        // Re-center the cursor only if it has moved since the last call
        // The re-center is required in order to prevent the mouse cursor
        // stopping at the edges of the screen.
        if (game.input.last_mouse_x != win32_state.window_width / 2 ||
            game.input.last_mouse_y != win32_state.window_height / 2)
        {
            win32_set_cursor_position(&game, win32_state.window_width / 2, win32_state.window_height / 2);
        }

        VkResult vulkan_result;  // Store error return codes

        {
            ZoneScopedN("Wait for fences");
            vkWaitForFences(vulkan.logical_device, 1, &vulkan.in_flight_fences[frame_in_flight_index], VK_TRUE, UINT64_MAX);
        }

        u32 swapchain_image_index;
        vulkan_result = vkAcquireNextImageKHR(vulkan.logical_device, vulkan.swapchain, UINT64_MAX, vulkan.image_available_semaphores[frame_in_flight_index], VK_NULL_HANDLE, &swapchain_image_index);
        if (vulkan_result == VK_ERROR_OUT_OF_DATE_KHR) {
            vulkan_re_create_swapchain(&vulkan, &renderer, &win32_state);
            continue;
        }
        vkResetFences(vulkan.logical_device, 1, &vulkan.in_flight_fences[frame_in_flight_index]);
        game_post_render_cleanup(&vulkan, &renderer, frame_in_flight_index);

        game_update_and_render(&game, frame_in_flight_index, swapchain_image_index);

        VkSubmitInfo submit_info = {};
        VkSemaphore wait_semaphores[] = {vulkan.image_available_semaphores[frame_in_flight_index]};
        VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSemaphore signal_semaphores[] = {vulkan.render_finished_semaphores[frame_in_flight_index]};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = wait_semaphores;
        submit_info.pWaitDstStageMask = wait_stages;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &vulkan.command_buffers[frame_in_flight_index];
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = signal_semaphores;
        vulkan_result = vkQueueSubmit(vulkan.graphics_queue, 1, &submit_info, vulkan.in_flight_fences[frame_in_flight_index]);
        assert(vulkan_result == VK_SUCCESS);

        {
            ZoneScopedN("Present");

            VkPresentInfoKHR present_info = {};
            VkSwapchainKHR swapchains[] = {vulkan.swapchain};
            present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            present_info.waitSemaphoreCount = 1;
            present_info.pWaitSemaphores = signal_semaphores;
            present_info.swapchainCount = 1;
            present_info.pSwapchains = swapchains;
            present_info.pImageIndices = &swapchain_image_index;
            present_info.pResults = 0;
            vulkan_result = vkQueuePresentKHR(vulkan.present_queue, &present_info);
            if (vulkan_result == VK_ERROR_OUT_OF_DATE_KHR || vulkan_result == VK_SUBOPTIMAL_KHR || win32_state.resize_happened) {
                win32_state.resize_happened = false;
                vulkan_re_create_swapchain(&vulkan, &renderer, &win32_state);
                continue;
            }
            assert(vulkan_result == VK_SUCCESS);
        }

        frame_in_flight_index = (frame_in_flight_index + 1) % MAX_FRAMES_IN_FLIGHT;

        LARGE_INTEGER performance_counter;
        QueryPerformanceCounter(&performance_counter);
        i64 ticks_passed = performance_counter.QuadPart - game.total_ticks;
        game.delta_time = (f32)((f64)ticks_passed / (f64)game.ticks_frequency);
        game.total_ticks = performance_counter.QuadPart;

        win32_display_fps_in_window_title(win32_state.window, (i32)(1 / game.delta_time));

        FrameMark;
    }

    win32_release_cursor();

    vkDeviceWaitIdle(vulkan.logical_device);  // Wait for any remaining GPU operations to end

    vulkan_teardown(&vulkan, &renderer);
    CloseWindow(win32_state.window);

    return 0;
}


void win32_open_console_for_debugging() {
    AllocConsole();
    AttachConsole(GetCurrentProcessId());
    FILE *stream;
    freopen_s(&stream, "CONIN$", "r", stdin);
    freopen_s(&stream, "CONOUT$", "w+", stdout);
    freopen_s(&stream, "CONOUT$", "w+", stderr);
    HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(consoleHandle, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(consoleHandle, dwMode);
    SetConsoleTitle(PROGRAM_TITLE);
}


HWND win32_create_window(Win32_State *win32_state) {
    char *window_class_name = "WindowClass";
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = win32_window_procedure;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = win32_state->instance;
    wcex.hIcon = LoadIcon(win32_state->instance, MAKEINTRESOURCE(IDI_APPLICATION));
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = window_class_name;
    wcex.hIconSm = LoadIcon(win32_state->instance, MAKEINTRESOURCE(IDI_APPLICATION));;
    ATOM ok = RegisterClassEx(&wcex);
    if (!ok) {
        win32_fatal("Failed to register window class\n");
    }

    HWND window = CreateWindowEx(
        WS_EX_TRANSPARENT,
        window_class_name,
        PROGRAM_TITLE,
        WS_OVERLAPPEDWINDOW,
        0, 0, win32_state->window_width, win32_state->window_height,
        NULL,
        NULL,
        win32_state->instance,
        NULL
    );
    if (window == NULL) {
        win32_fatal("Failed to create a window\n");
    }

    return window;
}


LRESULT CALLBACK win32_window_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    LRESULT result = 0;

    Game *game = (Game *)GetWindowLongPtr(window, GWLP_USERDATA);

    switch (message) {
        case WM_QUIT: {
            game->win32->should_quit = true;
        } break;

        case WM_CLOSE: {
            game->win32->should_quit = true;
        } break;

        case WM_DESTROY: {
            DestroyWindow(window);
            PostQuitMessage(0);
        } break;

        case WM_KEYUP:
        case WM_SYSKEYUP:
        case WM_SYSKEYDOWN:
        case WM_KEYDOWN: {
            Input_Button button;
            button.was_down = ((u32)lparam & (u32)(1 << 30)) != 0;
            button.is_down = ((u32)lparam & (u32)(1 << 31)) == 0;

            if (wparam == 'A') {
                game->input.buttons[BUTTON_LEFT] = button;
            } else if (wparam == 'D') {
                game->input.buttons[BUTTON_RIGHT] = button;
            } else if (wparam == 'W') {
                game->input.buttons[BUTTON_UP] = button;
            } else if (wparam == 'S') {
                game->input.buttons[BUTTON_DOWN] = button;
            } else {
                switch (wparam) {
                    case VK_ESCAPE: {
                        game->win32->should_quit = true;
                    } break;
                    case VK_LEFT: {
                        game->input.buttons[BUTTON_LEFT] = button;
                    } break;
                    case VK_RIGHT: {
                        game->input.buttons[BUTTON_RIGHT] = button;
                    } break;
                    case VK_UP: {
                        game->input.buttons[BUTTON_UP] = button;
                    } break;
                    case VK_DOWN: {
                        game->input.buttons[BUTTON_DOWN] = button;
                    } break;
                    case VK_SHIFT: {
                        game->input.buttons[BUTTON_SPRINT] = button;
                    } break;
                    case VK_F1: {
                        game->input.buttons[BUTTON_F1] = button;
                    } break;
                    case VK_F2: {
                        game->input.buttons[BUTTON_F2] = button;
                    } break;
                    case VK_F3: {
                        game->input.buttons[BUTTON_F3] = button;
                    } break;
                    case VK_F4: {
                        game->input.buttons[BUTTON_F4] = button;
                    } break;

                    default: {
                        return DefWindowProc(window, message, wparam, lparam);
                    } break;
                }
            }
        } break;

        case WM_MOUSEMOVE: {
            i32 x = GET_X_LPARAM(lparam);
            i32 y = GET_Y_LPARAM(lparam);

            game->input.mouse_dx = -1 * (x - game->input.last_mouse_x);
            game->input.mouse_dy = (y - game->input.last_mouse_y);

            game->input.last_mouse_x = x;
            game->input.last_mouse_y = y;
        } break;

        case WM_SIZE: {
            u32 new_width = LOWORD(lparam);
            u32 new_height = HIWORD(lparam);

            game->win32->window_width = (i32)new_width;
            game->win32->window_height = (i32)new_height;

            win32_capture_cursor(window);

            return DefWindowProc(window, message, wparam, lparam);
        } break;

        case WM_MOVE: {
            win32_capture_cursor(window);

            return DefWindowProc(window, message, wparam, lparam);
        } break;

        default: {
            return DefWindowProc(window, message, wparam, lparam);
        } break;
    }

    return result;
}


void win32_capture_cursor(HWND window) {
    RECT clip_rect;
    GetClientRect(window, &clip_rect);
    ClientToScreen(window, (POINT *)&clip_rect.left);
    ClientToScreen(window, (POINT *)&clip_rect.right);
    ClipCursor(&clip_rect);
}


void win32_release_cursor() {
    ClipCursor(NULL);
}


void win32_set_cursor_position(Game *game, i32 x, i32 y) {
    POINT position = {
        (int)x,
        (int)y,
    };

    game->input.last_mouse_x = position.x;
    game->input.last_mouse_y = position.y;

    ClientToScreen(game->win32->window, &position);
    SetCursorPos(position.x, position.y);
}


String win32_read_entire_file(char *filename) {
    String result;

    LPSECURITY_ATTRIBUTES security_attributes = 0;
    HANDLE file_handle = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, security_attributes, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (file_handle == INVALID_HANDLE_VALUE) {
        win32_fatal("Failed opening file!\n");
    }

    BY_HANDLE_FILE_INFORMATION information;
    GetFileInformationByHandle(file_handle, &information);
    result.length = (i32)information.nFileSizeLow;

    result.ptr = (char *)VirtualAlloc(0, (SIZE_T)result.length, MEM_COMMIT, PAGE_READWRITE);
    i32 number_of_bytes_read;
    BOOL ok = ReadFile(file_handle, result.ptr, (DWORD)result.length, (LPDWORD)&number_of_bytes_read, 0);
    assert(ok);
    assert(number_of_bytes_read == result.length);

    CloseHandle(file_handle);

    return result;
}


void win32_display_fps_in_window_title(HWND window, i32 fps) {
    // This is unreasonably slow
    ZoneScoped;
    char window_title_buffer[128];
    sprintf_s(window_title_buffer, "%s: %d FPS\0", PROGRAM_TITLE, fps);
    SetWindowTextA(window, window_title_buffer);
}


void win32_fatal(const char *message) {
    MessageBox(NULL, message, PROGRAM_TITLE, MB_ICONERROR);
    OutputDebugStringA(message);
    ExitProcess(1);
}
