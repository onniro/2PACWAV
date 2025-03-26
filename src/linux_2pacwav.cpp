
/*
File: linux_2pacwav.cpp
Date: Tue 18 Feb 2025 12:57:19 PM EET
*/

#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <stdarg.h>
#include <dirent.h>
#include <time.h>
#include <locale.h>

#include "SDL.h"
#include "SDL_mixer.h"
#include <GL/gl.h>
#include <GL/glu.h>

#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_FONT_BAKING
#include "nuklear.h"
#include "nuklear_sdl_gl2.h"

#include "ro_posix.h"
#include "2pacwav.h"
#include "linux_2pacwav.h"

PAC_INTERNAL char *find_res_path(Runtime_Vars *rtvars, char *out_res_path, int bufsize)
{
    char *result = 0;
    char try_buf[PATH_MAX];
    strncpy(try_buf, rtvars->working_directory, PATH_MAX - 1);
    char *end_ptr = try_buf + strlen(try_buf);
    char *last_ptr;
    int max_tries = 5;
    for(int try_index = 0; try_index < max_tries; ++try_index)
    {
        strcat(end_ptr, "/../res/");
        if(platform_directory_exists(try_buf))
        {
            strncpy(out_res_path, try_buf, bufsize);
            result = out_res_path;
            break;
        }
        last_ptr = strstr(end_ptr, "/res/");
        *last_ptr = 0;
    }
    return result;
}

PAC_INTERNAL void load_font(Runtime_Vars *rtvars)
{
    struct nk_context *nuklear_ctx = rtvars->nuklear_ctx;
    char *working_dir = rtvars->working_directory;
    char *res_dir = rtvars->resource_directory;
    char small_font_path[PATH_MAX], big_font_path[PATH_MAX];

    if(find_res_path(rtvars, res_dir, PATH_MAX - 1))
    {
        snprintf(small_font_path, PATH_MAX - 1, "%s/%s", res_dir, PAC_FONT_STRING);
        snprintf(big_font_path, PATH_MAX - 1, "%s/%s", res_dir, PAC_BIG_FONT_STRING);
    }
    else
    { platform_log("cannot load resources. reason: path missing.\nexiting.\n"); exit(1); }

    struct nk_font_config ft_config = {};
    ft_config.oversample_h = 2;
    ft_config.oversample_v = 2;
#if 0
    ft_config.range = nk_font_cyrillic_glyph_ranges();
#else
    ft_config.range = pac_font_glyph_ranges();
#endif
    
    struct nk_font_atlas *ft_atlas;
    nk_sdl_font_stash_begin(&ft_atlas);
    rtvars->small_font = nk_font_atlas_add_from_file(ft_atlas, 
                            small_font_path,
                            PAC_NUKLEAR_FONTSIZE, 
                            &ft_config);
    rtvars->big_font = nk_font_atlas_add_from_file(ft_atlas, 
                            big_font_path,
                            PAC_NUKLEAR_BIG_FONTSIZE, 
                            &ft_config);
    nk_sdl_font_stash_end();
    nk_style_set_font(nuklear_ctx, &rtvars->small_font->handle);
}

int platform_get_directory_listing(char *path, File_List *out_flist)
{
    int result = 0;
    dirent *dir_entry;
    DIR *dir_struct = opendir(path);

    if(dir_struct) 
    {
        file_list_push_dirname(path, out_flist);

        int filename_len;
        char *write_ptr;
        while(1) 
        {
            dir_entry = readdir(dir_struct);
            if(!dir_entry)
            { break; }

            if(dir_entry->d_type == DT_REG)
            {
                write_ptr = out_flist->filenames_string_loclist[out_flist->entry_count];
                filename_len = strlen(dir_entry->d_name);
                strncpy(write_ptr, dir_entry->d_name, NAME_MAX - 1);
                write_ptr[filename_len + 1] = 0x0;
                out_flist->filenames_string_loclist[out_flist->entry_count + 1] = write_ptr + filename_len + 1;
                ++out_flist->entry_count;
            }
        }

        result = 1;
    }
    else
    { platform_log("directory listing failed. reason: failed to initialize directory struct\n"); }

    return result;
}

