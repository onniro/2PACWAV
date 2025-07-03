
/*
File: linux_2pacwav2.cpp
Date: Thu 24 Apr 2025 04:22:31 PM EEST

Linux platform-specific code for 2pacwav
*/

#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <stdarg.h>
#include <dirent.h>
#include <time.h>
#include <locale.h>
#include <complex.h>

#include <fontconfig/fontconfig.h>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"

#define GL_GLEXT_PROTOTYPES 1
#include "SDL.h"
//#include "SDL_mixer.h"
#include <GL/gl.h>
#include <GL/glu.h>

#define PACMXR_IMPLEMENTATION 1
#include "2pacmixer.h"

#include "ro_posix.h"
#include "2pacwav2.h"
#include "linux_2pacwav2.h"

#include "2pacwav2.cpp"

PAC_INTERNAL char *platform_find_res_path(Runtime_Vars *rtvars, 
                                        char *out_res_path, 
                                        int bufsize)
{
    char *result = 0;
    char try_buf[PATH_MAX];
    strncpy(try_buf, rtvars->working_directory, PATH_MAX - 1);
    char *end_ptr = try_buf + strlen(try_buf);
    char *last_ptr;
    int max_tries = 5;
    for (int try_index = 0; try_index < max_tries; ++try_index) {
        strcat(end_ptr, "/../res/");
        if (platform_directory_exists(try_buf)) {
            strncpy(out_res_path, try_buf, bufsize);
            result = out_res_path;
            break;
        }
        last_ptr = strstr(end_ptr, "/res/");
        *last_ptr = 0;
    }
    return result;
}

PAC_INTERNAL void platform_get_font_path(Runtime_Vars *rtvars,
                                        char *dest,
                                        int dest_size)
{
    FcInit();
    FcConfig *fc_cfg = FcInitLoadConfigAndFonts();
    FcPattern *pattern = FcNameParse((const FcChar8 *)"Liberation Mono:Regular");
    FcObjectSet *obj_set = FcObjectSetBuild(FC_FILE, (void *)0);
    FcFontSet *font_set = FcFontList(fc_cfg, pattern, obj_set);
    FcChar8 *file;
    FcPattern *font;

    for (int i = 0; i < font_set->nfont; ++i) {
        font = font_set->fonts[i];
        FcPatternGetString(font, FC_FILE, 0, &file);
        if (!strcasestr((char *)file, "italic")) {
            snprintf(dest, dest_size, "%s", (char *)file);
            platform_dbg_log("loading font %s\n", dest);
            break;
        }
    }

    FcFontSetDestroy(font_set);
    FcObjectSetDestroy(obj_set);
    FcPatternDestroy(pattern);
    FcConfigDestroy(fc_cfg);
    FcFini();
}

PAC_INTERNAL int platform_list_files_simple(char *path, 
                                        File_List *out_flist, 
                                        char sort)
{
    int result = 0;
    dirent *dir_entry;
    DIR *dir_struct = opendir(path);
    if (dir_struct) {
        int filename_len;
        char *write_ptr;
        char **loclist = out_flist->filenames_string_loclist;
        while (1) {
            dir_entry = readdir(dir_struct);
            if (!dir_entry) { break; }

            if (!(!strcmp(".", dir_entry->d_name) || 
                !strcmp("..", dir_entry->d_name))) {
                write_ptr = loclist[out_flist->entry_count];
                filename_len = strlen(dir_entry->d_name);
                strncpy(write_ptr, dir_entry->d_name, NAME_MAX - 1);
                write_ptr[filename_len + 1] = 0x0;
                loclist[out_flist->entry_count + 1] = write_ptr + filename_len + 1;
                ++out_flist->entry_count;
            }
        }

        if (sort) 
        { sort_file_list_alpha(out_flist, 0); }
        result = 1;
    } else { 
        platform_dbg_log("directory listing failed. reason: failed to initialize directory struct\n"); 
    }
    return result;
}

