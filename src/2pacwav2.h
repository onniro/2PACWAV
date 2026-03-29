
/*
File: 2pacwav2.h
Date: Thu 24 Apr 2025 04:34:59 PM EEST
*/

#ifndef _2PACWAV_DOT_H

#include <GL/gl.h>
#include <limits.h>
//#include <libavcodec/avfft.h>

#include "2pacmixer.h"

#define _2PACWAV_VER_MAJOR      (0)
#define _2PACWAV_VER_MINOR      (13)
#define _2PACWAV_VER_PATCH      (1)

//#define PAC_INLINE static inline
//#define PAC_INTERNAL static
#define PAC_LOCAL_STATIC static

#define WINDOW_WIDTH    (1280)
#define WINDOW_HEIGHT   (800)
//#define MAX_FRAMETIME_MICROSEC ((useconds_t)11111) //90fps
#define MAX_FRAMETIME_MICROSEC ((useconds_t)16667) //60fps
//#define MAX_FRAMETIME_MICROSEC ((useconds_t)33333) //30fps
#define PAC_SEEK_VALUE_MAX (1000.0f)

//#define PAC_LATIN_FONT_STRING   "DejaVuSansMono.ttf"
#define PAC_LATIN_FONT_STRING   "LiberationMono-Regular.ttf"
#define PAC_CJK_FONT_STRING     "NotoSansMonoCJKhk-Regular.otf"
#define PAC_LATIN_FONTSIZE (17.0f)
#define PAC_CJK_FONTSIZE (18.0f)

static const uint8_t _stop_btn_glyph[4] = { 0xE2, 0x96, 0xA0, 0x00 };

#define PAC_NOP_MACRO(...)
#if _2PACWAV_LINUX
    #define PLATFORM_EXITCALL(code) _exit(code)
#elif _2PACWAV_WIN32
    #define PLATFORM_EXITCALL(code) ExitProcess(code)
#endif

