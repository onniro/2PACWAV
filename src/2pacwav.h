
/*
File: 2pacwav.h
Date: Tue 18 Feb 2025 12:56:32 PM EET
*/

#ifndef _2PACWAV_DOT_H

#ifdef __cplusplus
extern "C"
{
#endif

#define _2PACWAV_VER_MAJOR      (0)
#define _2PACWAV_VER_MINOR      (1)
#define _2PACWAV_VER_PATCH      (3)

#include "ro_heapbuf.h"

#define PAC_DEF static inline

#define WINDOW_WIDTH    1024
#define WINDOW_HEIGHT   768
#define MAX_FRAMETIME_MICROSEC ((useconds_t)16667)
#define PAC_SEEK_VALUE_MAX (100)

//#define PAC_FONT_STRING "LiberationMono-Regular.ttf"
//#define PAC_BIG_FONT_STRING "LiberationMono-Regular.ttf"
#define PAC_FONT_STRING "DejaVuSans.ttf"
#define PAC_BIG_FONT_STRING "DejaVuSans.ttf"
#define PAC_NUKLEAR_FONTSIZE (15.0f)
#define PAC_NUKLEAR_BIG_FONTSIZE (25.0f)

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
#define DEBUG_BUFFER_SIZE                   (8192)
#define USERINFO_BUFFER_SIZE                (512)
#define FILENAMES_BUFFER_SIZE               (PAC_MAX_FILES*NAME_MAX)
#define DIRNAMES_BUFFER_SIZE                (PAC_MAX_DIRS*PATH_MAX)
#define FILENAMEBUF_LOCATION_LIST_SIZE      (sizeof(char *)*PAC_MAX_FILES)
#define DIRNAMEBUF_LOCATION_LIST_SIZE       (sizeof(char *)*64)
#define SEARCH_BUFFER_SIZE                  (NAME_MAX)
#define MATCH_FLAGS_BUFFER_SIZE             (PAC_MAX_FILES*sizeof(char))

struct runtime_vars;
struct general_buffer_group;
struct file_list;
struct state_flags;
struct widget_bounds_info;
struct audio_metadata_group;
struct bitmap_info;
struct sdl_apidata;
struct music_data;

typedef struct general_buffer_group 
{
    void *inbuf_filename;
    void *inbuf_search;
    void *music_current_filename;
    void *working_directory;
    void *resource_directory;
    void *music_info_buffer;
    void *userinfo_buffer;
    void *flist_filenames_buf;
    void *flist_dirnames_buf;
    void *flist_filenames_string_loclist;
    void *flist_dirnames_string_loclist;
    void *flist_match_flags;
    void *scratch_space;
} general_buffer_group;

typedef struct file_list
{
    uint32_t entry_count; //(files)
    uint32_t dirs_added;
    uint32_t current_index;
    uint32_t match_count;
    int sel_index;
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

typedef enum userinfo_type 
{
    USERINFO_TYPE_ERROR = 0,
    USERINFO_TYPE_WARNING,
    USERINFO_TYPE_NOTE,
    USERINFO_TYPE__LAST
} userinfo_type ;

typedef enum center_view_state
{
    CENTER_VIEW_STATE_MUSIC_LIST = 0,
    CENTER_VIEW_STATE_CURRENT_INFO,
    CENTER_VIEW_STATE__LAST
} center_view_state;

PAC_DEF void cycle_center_view_state(center_view_state *value)
{
    if(!value) 
    { return; }
    uint8_t new_value = 1 + (uint8_t)(*value);
    if(new_value != CENTER_VIEW_STATE__LAST)
    { *value = (center_view_state)new_value; }
    else
    { *value = (center_view_state)0; }
}

typedef struct state_flags
{
    char d_wasdown;
    char x_wasdown;
    char s_wasdown;
    char l_wasdown;
    char right_wasdown;
    char left_wasdown;
    char up_wasdown;
    char down_wasdown;
    char space_wasdown;
    char enter_wasdown;
    char escape_wasdown;
    char clear_confirmation;
    char text_field_focused;
    center_view_state viewstate;
    userinfo_type last_userinfo_type;
} state_flags;

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

typedef struct audio_metadata_group
{
    const char *tag_title;
    char *title_begin_in_buf;

    const char *tag_artist;
    char *artist_begin_in_buf;

    const char *tag_album;
    char *album_begin_in_buf;
} audio_metadata_group;

typedef struct bitmap_info
{
    struct nk_image nuk_image;
    uint8_t *img_data;
    int img_data_bytes;
    int width;
    int height;
    int chan;
    GLuint ogl_tex_id;
} bitmap_info;

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
    runtime_vars *rtvars_ptr;
    file_list music_list;
    Mix_Music *sdlmixer_music; //IMPORTANT: ALWAYS SET TO NULL WHEN MUSIC IS UNLOADED
    audio_metadata_group current_metadata;
    struct nk_list_view music_list_view;
    bitmap_info cover;
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
    char *resource_directory;
    state_flags sflags;
    frametime_vars *frametime_info_ptr;
    general_buffer_group *bufgroup_ptr;
    ro_heap_buffer main_storage;
    sdl_apidata *sdldata_ptr;
    const uint8_t *kbd_state;
    struct nk_font *small_font;
    struct nk_font *big_font;
    struct nk_context *nuklear_ctx;
} runtime_vars;

//(forward declarations)

void pac_nop(void);
void get_version_string(char *buffer);
void show_version(void);
nk_rune *pac_font_glyph_ranges(void);
void pac_nuklearapi_paste_callback(nk_handle handle, struct nk_text_edit *txtedit);
void sdlapi_process_events(runtime_vars *rtvars, sdl_apidata *sdldata);
void sdlapi_correct_gl_viewport_and_clear(sdl_apidata *sdldata);
void pac_init_bitmap(bitmap_info *bmpinfo, runtime_vars *rtvars);
void pac_begin_frame(runtime_vars *rtvars, sdl_apidata *sdldata);
void pac_end_frame(runtime_vars *rtvars, sdl_apidata *sdldata);
void set_userinfo(runtime_vars *rtvars, char *notice, userinfo_type notice_type);
void file_list_push_dirname(char *dirname, file_list *flist);
void goto_next_file(music_data *mdata);
void goto_prev_file(music_data *mdata);
void set_match_flags(char *searchbuf, music_data *mdata);
void update_music_info(music_data *mdata);
void pac_main_loop(runtime_vars *rtvars, sdl_apidata *sdldata, general_buffer_group *bufgroup, music_data *mdata);
char id3_get_taginfo(music_data *mdata);
char sdlmixer_get_taginfo(music_data *mdata);
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