PAC_INTERNAL int platform_list_files_mlist(char *path, File_List *out_flist)
{
    int result = 0;
    dirent *dir_entry;
    DIR *dir_struct = opendir(path);
    if (dir_struct) {
        file_list_push_dirname(path, out_flist);

        int filename_len;
        char *write_ptr;
        char **loclist = out_flist->filenames_string_loclist;
        while (1) {
            dir_entry = readdir(dir_struct);
            if (!dir_entry) { break; }

            if (dir_entry->d_type == DT_REG) {
                write_ptr = loclist[out_flist->entry_count];
                filename_len = strlen(dir_entry->d_name);
                strncpy(write_ptr, dir_entry->d_name, NAME_MAX - 1);
                write_ptr[filename_len + 1] = 0x0;
                loclist[out_flist->entry_count + 1] = write_ptr + filename_len + 1;
                ++out_flist->entry_count;
            }
        }
        result = 1;
    } else { 
        platform_dbg_log("directory listing failed. reason: failed to initialize directory struct\n"); 
    }

    return result;
}

PAC_INTERNAL void startup_alloc_buffers(Ro_Heap_Buffer *heapbuf,
                                    General_Buffer_Group *bufgroup) 
{
#define MEM_INIT_ASSERT(main_buffer, buf2init, size)                                \
    buf2init = ro_buffer_alloc_region(main_buffer, size);                           \
    if (!buf2init) {                                                                \
        platform_dbg_log("failed to init buffer %s\n(unallocated=%u)exiting.\n",    \
                #buf2init, ro_buffer_unallocated_bytes(heapbuf));                   \
        PAC_ASSERT(0 && "not enough memory!");                                      \
    }

    memset(heapbuf->memory, 0, PAC_MAIN_STORAGE_SIZE);
    MEM_INIT_ASSERT(heapbuf, bufgroup->userinfo_buffer,                 USERINFO_BUFFER_SIZE);
    MEM_INIT_ASSERT(heapbuf, bufgroup->music_info_buffer,               DEBUG_BUFFER_SIZE);
    MEM_INIT_ASSERT(heapbuf, bufgroup->conf_file_buffer,                CONFBUFFER_SIZE);
    MEM_INIT_ASSERT(heapbuf, bufgroup->music_current_filename,          PATH_MAX);
    MEM_INIT_ASSERT(heapbuf, bufgroup->inbuf_filename,                  PATH_MAX);
    MEM_INIT_ASSERT(heapbuf, bufgroup->inbuf_search,                    PATH_MAX);
#if PAC_SPECTRUM_ENABLED
    MEM_INIT_ASSERT(heapbuf, bufgroup->fft_complex32_buffer,            FFT_COMPLEX32_BUFFER_SIZE*2);
#endif
    MEM_INIT_ASSERT(heapbuf, bufgroup->flist_match_flags,               MATCH_FLAGS_BUFFER_SIZE);
    MEM_INIT_ASSERT(heapbuf, bufgroup->flist_filenames_string_loclist,  FILENAMEBUF_LOCATION_LIST_SIZE);
    MEM_INIT_ASSERT(heapbuf, bufgroup->flist_dirnames_string_loclist,   DIRNAMEBUF_LOCATION_LIST_SIZE);
    MEM_INIT_ASSERT(heapbuf, bufgroup->autocomp_string_loclist,         FILENAMEBUF_LOCATION_LIST_SIZE);
    MEM_INIT_ASSERT(heapbuf, bufgroup->flist_filenames_buf,             FILENAMES_BUFFER_SIZE);
    MEM_INIT_ASSERT(heapbuf, bufgroup->flist_dirnames_buf,              DIRNAMES_BUFFER_SIZE);
    MEM_INIT_ASSERT(heapbuf, bufgroup->autocomp_buffer,                 FILENAMES_BUFFER_SIZE);

    bufgroup->scratch_bytes = ro_buffer_unallocated_bytes(heapbuf);
    if (bufgroup->scratch_bytes) {
        MEM_INIT_ASSERT(heapbuf, 
                bufgroup->scratch_space, 
                bufgroup->scratch_bytes); 
    }
    platform_dbg_log("scratch: %d bytes\n", bufgroup->scratch_bytes);
}

PAC_INTERNAL char platform_file_exists(char *path)
{
    return ro_posix_file_exists(path);
}

PAC_INTERNAL char platform_directory_exists(char *path)
{
    return ro_posix_directory_exists(path);
}

PAC_INTERNAL char platform_path_exists(char *path) 
{
    return ro_posix_path_exists(path);
}

PAC_INTERNAL uint64_t platform_read_file(char *file_path, 
                                        char *dest, 
                                        uint64_t dest_bytes)
{
    return ro_posix_read_file(file_path, dest, dest_bytes);
}

PAC_INTERNAL int platform_write_file(char *file_path, 
                                    void *in_buffer, 
                                    uint64_t buffer_size)
{
    return ro_posix_write_file(file_path, in_buffer, buffer_size);
}


PAC_INTERNAL void platform_log(char *fmt_string, ...)
{
    char buf[4096];
    va_list args;
    va_start(args, fmt_string);
    vsnprintf(buf, 4095, fmt_string, args);
    fprintf(stdout, "%s", buf);
    va_end(args);
}

PAC_INTERNAL void platform_dbg_log(char *fmt_string, ...)
{
#if _2PACWAV_DEBUG || (defined(_2PACWAV_ENABLE_LOG) && _2PACWAV_ENABLE_LOG)
    char buf[4096];
    va_list args;
    va_start(args, fmt_string);
    vsnprintf(buf, 4095, fmt_string, args);
    fprintf(stderr, "%s", buf);
    va_end(args);
#endif
}

PAC_INTERNAL void platform_get_working_directory(char *buf, int buf_size)
{
    ro_posix_get_working_directory(buf, buf_size);
    int len = strlen(buf);
    while (buf[len - 1] == '/') 
    { buf[--len] = 0; }
}

int main(int arg_count, char **args) 
{
    Runtime_Vars rtvars = {};
    rtvars.sflags.visualizer_enabled = 1;
    rtvars.vis_color[0] = 1.0f;
    rtvars.vis_color[1] = 0.1f;
    rtvars.vis_color[2] = 0.1f;
    rtvars.vis_color[3] = 1.0f;
    General_Buffer_Group bufgroup = {};
    if (ro_posix_make_heap_buffer(&rtvars.main_storage, 
            PAC_MAIN_STORAGE_SIZE)) { 
        startup_alloc_buffers(&rtvars.main_storage, &bufgroup); 
    } else { 
        fprintf(stderr, "failed to get memory\n"); 
        return -1; 
    }

    Startup_Args sargs = {};
    sargs.bufgroup_ptr = &bufgroup;
    sargs.paths.buffer = (char *)bufgroup.scratch_space;
    sargs.paths.buffer[0] = 0;
    sargs.paths.ptrs[0] = sargs.paths.buffer;
    sargs.font_size = PAC_LATIN_FONTSIZE;
    rtvars.sargs_ptr = &sargs;
    if (pac_do_command_args(arg_count, args, &sargs, &bufgroup)) 
    { return EXIT_SUCCESS; }

    Sdl_Apidata sdldata = {};
    Music_Data mdata = {};
    strcpy(mdata.music_type_buf, "NONE");

    setlocale(LC_ALL, "en_US.UTF-8");
    srand48(time(0));

    rtvars.sdldata_ptr = &sdldata;
    rtvars.bufgroup_ptr = &bufgroup;
    sdldata.mdata_ptr = &mdata;

    if (!pac_init_sdl(&sdldata)) { return -1; }
    if (!pac_init_tupacmixer(&mdata)) { return -1; }
    rtvars.pacmxr_ctx = pacmxr_get_context();
    mdata.current_filename = (char *)bufgroup.music_current_filename;
    mdata.rtvars_ptr = &rtvars;

    platform_get_working_directory(rtvars.working_directory, PATH_MAX);
    //platform_get_font_path(&rtvars, (char *)bufgroup.scratch_space, PATH_MAX);
    platform_find_res_path(&rtvars, rtvars.resource_directory, PATH_MAX - 1);

    mdata.music_list.filenames_buf = (char *)bufgroup.flist_filenames_buf;
    mdata.music_list.filenames_string_loclist = (char **)bufgroup.flist_filenames_string_loclist;
    mdata.music_list.filenames_string_loclist[0] = (char *)mdata.music_list.filenames_buf;
    mdata.music_list.dirnames_buf = (char *)bufgroup.flist_dirnames_buf;
    mdata.music_list.dirnames_string_loclist = (char **)bufgroup.flist_dirnames_string_loclist;
    mdata.music_list.dirnames_string_loclist[0] = (char *)mdata.music_list.dirnames_buf;
    mdata.music_list.match_flags = (char *)bufgroup.flist_match_flags;

#if PAC_SPECTRUM_ENABLED
    mdata.astream.complex32_buffer_in = (Complex32 *)bufgroup.fft_complex32_buffer;
    mdata.astream.complex32_buffer_out = mdata.astream.complex32_buffer_in + (FFT_COMPLEX32_BUFFER_SIZE/2);
#endif

    rtvars.keep_running = 1;

    Frametime_Vars frametime;
    rtvars.frametime_info_ptr = &frametime;
    rtvars.mdata_ptr = &mdata;

    bufgroup.fontpath_ptr = (char *)bufgroup.scratch_space + (bufgroup.scratch_bytes - (PATH_MAX + 1));
    if (!sargs.no_load_conf) {
        size_t conf_len = startup_load_conf(&rtvars,
                            (char *)bufgroup.conf_file_buffer, 
                            CONFBUFFER_SIZE);
        if (conf_len && (conf_len < CONFBUFFER_SIZE)) {
            parse_and_apply_config(&rtvars,
                    (char *)bufgroup.conf_file_buffer,
                    CONFBUFFER_SIZE);
        } else {
            platform_get_font_path(&rtvars, bufgroup.fontpath_ptr, PATH_MAX);
        }
    } else {
        platform_get_font_path(&rtvars, bufgroup.fontpath_ptr, PATH_MAX);
    }

    if (sargs.volume != -1) {
        mdata.volume = sargs.volume;
        pacmxr_set_volume(mdata.volume);
        mdata.volume = pacmxr_get_volume();
    }

    IMGUI_CHECKVERSION();
    ImGuiContext *imgui_context = ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::GetIO().ConfigFlags |= ImGuiWindowFlags_NoSavedSettings;
    ImGui::GetIO().IniFilename = 0;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForOpenGL(sdldata.window_ptr, sdldata.ogl_context);
    ImGui_ImplOpenGL3_Init("#version 130");

    if (!pac_imgui_load_font(bufgroup.fontpath_ptr,
            sargs.font_size,
            &rtvars)) {
        platform_log("[error]: loading fonts failed\n");
        return -1;
    }

    useconds_t us2sleep;

#if _2PACWAV_DEBUG
    {
        int gl_maj, gl_min;
        SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &gl_maj);
        SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &gl_min);            
        platform_dbg_log("OpenGL vendor: %s\n"
                "OpenGL version: %s\n"
                "renderer: %s\n"
                "GLSL version: %s\n"
                "SDL_GL context version: %d.%d\n",
                glGetString(GL_VENDOR),
                glGetString(GL_VERSION),
                glGetString(GL_RENDERER),
                glGetString(GL_SHADING_LANGUAGE_VERSION),
                gl_maj, gl_min);
    }
