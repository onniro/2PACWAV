
/*
File: 2pacwav.h
Date: Tue 18 Feb 2025 12:56:32 PM EET
*/

#ifndef _2PACWAV_DOT_H

#ifdef __cplusplus
extern "C"
{
#endif

#define WINDOW_WIDTH    1024
#define WINDOW_HEIGHT   768
#define PAC_MAIN_STORAGE_SIZE (500*1024)
#define MAX_FRAMETIME_MICROSEC ((useconds_t)16667)
#define PAC_SDLMIXER_CHUNKSIZE (2048)
#define PAC_NUKLEAR_FONTSIZE (16.0f)
#define PAC_SEEK_VALUE_MAX (100)

#define DEBUG_BUFFER_SIZE           (PATH_MAX)
#define PATH_CONTENT_BUFFER_SIZE    (20*1024)

//#define PAC_FONT_STRING "Inconsolata-Regular.ttf"
#define PAC_FONT_STRING "LiberationMono-Regular.ttf"
//#define PAC_FONT_STRING "Hack-Regular.ttf"

static const uint8_t _stop_btn_glyph[4] = {0xE2, 0x96, 0xA0, 0x00};

#define PAC_NOP_MACRO(...)

//members prefixed with inbuf_ are buffers that get passed to nk_edit_string*
typedef struct general_buffer_group
{
    char *inbuf_filename;
    char *music_current_filename;
    char *working_directory;
    char *debug_buffer;
    char *path_content_buffer;
} general_buffer_group;

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
    //char music_is_loaded; //this is redundant since sdlmixer_music should always get set to null when you unload music
    uint16_t pcm_bits;
    int sample_rate;
    int channels;
    int volume;
    int chunk_size;
    float seek_value;
    double current_position;
    double current_duration;
    char *current_filename;
    char **file_name_table;
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
    struct music_data *mdata_ptr;
} sdl_apidata;

typedef struct runtime_vars 
{
    char keep_running;
    char *working_directory;
    frametime_vars *frametime_info_ptr;
    general_buffer_group *bufgroup_ptr;
    ro_heap_buffer main_storage;
    sdl_apidata *sdldata_ptr;
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
