
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
#define PAC_SEEK_VALUE_MAX (100)

#define PAC_FONT_STRING "LiberationMono-Regular.ttf"
#define PAC_NUKLEAR_FONTSIZE (15.0f)

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
    #define PAC_CHAN_COUNT (2)
#endif

#define MLIST_MIN_WIN_WIDTH     (110)
#define MLIST_MIN_WIN_HEIGHT    (20)

#define PAC_MAX_FILES   (8192)
#define PAC_MAX_DIRS    (128)

#define PAC_MAIN_STORAGE_SIZE               (5*(1024*1024))
#define DEBUG_BUFFER_SIZE                   (PATH_MAX)
#define FILENAMES_BUFFER_SIZE               (PAC_MAX_FILES*NAME_MAX)
#define DIRNAMES_BUFFER_SIZE                (PAC_MAX_DIRS*PATH_MAX)
#define FILENAMEBUF_LOCATION_LIST_SIZE      (sizeof(char *)*PAC_MAX_FILES)
#define DIRNAMEBUF_LOCATION_LIST_SIZE       (sizeof(char *)*64)
#define SEARCH_BUFFER_SIZE                  (NAME_MAX)

//members prefixed with inbuf_ are buffers that get passed to nk_edit_string*
typedef struct general_buffer_group
{
    void *inbuf_filename;
    void *inbuf_search;
    void *music_current_filename;
    void *working_directory;
    void *debug_buffer;
    void *flist_filenames_buf;
    void *flist_dirnames_buf;
    void *flist_filenames_string_loclist;
    void *flist_dirnames_string_loclist;
    void *flist_path_ranges;
    void *flist_match_flags;
} general_buffer_group;

//dumbass
typedef struct file_list
{
    uint32_t entry_count; //(files)
    uint32_t dirs_added;
    uint32_t current_index;
    uint32_t match_count;
    char *dirnames_buf;                 //buffer containing the directories added, delimited by null
    char *filenames_buf;                //buffer containing file names, delimited by null
    char **filenames_string_loclist;    //array of pointers which specify the beginnings of strings in the filename array
    char **dirnames_string_loclist;     //array of pointers which specify the beginnings of strings in the dirname array
    char *match_flags;                  //buffer whose indices map to indices of filenames_buf, set to 1 if the index should *NOT* be rendered
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
    char shuffle_enabled;
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
    //char char_was_pressed;
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
void file_list_push_dirname(char *dirname, file_list *flist);
void set_match_flags(char *searchbuf, music_data *mdata);
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