PAC_INTERNAL void startup_alloc_buffers(Ro_Heap_Buffer *heapbuf, 
                                    General_Buffer_Group *bufgroup) 
{
#define MEM_INIT_ASSERT(main_buffer, buf2init, size)\
    buf2init = ro_buffer_alloc_region(main_buffer, size);\
    if(!buf2init)\
    {\
        platform_dbg_log("failed to init buffer %s\n(unallocated=%u)exiting.\n",\
                #buf2init, ro_buffer_unallocated_bytes(heapbuf));\
        _exit(1);\
    } PAC_NOP_MACRO()

    memset(heapbuf->memory, 0, PAC_MAIN_STORAGE_SIZE);
    MEM_INIT_ASSERT(heapbuf, bufgroup->userinfo_buffer,                 USERINFO_BUFFER_SIZE);
    MEM_INIT_ASSERT(heapbuf, bufgroup->music_info_buffer,               DEBUG_BUFFER_SIZE);
    MEM_INIT_ASSERT(heapbuf, bufgroup->music_current_filename,          PATH_MAX);
    MEM_INIT_ASSERT(heapbuf, bufgroup->inbuf_filename,                  PATH_MAX);
    MEM_INIT_ASSERT(heapbuf, bufgroup->inbuf_search,                    PATH_MAX);
    MEM_INIT_ASSERT(heapbuf, bufgroup->working_directory,               PATH_MAX);
    MEM_INIT_ASSERT(heapbuf, bufgroup->resource_directory,              PATH_MAX);
    MEM_INIT_ASSERT(heapbuf, bufgroup->flist_match_flags,               MATCH_FLAGS_BUFFER_SIZE);
    MEM_INIT_ASSERT(heapbuf, bufgroup->flist_filenames_string_loclist,  FILENAMEBUF_LOCATION_LIST_SIZE);
    MEM_INIT_ASSERT(heapbuf, bufgroup->flist_dirnames_string_loclist,   DIRNAMEBUF_LOCATION_LIST_SIZE);
    MEM_INIT_ASSERT(heapbuf, bufgroup->flist_filenames_buf,             FILENAMES_BUFFER_SIZE);
    MEM_INIT_ASSERT(heapbuf, bufgroup->flist_dirnames_buf,              DIRNAMES_BUFFER_SIZE);

#if 1
    //reserving "scratch space"
    uint64_t space_left = ro_buffer_unallocated_bytes(heapbuf);
    platform_dbg_log("scrath:%d bytes\n", space_left);
    MEM_INIT_ASSERT(heapbuf, bufgroup->scratch_space, space_left);
#else
    platform_dbg_log("unallocated bytes:%.2f/%.2f\n", 
            (float)(ro_buffer_unallocated_bytes(heapbuf)), 
            (float)(heapbuf->total_bytes));
#endif
}

char platform_file_exists(char *path)
{
    return ro_posix_file_exists(path);
}

char platform_directory_exists(char *path)
{
    return ro_posix_directory_exists(path);
}

int platform_read_file(char *file_path, char *dest, uint64_t *dest_bytes)
{
    return ro_posix_read_file(file_path, dest, dest_bytes);
}

int platform_write_file(char *file_path, void *in_buffer, uint64_t buffer_size)
{
    return ro_posix_write_file(file_path, in_buffer, buffer_size);
}

void platform_log(char *fmt_string, ...)
{
    char buf[4096];
    va_list args;
    va_start(args, fmt_string);
    vsnprintf(buf, 4095, fmt_string, args);
    fprintf(stdout, "%s", buf);
    va_end(args);
}

void platform_dbg_log(char *fmt_string, ...)
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

void platform_get_working_directory(char *buf, int buf_size)
{
    ro_posix_get_working_directory(buf, buf_size);
    int len = strlen(buf);
    if(buf[len - 1] == '/')
    { buf[len - 1] = 0; }
}

int main(int arg_count, char **args) 
{
    pac_do_command_args(arg_count, args);

    Sdl_Apidata sdldata = {};
    Runtime_Vars rtvars = {};
    General_Buffer_Group bufgroup = {};
    Music_Data mdata = {};
    strcpy(mdata.music_type_buf, "NONE");

    setlocale(LC_ALL, "en_US.UTF-8");

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
    mdata.current_filename = (char *)bufgroup.music_current_filename;
    mdata.rtvars_ptr = &rtvars;
    rtvars.working_directory = (char *)bufgroup.working_directory;
    rtvars.resource_directory = (char *)bufgroup.resource_directory;

    platform_get_working_directory(rtvars.working_directory, PATH_MAX);

    rtvars.nuklear_ctx = nk_sdl_init(sdldata.window_ptr);
    load_font(&rtvars);

    nuklearapi_set_style(rtvars.nuklear_ctx);
    rtvars.nuklear_ctx->style.button.rounding = 0;
    rtvars.nuklear_ctx->clip.paste = pac_nuklearapi_paste_callback;

    mdata.music_list.filenames_buf = (char *)bufgroup.flist_filenames_buf;
    mdata.music_list.filenames_string_loclist = (char **)bufgroup.flist_filenames_string_loclist;
    mdata.music_list.filenames_string_loclist[0] = (char *)mdata.music_list.filenames_buf;
    mdata.music_list.dirnames_buf = (char *)bufgroup.flist_dirnames_buf;
    mdata.music_list.dirnames_string_loclist = (char **)bufgroup.flist_dirnames_string_loclist;
    mdata.music_list.dirnames_string_loclist[0] = (char *)mdata.music_list.dirnames_buf;
    mdata.music_list.match_flags = (char *)bufgroup.flist_match_flags;

    rtvars.keep_running = 1;

    srand48(time(0));

    Frametime_Vars frametime;
    rtvars.frametime_info_ptr = &frametime;
    useconds_t us2sleep;

    //if(taglib_get_albumcover(&mdata.cover, "/home/onni/source/2pacwav/res/beware.mp3"))
    //{
    //    pac_init_bitmap(&mdata.cover, &rtvars);
    //}

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
}
