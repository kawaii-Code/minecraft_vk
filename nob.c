#define NOB_IMPLEMENTATION
#include "third_party/nob.h"

char *CPP_COMPILER = "cl";

// check out /favor:AMD64 /favor:INTEL64
char *GENERAL_COMPILER_OPTIONS[] = {
    "/D_HAS_EXCEPTIONS=0",
    "/nologo",
    // "/analyze:only",
    // "/analyze:plugin","EspxEngine.dll",
    "/GR-",
    "/diagnostics:column",
    "/std:c++14",
};

char *OPTIMIZATION_OPTIONS[] = {
    "/O2",
};

// /wd4365 can be periodically disabled to check signed/unsigned mismatch
char *WARNING_OPTIONS[] = {
    "/Wall",
    "/wd4365",
    "/wd4711",
    "/wd4710",
    "/wd4577",
    "/wd4820",
    "/wd4464",
    "/wd5045",
    "/wd4302",
    "/wd4191",
    "/wd5026",
    "/wd5027",
    "/wd4626",
    "/wd4625",
    "/wd4426",
};

char *INCLUDE_DIRS[] = {
    "/I", "../third_party",
    "/I", "../third_party/tracy",
};

char *LIBRARIES[] = {
    "user32.lib", "gdi32.lib", "../third_party/vulkan-1.lib"
};

char *GLSL_COMPILER = "glslc.exe";

char *SHADER_DIR = "assets/shaders";

char *OUTPUT_DIR = "run_tree";


bool compile_shader(char *shader_path);
void print_usage(char *program_name, Nob_Log_Level log_level);
void set_up_windows_environment_variables_for_msvc();


int main(int argc, char **argv) {
#ifdef _WIN32
    set_up_windows_environment_variables_for_msvc();
#endif

    NOB_GO_REBUILD_URSELF(argc, argv);

    bool release_mode = false;
    bool enable_tracy = false;
    bool enable_debug = true;
    bool enable_vulkan_debug_layers = true;
    bool run_game = false;

    char *program_name = nob_shift_args(&argc, &argv);
    if (argc > 0) {
        bool is_subcommand = argv[0][0] != '-';
        if (is_subcommand) {
            char *subcommand = nob_shift_args(&argc, &argv);
            if (strcmp(subcommand, "build") == 0) {
                // Do nothing, build is the default
            } else if (strcmp(subcommand, "build-run") == 0) {
                run_game = true;
            } else if (strcmp(subcommand, "help") == 0) {
                print_usage(program_name, NOB_INFO);
                return 0;
            } else {
                nob_log(NOB_ERROR, "Unrecognized subcommand '%s'", subcommand);
                return 1;
            }
        }

        while (argc > 0) {
            const char *option = nob_shift_args(&argc, &argv);
            if (strcmp("-profile", option) == 0) {
                enable_tracy = true;
            } else if (strcmp("-no-debug-layers", option) == 0) {
                enable_vulkan_debug_layers = false;
            } else if (strcmp("-no-debug", option) == 0) {
                enable_debug = false;
            } else if (strcmp("-release", option) == 0) {
                release_mode = true;
            } else if (strcmp("-h", option) == 0 || strcmp("-help", option) == 0 || strcmp("--help", option) == 0) {
                print_usage(program_name, NOB_INFO);
                return 0;
            } else {
                nob_log(NOB_ERROR, "Unrecognized option '%s'", option);
                return 1;
            }
        }
    }

    bool ok;

    ok = nob_mkdir_if_not_exists(OUTPUT_DIR);
    if (!ok) {
        return 1;
    }

    ok = compile_shader("texture");
    if (!ok) {
        return 1;
    }
    ok = compile_shader("block");
    if (!ok) {
        return 1;
    }
    ok = compile_shader("flat_color");
    if (!ok) {
        return 1;
    }
    ok = compile_shader("quad");
    if (!ok) {
        return 1;
    }

    Nob_File_Paths texture_files = {};
    nob_read_entire_dir("assets/textures", &texture_files);
    for (int i = 0; i < texture_files.count; i++) {
        char *full_filepath = nob_temp_sprintf("assets/textures/%s", texture_files.items[i]);
        if (nob_get_file_type(full_filepath) == NOB_FILE_REGULAR) {
            nob_copy_file(full_filepath, nob_temp_sprintf("%s/%s", OUTPUT_DIR, texture_files.items[i]));
        }
    }
    nob_temp_reset();

    ok = nob_set_current_dir(OUTPUT_DIR);
    if (!ok) {
        return 1;
    }

    Nob_Cmd compile_cmd = {};
    {
        nob_cmd_append(&compile_cmd, CPP_COMPILER);

        if (release_mode) {
            nob_da_append_many(&compile_cmd, OPTIMIZATION_OPTIONS, NOB_ARRAY_LEN(OPTIMIZATION_OPTIONS));
        }

        if (enable_tracy) {
            nob_cmd_append(&compile_cmd, "/DTRACY_ENABLE");
        }

        if (enable_vulkan_debug_layers) {
            nob_cmd_append(&compile_cmd, "/DUSE_VULKAN_DEBUG_LAYERS");
        }

        if (enable_debug) {
            nob_cmd_append(&compile_cmd, "/Zi");
        }

        nob_cmd_append(&compile_cmd, nob_temp_sprintf("/Fe:..\\%s\\minecraft.exe", OUTPUT_DIR));
        nob_da_append_many(&compile_cmd, GENERAL_COMPILER_OPTIONS, NOB_ARRAY_LEN(GENERAL_COMPILER_OPTIONS));
        nob_da_append_many(&compile_cmd, INCLUDE_DIRS, NOB_ARRAY_LEN(INCLUDE_DIRS));
        nob_da_append_many(&compile_cmd, WARNING_OPTIONS, NOB_ARRAY_LEN(WARNING_OPTIONS));

        nob_cmd_append(&compile_cmd, "../entry_point_windows.cpp");
        if (enable_tracy) {
            nob_cmd_append(&compile_cmd, "../third_party/tracy/TracyClient.cpp");
        }

        nob_da_append_many(&compile_cmd, LIBRARIES, NOB_ARRAY_LEN(LIBRARIES));
    }
    ok = nob_cmd_run_sync(compile_cmd);
    if (!ok) {
        return 1;
    }

    if (run_game) {
        Nob_Cmd run_cmd = {};

        nob_cmd_append(&run_cmd, "minecraft.exe");
        ok = nob_cmd_run_sync(run_cmd);
        if (!ok) {
            return 1;
        }
    }

    return 0;
}


