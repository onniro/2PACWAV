
/*
File: 2pacwav.h
Date: Tue 18 Feb 2025 12:56:32 PM EET
*/

#ifndef _2PACWAV_DOT_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "ro_heapbuf.h"

#define WINDOW_WIDTH    1024
#define WINDOW_HEIGHT   768
#define MAX_FRAMETIME_MICROSEC ((useconds_t)16667)
#define PAC_NUKLEAR_FONTSIZE (16.0f)
#define PAC_SEEK_VALUE_MAX (100)

#define PAC_FONT_STRING "NotoMono-Regular.ttf"

#define PAC_DIRENT_DIRECTORY (4)

static const uint8_t _stop_btn_glyph[4] = {0xE2, 0x96, 0xA0, 0x00};

#define PAC_NOP_MACRO(...)

#ifndef PAC_SAMPLE_RATE
    #define PAC_SAMPLE_RATE MIX_DEFAULT_FREQUENCY
#endif
#ifndef PAC_PCM_BITS
    #define PAC_PCM_BITS MIX_DEFAULT_FORMAT
#endif
#ifndef PAC_SDLMIXER_CHUNKSIZE
    #define PAC_SDLMIXER_CHUNKSIZE (2048)
#endif
#ifndef PAC_CHAN_COUNT
    #define PAC_CHAN_COUNT 2
#endif

#define PAC_MAX_FILES (8192)

#define PAC_MAIN_STORAGE_SIZE               (10*(1024*1024)) //10 MB
#define DEBUG_BUFFER_SIZE                   (PATH_MAX/4)
#define FILENAMES_BUFFER_SIZE               (PAC_MAX_FILES*NAME_MAX)
#define DIRNAMES_BUFFER_SIZE                (128*PATH_MAX)
#define FILENAMEBUF_LOCATION_LIST_SIZE      (sizeof(char *)*PAC_MAX_FILES)
#define DIRNAMEBUF_LOCATION_LIST_SIZE       (sizeof(char *)*64)

//members prefixed with inbuf_ are buffers that get passed to nk_edit_string*
typedef struct general_buffer_group
{
    void *inbuf_filename;
    void *music_current_filename;
    void *working_directory;
    void *debug_buffer;
    void *flist_filenames_buf; //TODO: rename
    void *flist_dirnames_buf;
    void *flist_filenames_string_loclist;
    void *flist_dirnames_string_loclist;
    void *flist_path_ranges;
} general_buffer_group;

//dumbass
typedef struct file_list
{
    uint32_t entry_count; //(files)
    uint32_t dirs_added;
    char *dirnames_buf;                 //buffer containing top-level directory names, delimited by null
    char *filenames_buf;                //buffer containing file names, delimited by null
    char **filenames_string_loclist;    //array of pointers which specify the beginnings of strings in the filename array
    char **dirnames_string_loclist;     //array of pointers which specify the beginnings of strings in the dirname array

    //HACK: array of indices in which the array index maps to the index of the filename
    //whereas the value at the index maps to the index of the top level directory name
    //which contains the file 
    uint32_t *path_ranges;
} file_list;

typedef struct frametime_vars
{
    uint64_t start;
    uint64_t end;
    uint64_t delta;
} frametime_vars;

typedef struct widget_bounds_info
{
    float width;
    float height;
    float pad;
    float y_alignment;
    float y_offset;
    float x_offset;
    struct nk_rect content_bounds;
} widget_bounds_info;

typedef struct music_data
{
    char paused; //Mix_PausedMusic() doesn't work seemingly
    uint16_t pcm_bits;
    int sample_rate;
    int channels;
    int volume;
    int chunk_size;
    float seek_value;
    double current_position;
    double current_duration;
    char *current_filename;
    file_list music_list;
    Mix_Music *sdlmixer_music; //IMPORTANT: ALWAYS SET TO NULL WHEN MUSIC IS UNLOADED
    struct nk_list_view music_list_view;
    char music_type_buf[16];
} music_data;

typedef struct sdl_apidata 
{
    int win_width;
    int win_height;
    SDL_Window *window_ptr;
    SDL_GLContext ogl_context;
    music_data *mdata_ptr;
} sdl_apidata;

typedef struct runtime_vars 
{
    char keep_running;
    char *working_directory;
    frametime_vars *frametime_info_ptr;
    general_buffer_group *bufgroup_ptr;
    ro_heap_buffer main_storage;
    sdl_apidata *sdldata_ptr;
    const uint8_t *kbd_state;
    struct nk_context *nuklear_ctx;
} runtime_vars;

//(forward declarations)

nk_rune *pac_font_glyph_ranges(void);
void pac_nuklearapi_paste_callback(nk_handle handle, struct nk_text_edit *txtedit);
void sdlapi_process_events(runtime_vars *rtvars, sdl_apidata *sdldata);
void sdlapi_correct_gl_viewport_and_clear(sdl_apidata *sdldata);
void pac_begin_frame(runtime_vars *rtvars, sdl_apidata *sdldata);
void pac_end_frame(runtime_vars *rtvars, sdl_apidata *sdldata);
void update_music_info(music_data *mdata);
void pac_main_loop(runtime_vars *rtvars, sdl_apidata *sdldata, general_buffer_group *bufgroup, music_data *mdata);
void sdlmixer_start_music(music_data *mdata, char *music_path);
void sdlmixer_stop_music(music_data *mdata);
char pac_init_sdlmixer(music_data *mdata);
void nuklearapi_set_style(struct nk_context *ctx);
char pac_init_sdl(sdl_apidata *sdldata);

#ifdef __cplusplus
}
#endif

#define _2PACWAV_DOT_H
#endif
