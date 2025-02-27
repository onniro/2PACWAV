
/*
File: linux_2pacwav.cpp
Date: Tue 18 Feb 2025 12:57:19 PM EET
*/

#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <stdarg.h>
#include <dirent.h>

#include "SDL.h"
#include "SDL_mixer.h"
#include <GL/gl.h>
#include <GL/glu.h>

//#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
//#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
//#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
//#define NK_INCLUDE_DEFAULT_FONT
//#define NK_IMPLEMENTATION
//#define NK_SDL_GL2_IMPLEMENTATION
#include "nuklear.h"
#include "nuklear_sdl_gl2.h"

#include "ro_posix.h"
#include "linux_2pacwav.h"
#include "2pacwav.h"

void load_font(struct nk_context *nuklear_ctx, char *working_dir) 
{
    char font_path[PATH_MAX];
#if _2PACWAV_DEBUG
    snprintf(font_path, PATH_MAX - 1, "%s/../../res/%s", working_dir, PAC_FONT_STRING);
#else
    //TODO
#endif
    struct nk_font_config ft_config = {0};
    ft_config.oversample_h = 2;
    ft_config.oversample_v = 2;
#if 0
    ft_config.range = nk_font_cyrillic_glyph_ranges();
#else
    ft_config.range = pac_font_glyph_ranges();
#endif

    struct nk_font_atlas *ft_atlas;
    nk_sdl_font_stash_begin(&ft_atlas);
    struct nk_font *font = nk_font_atlas_add_from_file(ft_atlas, 
                            font_path,
                            PAC_NUKLEAR_FONTSIZE, 
                            &ft_config);
    nk_sdl_font_stash_end();
    nk_style_set_font(nuklear_ctx, &font->handle);
}

int platform_get_directory_listing(char *path, 
                                char *dest_buffer, 
                                int dest_buf_size, 
                                int *out_bytes_written)
{
    int entry_count = 0;
    struct dirent *dir_entry;
    DIR *dir_handle = opendir(path);
    if(dir_handle)
    {
        uint64_t bytes_written = 0;
        int len;
        char *write_ptr = dest_buffer;
        while(1)
        {
            dir_entry = readdir(dir_handle);
            if(!dir_entry)
            { break; }

            ++entry_count;
            len = strlen(dir_entry->d_name);
            //snprintf(write_ptr, dest_buf_size - bytes_written, "%s\n", dir_entry->d_name);
            strncpy(write_ptr, dir_entry->d_name, dest_buf_size - (bytes_written + 1));
            write_ptr[len] = '\n';
            write_ptr[len + 1] = 0;
            write_ptr = (char *)(1 + (uintptr_t)write_ptr + len);
            bytes_written += len + 1;
        }
        closedir(dir_handle);
        if(out_bytes_written)
        { *out_bytes_written = bytes_written; }
    }
    printf("%s\n", dest_buffer);
    return entry_count;
}

void startup_alloc_buffers(ro_heap_buffer *heapbuf, general_buffer_group *bufgroup) 
{
#define MEM_INIT_ASSERT(main_buffer, buf2init, size)\
    buf2init = ro_buffer_alloc_region(main_buffer, size);\
    if(!buf2init)\
    { fprintf(stderr, "failed to init buffer %s\nexiting.\n", #buf2init); _exit(1); }\
    PAC_NOP_MACRO()

    memset(heapbuf->memory, 0, PAC_MAIN_STORAGE_SIZE);
    MEM_INIT_ASSERT(heapbuf, bufgroup->debug_buffer,                DEBUG_BUFFER_SIZE);
    MEM_INIT_ASSERT(heapbuf, bufgroup->music_current_filename,      PATH_MAX);
    MEM_INIT_ASSERT(heapbuf, bufgroup->inbuf_filename,              PATH_MAX);
    MEM_INIT_ASSERT(heapbuf, bufgroup->working_directory,           PATH_MAX);
    MEM_INIT_ASSERT(heapbuf, bufgroup->path_content_buffer,         PATH_CONTENT_BUFFER_SIZE);
}

char platform_file_exists(char *path)
{
    return ro_posix_file_exists(path);
}

char platform_directory_exists(char *path)
{
    return ro_posix_directory_exists(path);
}

void platform_log(char *fmt_string, ...)
{
#if _2PACWAV_DEBUG
    char buf[4096];
    va_list args;
    va_start(args, fmt_string);
    vsnprintf(buf, 4095, fmt_string, args);
    fprintf(stderr, "%s", buf);
    va_end(args);
#endif
}

int main(int argc, char **argv) 
{
    //char something[12] = "■"; //0xE2 0x96 0x00
    sdl_apidata sdldata = {0};
    runtime_vars rtvars = {0};
    general_buffer_group bufgroup = {0};
    music_data mdata = {0};
    strcpy(mdata.music_type_buf, "NONE");

    rtvars.sdldata_ptr = &sdldata;
    rtvars.bufgroup_ptr = &bufgroup;
    sdldata.mdata_ptr = &mdata;
    if(!pac_init_sdl(&sdldata)) 
    {
        fprintf(stderr, "failed to init SDL\n");
        return -1; 
    }
    if(!pac_init_sdlmixer(&mdata)) 
    {
        fprintf(stderr, "failed to init SDL mixer\n");
        return -1; 
    }
        
    if(ro_posix_make_heap_buffer(&rtvars.main_storage, PAC_MAIN_STORAGE_SIZE)) 
    { startup_alloc_buffers(&rtvars.main_storage, &bufgroup); }
    else
    { fprintf(stderr, "failed to get memory\n"); return -1; }
    mdata.current_filename = bufgroup.music_current_filename;
    rtvars.working_directory = bufgroup.working_directory;

    ro_posix_get_working_directory(rtvars.working_directory, PATH_MAX);

    rtvars.nuklear_ctx = nk_sdl_init(sdldata.window_ptr);
    load_font(rtvars.nuklear_ctx, rtvars.working_directory);

    nuklearapi_set_style(rtvars.nuklear_ctx);
    rtvars.nuklear_ctx->style.button.rounding = 0;
    rtvars.nuklear_ctx->clip.paste = pac_nuklearapi_paste_callback;
    //mdata.music_list.ctx = rtvars.nuklear_ctx;

    rtvars.keep_running = 1;

    frametime_vars frametime;
    rtvars.frametime_info_ptr = &frametime;
    useconds_t us2sleep;
    while(rtvars.keep_running) 
    {
        frametime.start = ro_posix_get_timestamp();
        pac_main_loop(&rtvars, &sdldata, &bufgroup, &mdata);
        frametime.end = ro_posix_get_timestamp();
        frametime.delta = frametime.end - frametime.start;
        if((useconds_t)frametime.delta < MAX_FRAMETIME_MICROSEC) 
        {
            us2sleep = (MAX_FRAMETIME_MICROSEC - (useconds_t)frametime.delta);
            ro_posix_sleep_microsec(us2sleep);
        }
    }

    if(mdata.sdlmixer_music) 
    { Mix_FreeMusic(mdata.sdlmixer_music); }
    Mix_CloseAudio();

    nk_sdl_shutdown();
    SDL_GL_DeleteContext(sdldata.ogl_context);
    SDL_DestroyWindow(sdldata.window_ptr);
    SDL_Quit();

    return 0;
}