// TODO: It's possible to emit debug info for shaders, look into that
bool compile_shader(char *shader_filename_without_extension) {
    bool ok;
    Nob_Cmd cmd = {};

    char *vertex_shader_path = nob_temp_sprintf("%s/%s.vert", SHADER_DIR, shader_filename_without_extension);
    char *vertex_shader_output_path = nob_temp_sprintf("%s/%s_vertex.spv", OUTPUT_DIR, shader_filename_without_extension);
    nob_cmd_append(&cmd, GLSL_COMPILER, "-fshader-stage=vert", vertex_shader_path, "-o", vertex_shader_output_path);
    ok = nob_cmd_run_sync_and_reset(&cmd);
    if (!ok) {
        return false;
    }

    char *fragment_shader_path = nob_temp_sprintf("%s/%s.frag", SHADER_DIR, shader_filename_without_extension);
    char *fragment_shader_output_path = nob_temp_sprintf("%s/%s_fragment.spv", OUTPUT_DIR, shader_filename_without_extension);
    nob_cmd_append(&cmd, GLSL_COMPILER, "-fshader-stage=frag", fragment_shader_path, "-o", fragment_shader_output_path);
    ok = nob_cmd_run_sync_and_reset(&cmd);
    if (!ok) {
        return false;
    }

    return true;
}


void print_usage(char *program_name, Nob_Log_Level log_level) {
    nob_log(log_level, "Usage: %s [subcommand] [options...]", program_name);
    nob_log(log_level, "Subcommands:");
    nob_log(log_level, "    build [default]     Build the game");
    nob_log(log_level, "    build-run           Build and run the game");
    nob_log(log_level, "    help                Print this message");
    nob_log(log_level, "Options:");
    nob_log(log_level, "    -release          Enable optimizations");
    nob_log(log_level, "    -profile          Enable the tracy profiler");
    nob_log(log_level, "    -no-debug         Do not generate debug info");
    nob_log(log_level, "    -no-debug-layers  Disable vulkan debug layers");
}


#ifdef _WIN32

// For my machine only!

#define PATH_DIR_0    "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Tools\\MSVC\\14.40.33807\\bin\\HostX64\\x64"

#define LIBRARY_DIR_0 "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Tools\\MSVC\\14.40.33807\\lib\\x64"
#define LIBRARY_DIR_1 "C:\\Program Files (x86)\\Windows Kits\\10\\lib\\10.0.22621.0\\ucrt\\x64"
#define LIBRARY_DIR_2 "C:\\Program Files (x86)\\Windows Kits\\10\\lib\\10.0.22621.0\\um\\x64"

#define INCLUDE_DIR_0 "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Tools\\MSVC\\14.40.33807\\include"
#define INCLUDE_DIR_1 "C:\\Program Files (x86)\\Windows Kits\\10\\Include\\10.0.22621.0\\ucrt"
#define INCLUDE_DIR_2 "C:\\Program Files (x86)\\Windows Kits\\10\\Include\\10.0.22621.0\\um"
#define INCLUDE_DIR_3 "C:\\Program Files (x86)\\Windows Kits\\10\\Include\\10.0.22621.0\\shared"

void set_up_windows_environment_variables_for_msvc() {
    // Incredible crutch to get cl.exe, libraries and include files into environment variables.
    // Basically, vcvarsall.bat inlined
    char path_env[4096];
    GetEnvironmentVariable("PATH", path_env, NOB_ARRAY_LEN(path_env));
    char *new_path_env = nob_temp_sprintf("%s" ";" PATH_DIR_0, path_env);
    SetEnvironmentVariable("PATH", new_path_env);
    nob_temp_reset();

    char lib_env[4096];
    GetEnvironmentVariable("LIB", lib_env, NOB_ARRAY_LEN(lib_env));
    char *new_lib_env = nob_temp_sprintf("%s" ";" LIBRARY_DIR_0 ";" LIBRARY_DIR_1 ";" LIBRARY_DIR_2, lib_env);
    SetEnvironmentVariable("LIB", new_lib_env);
    nob_temp_reset();

    char include_env[4096];
    GetEnvironmentVariable("INCLUDE", include_env, NOB_ARRAY_LEN(include_env));
    char *new_include_env = nob_temp_sprintf("%s" ";" INCLUDE_DIR_0 ";" INCLUDE_DIR_1 ";" INCLUDE_DIR_2 ";" INCLUDE_DIR_3, include_env);
    SetEnvironmentVariable("INCLUDE", new_lib_env);
    nob_temp_reset();
}

#endif // _WIN32