#endif

    if (sargs.paths.count) {
        startup_add_paths(&sargs, &rtvars, &mdata);
        memset(bufgroup.scratch_space, 0, (sargs.paths.count + 2)*PATH_MAX);
    }
    
    rtvars.autocomp_list.filenames_buf = (char *)bufgroup.autocomp_buffer;
    rtvars.autocomp_list.filenames_string_loclist = (char **)bufgroup.autocomp_string_loclist;
    rtvars.autocomp_list.filenames_string_loclist[0] = rtvars.autocomp_list.filenames_buf;

    while (rtvars.keep_running) {
        frametime.start = ro_posix_get_timestamp();
        pac_main_loop(&rtvars, &sdldata, &bufgroup, &mdata);
        frametime.end = ro_posix_get_timestamp();
        frametime.delta = frametime.end - frametime.start;

        if ((useconds_t)frametime.delta < MAX_FRAMETIME_MICROSEC) {
            us2sleep = (MAX_FRAMETIME_MICROSEC - (useconds_t)frametime.delta);
            ro_posix_sleep_usec(us2sleep);
        }
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    pacmxr_deinit();
    SDL_GL_DeleteContext(sdldata.ogl_context);
    SDL_DestroyWindow(sdldata.window_ptr);
    SDL_Quit();

    return EXIT_SUCCESS;
}