#define PAC_ASSERT(expression)\
    if (!(expression)) {\
        platform_log("ASSERTION FAILED. file: %s @ L%d\nexpression: (%s)\n",\
                __FILE__, __LINE__, #expression);\
        PLATFORM_EXITCALL(1337);\
    }

#ifndef PAC_SAMPLE_RATE
    //#define PAC_SAMPLE_RATE MIX_DEFAULT_FREQUENCY
    #define PAC_SAMPLE_RATE (48000)
#endif
#ifndef PAC_PCM_BITS
    //#define PAC_PCM_BITS MIX_DEFAULT_FORMAT
    #define PAC_PCM_BITS ("do not use PAC_PCM_BITS")
#endif
#ifndef PAC_SDLMIXER_CHUNKSIZE
    #define PAC_SDLMIXER_CHUNKSIZE (2048)
#endif

#define PAC_VISUALIZER_BYTES2COPY (2048)

#ifndef PAC_CHAN_COUNT
    #define PAC_CHAN_COUNT (2)
#endif

#define MLIST_MIN_WIN_WIDTH     (110)
#define MLIST_MIN_WIN_HEIGHT    (20)

#define PAC_MAX_FILES   (8192)
#define PAC_MAX_DIRS    (128)

#define PAC_PI32 (3.14159265f)

#define PAC_MAIN_STORAGE_SIZE               (5*(1024*1024))
#define DEBUG_BUFFER_SIZE                   (8192)
#define USERINFO_BUFFER_SIZE                (512)
#define FILENAMES_BUFFER_SIZE               (PAC_MAX_FILES*NAME_MAX)
#define DIRNAMES_BUFFER_SIZE                (PAC_MAX_DIRS*PATH_MAX)
#define FILENAMEBUF_LOCATION_LIST_SIZE      (sizeof(char *)*PAC_MAX_FILES)
#define DIRNAMEBUF_LOCATION_LIST_SIZE       (sizeof(char *)*64)
#define SEARCH_BUFFER_SIZE                  (NAME_MAX)
#define MATCH_FLAGS_BUFFER_SIZE             (PAC_MAX_FILES*sizeof(char))
#define CONFBUFFER_SIZE                     (8192)

#define FFT_FLOAT_COUNT                     (8192) //this is way too big
#define FFT_COMPLEX32_BUFFER_SIZE           ((FFT_FLOAT_COUNT)*sizeof(Complex32))
#define PAC_SPECTRUM_FREQ_BIN_COUNT         (800)
#define PAC_OSCILLOSCOPE_POINT_COUNT        (PAC_SDLMIXER_CHUNKSIZE/2)

#define PAC_HOLD_WAIT_FRAMES (25)
#define PAC_HOLD_INCREMENT_MODULO (3) //controls how fast u scroll

#define PAC_DEFAULT_SEEK_INCREMENT (5.0f) //(seconds)
#define PAC_DEFAULT_VOLUME_INCREMENT (5) //(seconds)

//typedef _Complex float Complex32;
//typedef FFTComplex Complex32;

struct Runtime_Vars;
struct General_Buffer_Group;
struct File_List;
struct Frametime_Vars;
struct State_Flags;
struct Widget_Bounds_Info;
struct Audio_Metadata_Group;
struct Bitmap_Info;
struct Sdl_Apidata;
struct Music_Data;
struct Startup_Args_Paths;
struct Startup_Args;
struct Mouse_State;
struct Metadata_Editor;
struct Audio_Stream;

#define PAC_CONFNAME_STRING         "2wconf"

typedef struct General_Buffer_Group {
    void *inbuf_filename;
    void *inbuf_search;
    void *music_current_filename;
    void *music_info_buffer;
    void *conf_file_buffer;
    void *userinfo_buffer;
    void *flist_filenames_buf;
    void *flist_dirnames_buf;
    void *flist_filenames_string_loclist;
    void *flist_dirnames_string_loclist;
    void *flist_match_flags;
    void *autocomp_string_loclist;
    void *autocomp_buffer;
    void *scratch_space;
    uint64_t scratch_bytes;
    char *fontpath_ptr;
    void *fft_complex32_buffer;
    char sort_text[32] = "sort (a-z)";
    char ls_toggle_text[16] = "hide list";
    char play_toggle_text[24] = "pause";
    char shuf_toggle_text[24] = "enable shuffle";
    char loop_toggle_text[24] = "enable looping";
    char vis_toggle_text[24] = "disable visualizer";
} General_Buffer_Group;

typedef struct Startup_Args_Paths {
    int count;
    char *ptrs[PAC_MAX_DIRS];
    char *buffer;
} Startup_Args_Paths;

typedef struct Startup_Args {
    char no_load_conf;
    char no_load_startup_paths;
    General_Buffer_Group *bufgroup_ptr;
    Startup_Args_Paths paths;
    float font_size;
    int volume;
    int volume_step; //how much the volume changes when adjusted with keybind
    char *conf_path;
} Startup_Args;

typedef struct File_List {
    int entry_count;
    int dirs_added;
    int current_index; //why is this here
    int match_count;
    int context_index;
    int sel_index;
    char *dirnames_buf;                 //buffer containing the directories added, delimited by null
    char *filenames_buf;                //buffer containing file names, delimited by null
    char **filenames_string_loclist;    //array of pointers which specify the beginnings of strings in the filename array
    char **dirnames_string_loclist;     //array of pointers which specify the beginnings of strings in the dirname array
    char *match_flags;                  //buffer whose indices map to indices of filenames_buf, set to 1 if the index should *NOT* be shown in the list
} File_List;

typedef struct Frametime_Vars {
    uint64_t start;
    uint64_t end;
    uint64_t delta;
} Frametime_Vars;

typedef enum Userinfo_Type {
    USERINFO_TYPE_ERROR = 0,
    USERINFO_TYPE_WARNING,
    USERINFO_TYPE_NOTE,
    USERINFO_TYPE__LAST
} Userinfo_Type;

typedef enum Center_View_State {
    CENTER_VIEW_STATE_MUSIC_LIST = 0,
    CENTER_VIEW_STATE_CURRENT_INFO,
    CENTER_VIEW_STATE_METADATA_EDITOR,
    CENTER_VIEW_STATE__LAST
} Center_View_State;

static inline void cycle_center_view_state(Center_View_State *value) {
    if (!value) { return; }
    uint8_t new_value = 1 + (uint8_t)(*value);
    if (new_value != CENTER_VIEW_STATE__LAST) {
        *value = (Center_View_State)new_value;
    } else {
        *value = (Center_View_State)0;
    }
}

#define PAC_SDL_MOUSELEFT   (1)
#define PAC_SDL_MOUSEMIDDLE (2)
#define PAC_SDL_MOUSERIGHT  (3)

typedef struct Mouse_State {
    char down;
    char wasdown_flags[4];
    struct {
        int x;
        int y;
    } pos;
} Mouse_State;

typedef enum Sort_State {
    SORT_STATE_ALPHA_ASCENDING = 0,
    SORT_STATE_ALPHA_DESCENDING,
    SORT_STATE_MOD_DATE_ASCENDING,
    SORT_STATE_MOD_DATE_DESCENDING,
    SORT_STATE__LAST
} Sort_State;

static inline void cycle_sort_state(Sort_State *value) {
    if (!value) { return; }
    uint8_t new_value = 1 + (uint8_t)(*value);
    if (new_value != SORT_STATE__LAST) {
        *value = (Sort_State)new_value;
    } else {
        *value = (Sort_State)0;
    }
}

typedef struct State_Flags {
    char d_wasdown;
    char q_wasdown;
    char x_wasdown;
    char s_wasdown;
    char l_wasdown;
    char f_wasdown;
    char n_wasdown;
    char m_wasdown;
    char p_wasdown;
    char r_wasdown;
    char right_wasdown;
    char left_wasdown;
    char up_wasdown;
    char down_wasdown;
    char zero_wasdown;
    char nine_wasdown;
    char space_wasdown;
    char enter_wasdown;
    char tab_wasdown;
    char esc_wasdown;
    char home_wasdown;
    char clear_confirmation;
    char search_changed;
    char text_field_focused;
    char visualizer_enabled;
    char mlist_ctxmenu_active;
    char searchwindow_open;
    char colorpicker_open;
    Mouse_State mouse;
    Center_View_State viewstate;
    Userinfo_Type last_userinfo_type;
    Sort_State sort_state;
} State_Flags;

#define META_EDITOR_BUFSIZE 256

typedef struct Metadata_Editor {
    char inbuf_title[META_EDITOR_BUFSIZE];
    char inbuf_artist[META_EDITOR_BUFSIZE];
    char inbuf_album[META_EDITOR_BUFSIZE];
    char inbuf_genre[META_EDITOR_BUFSIZE];
    char inbuf_tracknum[16];
    char inbuf_date[64];
    char editor_current[PATH_MAX];
    Pacmxr_Metadata meta_struct;
} Metadata_Editor;

typedef struct Audio_Metadata_Group {
    char tagbuffer[1024];
} Audio_Metadata_Group;

typedef struct Bitmap_Info {
    uint8_t *img_data;
    int img_data_bytes;
    int width;
    int height;
    int chan;
    GLuint ogl_tex_id;
} Bitmap_Info;

typedef struct Audio_Stream {
    int stream_size; 
    uint8_t *stream;
    //(unused (((((for now at least))))))
#if PAC_SPECTRUM_ENABLED
    Complex32 *complex32_buffer_in;
    Complex32 *complex32_buffer_out;
    float real32_buffer_final[PAC_SPECTRUM_FREQ_BIN_COUNT];
    float real32_buffer_out[FFT_FLOAT_COUNT];
#endif
    float real32_buffer_in[FFT_FLOAT_COUNT];
} Audio_Stream;

typedef struct Music_Data {
    //char paused;
    char shuffle_enabled;
    char loop_enabled;
    uint16_t pcm_bits;
    int sample_rate;
    int channels;
    /*
    NOTE: after 2pacmixer port chunk_size is a terrible name since 2pacmixer
    also uses the same name for something totally different and also it doesn't
    and also it doesn't actually mean anything anymore really, it just denotes
    how much data the visualizer needs to copy
    */
    int chunk_size;
    //int seek_increment;
    float seek_increment;
    int previous_index;
    int volume;
    int seek_value;
    double current_position;
    double current_duration;
    char *current_filename;
    Audio_Stream astream;
    Runtime_Vars *rtvars_ptr;
    File_List music_list;
    //Mix_Music *sdlmixer_music; //IMPORTANT: ALWAYS SET TO NULL WHEN MUSIC IS UNLOADED
    Audio_Metadata_Group current_metadata;
    Metadata_Editor metaed;
    Bitmap_Info cover;
    char music_type_buf[32];
} Music_Data;

typedef struct Sdl_Apidata {
    int win_width;
    int win_height;
    SDL_Window *window_ptr;
    SDL_GLContext ogl_context;
    Music_Data *mdata_ptr;
} Sdl_Apidata;

typedef struct Runtime_Vars {
    char keep_running;
    const uint8_t *kbd_state;
    State_Flags sflags;
    Frametime_Vars *frametime_info_ptr;
    Startup_Args *sargs_ptr;
    General_Buffer_Group *bufgroup_ptr;
    File_List autocomp_list;
    float vis_color[4]; //strange place for this but aight
    Ro_Heap_Buffer main_storage;
    Sdl_Apidata *sdldata_ptr;
    Music_Data *mdata_ptr;
    Pacmxr_Context *pacmxr_ctx;
    ImFont *main_font;
    char working_directory[PATH_MAX];
    char resource_directory[PATH_MAX];
    char conf_directory[PATH_MAX];
} Runtime_Vars;

static inline char *pac_strnstr(char *haystack, char *needle, int nchars) {
    char *ret = 0, *tmp_full, *tmp_sub;
    for (int chari = 0;
        (chari < nchars) && haystack[chari];
        ++chari) {
        int charj = 0;
        while ((chari + charj < nchars) &&
            haystack[charj] &&
            haystack[chari + charj] == needle[charj])
        { ++charj; }
        if (!needle[charj]) {
            ret = (char *)((uintptr_t)haystack + chari);
            break;
        }
    }
    return ret;
}

static inline char *pac_strnchr(char *haystack, char needle, int nvalue) {
    char *ret = 0;
    int index = 0;
    while (haystack[index] && nvalue) {
        --nvalue;
        if (haystack[index] == needle) {
            ret = &haystack[index];
            break;
        }
        ++index;
    }
    return ret;
}

//(forward declarations)
static void pac_nop(void);
static void get_version_string(char *buffer);
static void show_version(void);
static void pac_do_command_args(int arg_count, char **args);
static void sdlapi_process_events(Runtime_Vars *rtvars, Sdl_Apidata *sdldata);
static void sdlapi_correct_gl_viewport_and_clear(Sdl_Apidata *sdldata);
static void pac_init_bitmap(Bitmap_Info *bmpinfo, Runtime_Vars *rtvars);
static void pac_begin_frame(Runtime_Vars *rtvars, Sdl_Apidata *sdldata);
static void pac_end_frame(Runtime_Vars *rtvars, Sdl_Apidata *sdldata);
static void set_userinfo(Runtime_Vars *rtvars, char *notice, Userinfo_Type notice_type);
static void add_to_music_list(char *path, Music_Data *mdata, Runtime_Vars *rtvars);
static char file_is_playlist(char *path);
static void menu_do_search(Runtime_Vars *rtvars, General_Buffer_Group *bufgroup, Music_Data *mdata);
static void file_list_push_dirname(char *dirname, File_List *flist);
static void goto_next_file(Music_Data *mdata);
static void goto_prev_file(Music_Data *mdata);
static void set_match_flags(char *searchbuf, Music_Data *mdata);
static void update_audio_time(Music_Data *mdata);
static void pac_main_loop(Runtime_Vars *rtvars, Sdl_Apidata *sdldata, General_Buffer_Group *bufgroup, Music_Data *mdata);
static void tupacmixer_start_music(Music_Data *mdata, char *music_path);
static void tupacmixer_stop_music(Music_Data *mdata);
static void tupacmixer_get_taginfo(Music_Data *mdata);
static char pac_init_tupacmixer(Music_Data *mdata);

#define _2PACWAV_DOT_H
#endif
