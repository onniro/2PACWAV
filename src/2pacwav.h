
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
#define _2PACWAV_VER_PATCH      (6)

#include "ro_heapbuf.h"

#define PAC_DEF static inline
#define PAC_INTERNAL static
#define PAC_LOCAL_STATIC static

#define WINDOW_WIDTH    1024
#define WINDOW_HEIGHT   768
#define MAX_FRAMETIME_MICROSEC ((useconds_t)16667)
#define PAC_SEEK_VALUE_MAX (100)

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

#define PAC_HOLD_WAIT_FRAMES (25)
#define PAC_HOLD_INCREMENT_MODULO (3)

struct Runtime_Vars;
struct General_Buffer_Group;
struct File_List;
struct State_Flags;
struct Widget_Bounds_Info;
struct Audio_Metadata_Group;
struct Bitmap_Info;
struct Sdl_Apidata;
struct Music_Data;

typedef struct General_Buffer_Group 
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
} General_Buffer_Group;

typedef struct File_List
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
} File_List;

typedef struct Frametime_Vars
{
    uint64_t start;
    uint64_t end;
    uint64_t delta;
} Frametime_Vars;

typedef enum Userinfo_Type 
{
    USERINFO_TYPE_ERROR = 0,
    USERINFO_TYPE_WARNING,
    USERINFO_TYPE_NOTE,
    USERINFO_TYPE__LAST
} Userinfo_Type;

typedef enum Center_View_State
{
    CENTER_VIEW_STATE_MUSIC_LIST = 0,
    CENTER_VIEW_STATE_CURRENT_INFO,
    CENTER_VIEW_STATE__LAST
} Center_View_State;

PAC_DEF void cycle_center_view_state(Center_View_State *value)
{
    if(!value) 
    { return; }
    uint8_t new_value = 1 + (uint8_t)(*value);
    if(new_value != CENTER_VIEW_STATE__LAST)
    { *value = (Center_View_State)new_value; }
    else
    { *value = (Center_View_State)0; }
}

typedef struct State_Flags
{
    char d_wasdown;
    char x_wasdown;
    char s_wasdown;
    char l_wasdown;
    char f_wasdown;
    char right_wasdown;
    char left_wasdown;
    char up_wasdown;
    char down_wasdown;
    char space_wasdown;
    char enter_wasdown;
    char escape_wasdown;
    char clear_confirmation;
    char text_field_focused;
    Center_View_State viewstate;
    Userinfo_Type last_userinfo_type;
} State_Flags;

typedef struct Widget_Bounds_Info
{
    float width;
    float height;
    float pad;
    float y_alignment;
    float y_offset;
    float x_offset;
    struct nk_rect content_bounds;
} Widget_Bounds_Info;

typedef struct Audio_Metadata_Group
{
    const char *tag_title;
    char *title_begin_in_buf;

    const char *tag_artist;
    char *artist_begin_in_buf;

    const char *tag_album;
    char *album_begin_in_buf;
} Audio_Metadata_Group;

typedef struct Bitmap_Info
{
    struct nk_image nuk_image;
    uint8_t *img_data;
    int img_data_bytes;
    int width;
    int height;
    int chan;
    GLuint ogl_tex_id;
} Bitmap_Info;

typedef struct Music_Data
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
    Runtime_Vars *rtvars_ptr;
    File_List music_list;
    Mix_Music *sdlmixer_music; //IMPORTANT: ALWAYS SET TO NULL WHEN MUSIC IS UNLOADED
    Audio_Metadata_Group current_metadata;
    struct nk_list_view music_list_view;
    Bitmap_Info cover;
    char music_type_buf[16];
} Music_Data;

typedef struct Sdl_Apidata 
{
    int win_width;
    int win_height;
    SDL_Window *window_ptr;
    SDL_GLContext ogl_context;
    Music_Data *mdata_ptr;
} Sdl_Apidata;

typedef struct Runtime_Vars 
{
    char keep_running;
    char *working_directory;
    char *resource_directory;
    State_Flags sflags;
    Frametime_Vars *frametime_info_ptr;
    General_Buffer_Group *bufgroup_ptr;
    Ro_Heap_Buffer main_storage;
    Sdl_Apidata *sdldata_ptr;
    const uint8_t *kbd_state;
    struct nk_font *small_font;
    struct nk_font *big_font;
    struct nk_context *nuklear_ctx;
} Runtime_Vars;

//(forward declarations)

void pac_nop(void);
void get_version_string(char *buffer);
void show_version(void);
void pac_do_command_args(int arg_count, char **args);
nk_rune *pac_font_glyph_ranges(void);
void pac_nuklearapi_paste_callback(nk_handle handle, struct nk_text_edit *txtedit);
PAC_INTERNAL void sdlapi_process_events(Runtime_Vars *rtvars, Sdl_Apidata *sdldata);
PAC_INTERNAL void sdlapi_correct_gl_viewport_and_clear(Sdl_Apidata *sdldata);
PAC_INTERNAL void pac_init_bitmap(Bitmap_Info *bmpinfo, Runtime_Vars *rtvars);
PAC_INTERNAL void pac_begin_frame(Runtime_Vars *rtvars, Sdl_Apidata *sdldata);
PAC_INTERNAL void pac_end_frame(Runtime_Vars *rtvars, Sdl_Apidata *sdldata);
void set_userinfo(Runtime_Vars *rtvars, char *notice, Userinfo_Type notice_type);
void file_list_push_dirname(char *dirname, File_List *flist);
PAC_INTERNAL void goto_next_file(Music_Data *mdata);
PAC_INTERNAL void goto_prev_file(Music_Data *mdata);
PAC_INTERNAL void set_match_flags(char *searchbuf, Music_Data *mdata);
PAC_INTERNAL void update_music_info(Music_Data *mdata);
void pac_main_loop(Runtime_Vars *rtvars, Sdl_Apidata *sdldata, General_Buffer_Group *bufgroup, Music_Data *mdata);
PAC_INTERNAL char id3_get_taginfo(Music_Data *mdata);
PAC_INTERNAL char sdlmixer_get_taginfo(Music_Data *mdata);
PAC_INTERNAL void sdlmixer_start_music(Music_Data *mdata, char *music_path);
PAC_INTERNAL void sdlmixer_stop_music(Music_Data *mdata);
char pac_init_sdlmixer(Music_Data *mdata);
void nuklearapi_set_style(struct nk_context *ctx);
char pac_init_sdl(Sdl_Apidata *sdldata);

#ifdef __cplusplus
}
#endif

#define _2PACWAV_DOT_H
#endif
