
/*
File: 2pacwav.c
Date: Sat 22 Feb 2025 06:29:25 PM EET
*/

#include <stdio.h>
#include <limits.h>
#include <stdint.h>
#include <assert.h>

#include "SDL.h"
#include "SDL_mixer.h"
#include <GL/gl.h>

#define NK_INCLUDE_FONT_BAKING
#define NK_ASSERT
#include "nuklear.h"
#include "nuklear_sdl_gl2.h"

#include "2pacwav.h"

#if _2PACWAV_LINUX
    ///#include "ro_posix.h"
    #include "linux_2pacwav.h"
#elif _2PACWAV_WIN32
#endif

void pac_nop(void) 
{
    return; 
}

nk_rune *pac_font_glyph_ranges(void) 
{
    //NOTE: pasted from nuklear.h
    //CLEANUP: ULTRA SUPER DUPER MEGA HOOD GHETTO WALMART FIX 
    //(fixes crash at least but the glyphs dont actually render)
    static const nk_rune ranges[] = 
    {
#if 0
        0x0020, 0x00FF,
        0x0400, 0x052F,
        0x25A0, 0x25A1,
        0x2DE0, 0x2DFF,
        0x3000, 0x303F,
        0x3040, 0x309F,
        0x30A0, 0x30FF,
        0x3131, 0x3163,
        0x4E00, 0x9FFF,
        0xA640, 0xA69F,
        0xAC00, 0xD79D,
#else
        0x0020, 0xFFFF,
#endif
        0
    };
    return (nk_rune *)ranges;
}

//basically the same function as in nuklear_sdl_gl2.h (default callback)
//but this one calls nk_textedit_text instead of nk_textedit_paste which 
//avoids crash when pasting unicode 
//TODO: copying unicode chars doesn't quite seem to work
void pac_nuklearapi_paste_callback(nk_handle handle, struct nk_text_edit *txtedit) 
{
    const char *clip_bytes = SDL_GetClipboardText();
    if(clip_bytes) 
    {
        nk_textedit_text(txtedit, clip_bytes, strlen(clip_bytes));
        SDL_free((void *)clip_bytes);
    }
}

char pac_btn_press(SDL_Scancode scan, char *wasdown, const uint8_t *kdb_state) 
{
    char state = 0;
    if(kdb_state[scan]) 
    {
        if(!*wasdown) 
        { state = 1; *wasdown = 1; }
    } 
    else 
    { *wasdown = 0; }
    return state;
}

void sdlapi_process_events(runtime_vars *rtvars, sdl_apidata *sdldata) 
{
    SDL_Event event;
    nk_input_begin(rtvars->nuklear_ctx);
    rtvars->kbd_state = SDL_GetKeyboardState(0);

    while(SDL_PollEvent(&event)) 
    {
        switch(event.type) 
        {
        case SDL_QUIT: 
        {
            rtvars->keep_running = 0;
        } break;

        case SDL_DROPFILE:
        {
            strncpy((char *)rtvars->bufgroup_ptr->inbuf_filename, event.drop.file, PATH_MAX);
        } break;
        }
        nk_sdl_handle_event(&event);
    }

    nk_input_end(rtvars->nuklear_ctx);
}

void sdlapi_correct_gl_viewport_and_clear(sdl_apidata *sdldata)
{
    SDL_GetWindowSize(sdldata->window_ptr, &sdldata->win_width, &sdldata->win_height);
    glViewport(0, 0, sdldata->win_width, sdldata->win_height);
    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
}

void pac_begin_frame(runtime_vars *rtvars, sdl_apidata *sdldata)
{
    sdlapi_correct_gl_viewport_and_clear(sdldata);
    sdlapi_process_events(rtvars, sdldata);
    nk_begin(rtvars->nuklear_ctx, 
            "2PACWAV", 
            nk_rect(0, 0, sdldata->win_width, sdldata->win_height), 
            NK_WINDOW_NO_INPUT|NK_WINDOW_NO_SCROLLBAR);
}

void pac_end_frame(runtime_vars *rtvars, sdl_apidata *sdldata)
{
    nk_end(rtvars->nuklear_ctx);
    nk_sdl_render(NK_ANTI_ALIASING_ON);
    SDL_GL_SwapWindow(sdldata->window_ptr);
}

void update_music_info(music_data *mdata)
{
    mdata->current_duration = Mix_MusicDuration(mdata->sdlmixer_music);
    mdata->current_position = Mix_GetMusicPosition(mdata->sdlmixer_music);
}

int pac_qsort_strcasecmp(const void *a, const void *b)
{
    int result = strcasecmp(*(const char **)a, *(const char **)b);
    return result;
}

void sort_file_list_alpha(file_list *flist)
{
    char **strings = flist->filenames_string_loclist;
    int sort_count = flist->entry_count;
    qsort(strings, sort_count, sizeof(char **), pac_qsort_strcasecmp);
}

char *update_debug_buffer_info(music_data *mdata, general_buffer_group *bufgroup) 
{
    snprintf((char *)bufgroup->debug_buffer, DEBUG_BUFFER_SIZE - 1, 
            "[srate:%dhz][pcm_bits:%d][chan:%d][vol:%d/128][pos:%6.1f/%6.1f][type:%s]\000", 
            mdata->sample_rate, 
            mdata->pcm_bits, 
            mdata->channels,
            mdata->volume,
            mdata->current_position,
            mdata->current_duration,
            mdata->music_type_buf);

    //lol
    int dbglen = strlen((char *)bufgroup->debug_buffer);
    char *second_dbg_buf = ((char *)bufgroup->debug_buffer + dbglen) + 2;
    snprintf(second_dbg_buf, (DEBUG_BUFFER_SIZE - 1) - dbglen,
            "[path:%s]", mdata->current_filename);
    return second_dbg_buf;
}

void sdlmixer_get_music_type(music_data *mdata)
{
    Mix_MusicType music_type = Mix_GetMusicType(mdata->sdlmixer_music);
    switch(music_type)
    {
        case MUS_CMD: strcpy(mdata->music_type_buf, "CMD"); break;
        case MUS_MOD: strcpy(mdata->music_type_buf, "MOD"); break;
        case MUS_FLAC: strcpy(mdata->music_type_buf, "FLAC"); break;
        case MUS_MID: strcpy(mdata->music_type_buf, "MIDI"); break;
        case MUS_OGG: strcpy(mdata->music_type_buf, "OGG"); break;
        case MUS_MP3_MAD_UNUSED:
        case MUS_MP3: strcpy(mdata->music_type_buf, "MP3"); break;
        case MUS_OPUS: strcpy(mdata->music_type_buf, "OPUS"); break;
        case MUS_WAVPACK: strcpy(mdata->music_type_buf, "WAVPACK"); break;
        case MUS_NONE:
        default: strcpy(mdata->music_type_buf, "NONE"); break;
    }
}

void load_file_from_path(char *path, music_data *mdata)
{
    if(platform_file_exists(path))
    { sdlmixer_start_music(mdata, path); }
    else
    { platform_log("%s: no such file or directory\n", path); }
}

char *separate_file_and_dir_name(char *dir_in_out, char *name_out) 
{
    int length = strlen(dir_in_out);
    char *temp_in = dir_in_out + length;
    int chars = 0;
    while(temp_in != dir_in_out) 
    {
        if(*temp_in == '/') 
        {
            *temp_in = 0x0;
            strncpy(name_out, temp_in + 1, chars);
            break;
        }
        --temp_in; ++chars;
    }
    return dir_in_out;
}

void add_single_file_to_music_list(char *path, music_data *mdata)
{
    char dir[PATH_MAX];
    char name[NAME_MAX];
    strncpy(dir, path, PATH_MAX - 1);
    separate_file_and_dir_name(dir, name);
    file_list *mlist = &mdata->music_list;

    char *file_entry = mlist->filenames_string_loclist[mlist->entry_count];
    char *dir_entry = mlist->dirnames_string_loclist[mlist->dirs_added];
    strncpy(file_entry, name, NAME_MAX - 1);
    strncpy(dir_entry, dir, PATH_MAX - 1);
    int file_len = strlen(name);
    int dir_len = strlen(dir);
    file_entry[file_len + 1] = 0x0;
    dir_entry[dir_len + 1] = 0x0;

    mlist->filenames_string_loclist[mlist->entry_count + 1] = file_entry + file_len + 1;
    mlist->dirnames_string_loclist[mlist->dirs_added + 1] = dir_entry + dir_len + 1;
    ++mdata->music_list.entry_count;
    ++mdata->music_list.dirs_added;
}

void add_to_music_list(char *path, music_data *mdata, runtime_vars *rtvars)
{
    if(platform_file_exists(path))
    { add_single_file_to_music_list(path, mdata); }
    else if(platform_directory_exists(path))
#if 1
    { platform_get_directory_listing(path, &mdata->music_list); }
#else
    { platform_get_directory_listing_presorted(path, &mdata->music_list, rtvars); }
#endif
    else 
    { platform_log("%s: no such file or directory\n", path); }
}

char *find_top_level_path4file(char *filename, file_list *flist)
{
    char *result = 0, *dirname;
    char pathbuf[PATH_MAX];
    int dir_count = flist->dirs_added;
    for(int dir_index = 0; dir_index < dir_count; ++dir_index)
    {
        dirname = flist->dirnames_string_loclist[dir_index];
        snprintf(pathbuf, PATH_MAX - 1, "%s/%s", dirname, filename);
        if(platform_file_exists(pathbuf))
        { result = dirname; break; }
    }
    return result;
}

void menu_do_music_list(runtime_vars *rtvars, 
                    music_data *mdata, 
                    widget_bounds_info *bound_info)
{
    rtvars->nuklear_ctx->style.button.text_alignment = NK_TEXT_LEFT;
    nk_layout_space_push(rtvars->nuklear_ctx, 
                        nk_rect(bound_info->height + bound_info->pad, 
                        bound_info->y_offset, 
                        bound_info->width - (bound_info->height + bound_info->pad), 
                        bound_info->content_bounds.h - bound_info->height*4));
    nk_list_view_begin(rtvars->nuklear_ctx, 
                    &mdata->music_list_view, 
                    "music", 
                    NK_WINDOW_BORDER, 
                    bound_info->height - bound_info->y_alignment, 
                    mdata->music_list.entry_count);

    nk_layout_row_dynamic(rtvars->nuklear_ctx, bound_info->height - bound_info->y_alignment, 1);
    for(uint32_t file_index = 0; 
            file_index < (uint32_t)mdata->music_list_view.count;
            ++file_index)
    {
        uint32_t render_index = file_index + mdata->music_list_view.begin;
        char *btntext = mdata->music_list.filenames_string_loclist[render_index];
        if(nk_button_label(rtvars->nuklear_ctx, btntext))
        { 
            char path_buf[PATH_MAX];
            char *toplevel_path = find_top_level_path4file(btntext, &mdata->music_list);
            if(toplevel_path)
            {
                snprintf(path_buf, PATH_MAX - 1,
                        "%s/%s", toplevel_path, btntext);
                platform_log("trying to play %s\n", path_buf);
                strncpy(mdata->current_filename, path_buf, PATH_MAX - 1);
                load_file_from_path(path_buf, mdata);
            }
            else
            {
                platform_log("could not find containing directory for file %s.\n", btntext);
            }
        }
    }

    nk_list_view_end(&mdata->music_list_view);
    rtvars->nuklear_ctx->style.button.text_alignment = NK_TEXT_CENTERED;
}

void menu_show_debuginfo(runtime_vars *rtvars, 
                        general_buffer_group *bufgroup, 
                        widget_bounds_info *bound_info,
                        char *second_debug_buf)
{
    nk_layout_space_push(rtvars->nuklear_ctx, 
                        nk_rect(0, 
                        bound_info->y_offset, 
                        bound_info->width, 
                        bound_info->height - 20));
    bound_info->y_offset += 20;
    struct nk_color debug_col = nk_rgba(0x33, 0xCC, 0x33, 0xFF);
    nk_text_colored(rtvars->nuklear_ctx, 
                    (char *)bufgroup->debug_buffer, 
                    strlen((char *)bufgroup->debug_buffer), 
                    NK_TEXT_LEFT, 
                    debug_col);

    nk_layout_space_push(rtvars->nuklear_ctx,
                        nk_rect(0, 
                        bound_info->y_offset, 
                        bound_info->width, 
                        bound_info->height - 20));
    bound_info->y_offset += 20;
    nk_text_colored(rtvars->nuklear_ctx, 
                    second_debug_buf, 
                    strlen(second_debug_buf), 
                    NK_TEXT_LEFT, 
                    debug_col);
}

void query_bounds_info(struct nk_context *nuklear_ctx, widget_bounds_info *bound_info)
{
    struct nk_rect widget_bounds = nk_layout_widget_bounds(nuklear_ctx);
    bound_info->width = widget_bounds.w;
    bound_info->height = widget_bounds.h;
    bound_info->content_bounds = nk_window_get_content_region(nuklear_ctx);
    bound_info->pad = widget_bounds.y - bound_info->content_bounds.y;
    bound_info->content_bounds.h -= bound_info->pad;
    bound_info->y_alignment = 10;
    bound_info->x_offset = 0.0f;
    bound_info->y_offset = 0.0f;
}

//kek
void set_playback_btn(char *btn, char paused) 
{
    if(paused) 
    {
        btn[0] = '>';  
        btn[1] = 0;
    } 
    else 
    {
        btn[0] = '|'; 
        btn[1] = '|'; 
        btn[2] = 0;
    }
}

double conv_slide_value2songpos(music_data *mdata)
{
    double result = 0;
    if(mdata->sdlmixer_music && 
            mdata->current_position && 
            mdata->current_duration)
    { result = (((double)(mdata->seek_value))/PAC_SEEK_VALUE_MAX)*mdata->current_duration; }
    return result;
}

float conv_songpos2slide_value(music_data *mdata) 
{
    float result = 0.0f;
    if(mdata->sdlmixer_music && 
            mdata->current_position && 
            mdata->current_duration)
    { result = ((float)(mdata->current_position/mdata->current_duration))*PAC_SEEK_VALUE_MAX; }
    return result;
}

void pac_main_loop(runtime_vars *rtvars, 
                sdl_apidata *sdldata, 
                general_buffer_group *bufgroup,
                music_data *mdata)
{
    static char playback_btn_text[4] = ">";
    static char d_wasdown = 0;

    if(mdata->sdlmixer_music && (Mix_PlayingMusic() || Mix_PausedMusic()))
    { update_music_info(mdata); }
    char *second_debug_buf = update_debug_buffer_info(mdata, bufgroup);

    pac_begin_frame(rtvars, sdldata);

    ////////////////////////////////

    nk_layout_space_begin(rtvars->nuklear_ctx, NK_STATIC, 0, INT_MAX);
    widget_bounds_info bound_info = {0};
    query_bounds_info(rtvars->nuklear_ctx, &bound_info);
    float add_width = 80.0f;
    float vol_width = 180.0f;

    menu_show_debuginfo(rtvars, bufgroup, &bound_info, second_debug_buf);

    nk_layout_space_push(rtvars->nuklear_ctx, 
                        nk_rect(0, 
                        bound_info.y_offset, 
                        bound_info.width - add_width - bound_info.pad, 
                        bound_info.height - bound_info.y_alignment));

    if(rtvars->kbd_state[SDL_SCANCODE_LALT])
    {
        if(pac_btn_press(SDL_SCANCODE_D, &d_wasdown, rtvars->kbd_state))
        { nk_edit_focus(rtvars->nuklear_ctx, NK_TEXT_EDIT_MODE_INSERT); }
    }
    //if(nuklear_edit_has_focus(rtvars->nuklear_ctx))
    //{ printf("hello\n"); }

    nk_edit_string_zero_terminated(rtvars->nuklear_ctx, 
                                NK_EDIT_FIELD, 
                                (char *)bufgroup->inbuf_filename, 
                                PATH_MAX - 1, 
                                nk_filter_default);

    nk_layout_space_push(rtvars->nuklear_ctx, 
                        nk_rect(bound_info.width - add_width, 
                        bound_info.y_offset, 
                        add_width, 
                        bound_info.height - bound_info.y_alignment));
    bound_info.y_offset += bound_info.y_alignment + 20;
    if(nk_button_label(rtvars->nuklear_ctx, "add"))
    { add_to_music_list((char *)bufgroup->inbuf_filename, mdata, rtvars); }

    nk_layout_space_push(rtvars->nuklear_ctx, 
                        nk_rect(bound_info.height + bound_info.pad, 
                        bound_info.y_offset, 
                        add_width*2, 
                        bound_info.height - (bound_info.y_alignment + bound_info.pad)));
    bound_info.y_offset += bound_info.height - bound_info.y_alignment;
    if(nk_button_label(rtvars->nuklear_ctx, "sort (down)"))
    {
        if(mdata->music_list.entry_count)
        {
            sort_file_list_alpha(&mdata->music_list);
        }
    }

    menu_do_music_list(rtvars, mdata, &bound_info);

    nk_layout_space_push(rtvars->nuklear_ctx, 
                        nk_rect(bound_info.x_offset, 
                        bound_info.content_bounds.h - (bound_info.height + bound_info.pad)*4, 
                        bound_info.height, 
                        bound_info.height));
    if(nk_button_label(rtvars->nuklear_ctx, ">>")) //next
    {
    }

    nk_layout_space_push(rtvars->nuklear_ctx, 
                        nk_rect(bound_info.x_offset, 
                        bound_info.content_bounds.h - (bound_info.height + bound_info.pad)*3, 
                        bound_info.height, 
                        bound_info.height));
    if(nk_button_label(rtvars->nuklear_ctx, "<<")) //prev
    {
    }

    nk_layout_space_push(rtvars->nuklear_ctx, 
                        nk_rect(bound_info.x_offset, 
                        bound_info.content_bounds.h - (bound_info.height + bound_info.pad)*2, 
                        bound_info.height, 
                        bound_info.height));
#if 0
    if(nk_button_label(rtvars->nuklear_ctx, (char *)_stop_btn_glyph)) //stop
#else
    if(nk_button_label(rtvars->nuklear_ctx, "[]")) //stop
#endif
    { sdlmixer_stop_music(mdata); }

    nk_layout_space_push(rtvars->nuklear_ctx, 
                        nk_rect(bound_info.x_offset, 
                        bound_info.content_bounds.h - (bound_info.height + bound_info.pad), 
                        bound_info.height, 
                        bound_info.height));
    bound_info.x_offset += (bound_info.height + bound_info.pad);
    
    if(nk_button_label(rtvars->nuklear_ctx, playback_btn_text)) //play
    {
        if(!mdata->paused && mdata->sdlmixer_music) 
        {
            mdata->paused = 1; 
            Mix_PauseMusic(); 
        } 
        else if(mdata->paused && mdata->sdlmixer_music) 
        { 
            mdata->paused = 0; 
            Mix_ResumeMusic(); 
        }
    }

    set_playback_btn(playback_btn_text, mdata->paused);

    nk_layout_space_push(rtvars->nuklear_ctx, 
                        nk_rect(bound_info.x_offset, 
                        bound_info.content_bounds.h - bound_info.height - bound_info.pad, 
                        vol_width, bound_info.height));
    bound_info.x_offset += (bound_info.height + (vol_width - bound_info.y_alignment));
    if(nk_slider_int(rtvars->nuklear_ctx, 0, &mdata->volume, MIX_MAX_VOLUME, 1))
    { Mix_VolumeMusic(mdata->volume); }

    mdata->seek_value = conv_songpos2slide_value(mdata);
    nk_layout_space_push(rtvars->nuklear_ctx, 
                        nk_rect(bound_info.x_offset, 
                        bound_info.content_bounds.h - bound_info.height - bound_info.pad, 
                        bound_info.width - bound_info.x_offset - bound_info.pad, 
                        bound_info.height));
    if(nk_slider_float(rtvars->nuklear_ctx, 0.0f, &mdata->seek_value, PAC_SEEK_VALUE_MAX, 1.0f))
    {
        if(mdata->sdlmixer_music)
        { Mix_SetMusicPosition(conv_slide_value2songpos(mdata)); }
    }

    ////////////////////////////////

    nk_layout_space_end(rtvars->nuklear_ctx);
    pac_end_frame(rtvars, sdldata);
}

void sdlmixer_start_music(music_data *mdata, char *music_path) 
{
    if(mdata->sdlmixer_music && (Mix_PlayingMusic() || Mix_PausedMusic()))
    { sdlmixer_stop_music(mdata); }

    mdata->sdlmixer_music = Mix_LoadMUS(music_path);

    if(mdata->sdlmixer_music)
    { 
        mdata->paused = 0;
        sdlmixer_get_music_type(mdata);
        Mix_FadeInMusic(mdata->sdlmixer_music, 0, 0); 
    }
    else
    { fprintf(stderr, "failed to load music. desc: %s\n", SDL_GetError()); }
}

void sdlmixer_stop_music(music_data *mdata)
{
    if(mdata->sdlmixer_music)
    {
        Mix_HaltMusic();
        Mix_FreeMusic(mdata->sdlmixer_music);
        mdata->sdlmixer_music = 0;
        strcpy(mdata->music_type_buf, "NONE");
    }
}

char pac_init_sdlmixer(music_data *mdata) 
{
    char result = 0;
    Mix_Init(MIX_INIT_FLAC
            |MIX_INIT_MOD
            |MIX_INIT_MP3
            |MIX_INIT_OGG
            |MIX_INIT_MID
            |MIX_INIT_OPUS
            |MIX_INIT_WAVPACK);

    int flags = SDL_AUDIO_ALLOW_FREQUENCY_CHANGE|SDL_AUDIO_ALLOW_CHANNELS_CHANGE;
    mdata->sample_rate = PAC_SAMPLE_RATE;
    mdata->pcm_bits = PAC_PCM_BITS;
    mdata->channels = PAC_CHAN_COUNT;
    mdata->chunk_size = PAC_SDLMIXER_CHUNKSIZE;
    mdata->volume = 10;
#if 0
    if(Mix_OpenAudio(mdata->sample_rate, mdata->pcm_bits, mdata->channels, mdata->chunk_size)) 
    { fprintf(stderr, "Mix_OpenAudio failed. desc: %s\n", SDL_GetError()); } 
#else
    if(Mix_OpenAudioDevice(mdata->sample_rate, 
            mdata->pcm_bits, 
            mdata->channels, 
            mdata->chunk_size,
            0,
            flags)) 
    { fprintf(stderr, "failed to open audio device. desc: %s\n", SDL_GetError()); } 
#endif
    else
    { 
        result = 1; 
        Mix_VolumeMusic(mdata->volume);
        Mix_SetMusicCMD(SDL_getenv("MUSIC_CMD"));
        Mix_QuerySpec(&mdata->sample_rate, &mdata->pcm_bits, &mdata->channels);
        mdata->pcm_bits &= 0xFF;
    }
    return result;
}

char pac_init_sdl(sdl_apidata *sdldata) 
{
    char result = 0;
    if(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO)) 
    {
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

        sdldata->window_ptr = SDL_CreateWindow("2PACWAV", 
                                SDL_WINDOWPOS_UNDEFINED, 
                                SDL_WINDOWPOS_UNDEFINED, 
                                WINDOW_WIDTH, 
                                WINDOW_HEIGHT, 
                                SDL_WINDOW_RESIZABLE|SDL_WINDOW_OPENGL);
        if(sdldata->window_ptr) 
        {
            sdldata->ogl_context = SDL_GL_CreateContext(sdldata->window_ptr);
            if(sdldata->ogl_context) 
            { result = 1; }
            else 
            { fprintf(stderr, "SDL failed to create OpenGL context\n"); }
        } 
        else 
        { fprintf(stderr, "SDL failed to create window\n"); }
    } 
    else 
    { fprintf(stderr, "SDL failed to init\n"); }
    return result;
}

void nuklearapi_set_style(struct nk_context *ctx) 
{
    struct nk_color color_tbl[NK_COLOR_COUNT];
    color_tbl[NK_COLOR_TEXT] = nk_rgba(0xCC, 0xAA, 0x00, 0xFF);
    color_tbl[NK_COLOR_WINDOW] = nk_rgba(0, 0, 0, 255);
    color_tbl[NK_COLOR_HEADER] = nk_rgba(51, 51, 56, 220);
    color_tbl[NK_COLOR_BORDER] = nk_rgba(0x88, 0x88, 0x88, 0xFF);
    color_tbl[NK_COLOR_BUTTON] = nk_rgba(0x22, 0x22, 0x2F, 255);
    color_tbl[NK_COLOR_BUTTON_HOVER] = nk_rgba(58, 93, 121, 255);
    color_tbl[NK_COLOR_BUTTON_ACTIVE] = nk_rgba(63, 98, 126, 255);
    color_tbl[NK_COLOR_TOGGLE] = nk_rgba(50, 58, 61, 255);
    color_tbl[NK_COLOR_TOGGLE_HOVER] = nk_rgba(45, 53, 56, 255);
    color_tbl[NK_COLOR_TOGGLE_CURSOR] = nk_rgba(48, 83, 111, 255);
    color_tbl[NK_COLOR_SELECT] = nk_rgba(57, 67, 61, 255);
    color_tbl[NK_COLOR_SELECT_ACTIVE] = nk_rgba(48, 83, 111, 255);
    color_tbl[NK_COLOR_SLIDER] = nk_rgba(50, 58, 61, 255);
    color_tbl[NK_COLOR_SLIDER_CURSOR] = nk_rgba(48, 83, 111, 245);
    color_tbl[NK_COLOR_SLIDER_CURSOR_HOVER] = nk_rgba(53, 88, 116, 255);
    color_tbl[NK_COLOR_SLIDER_CURSOR_ACTIVE] = nk_rgba(58, 93, 121, 255);
    color_tbl[NK_COLOR_PROPERTY] = nk_rgba(50, 58, 61, 255);
    color_tbl[NK_COLOR_EDIT] = nk_rgba(50, 58, 61, 225);
    color_tbl[NK_COLOR_EDIT_CURSOR] = nk_rgba(210, 210, 210, 255);
    color_tbl[NK_COLOR_COMBO] = nk_rgba(50, 58, 61, 255);
    color_tbl[NK_COLOR_CHART] = nk_rgba(50, 58, 61, 255);
    color_tbl[NK_COLOR_CHART_COLOR] = nk_rgba(48, 83, 111, 255);
    color_tbl[NK_COLOR_CHART_COLOR_HIGHLIGHT] = nk_rgba(255, 0, 0, 255);
    color_tbl[NK_COLOR_SCROLLBAR] = nk_rgba(50, 58, 61, 255);
    color_tbl[NK_COLOR_SCROLLBAR_CURSOR] = nk_rgba(48, 83, 111, 255);
    color_tbl[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = nk_rgba(53, 88, 116, 255);
    color_tbl[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgba(58, 93, 121, 255);
    color_tbl[NK_COLOR_TAB_HEADER] = nk_rgba(48, 83, 111, 255);
    color_tbl[NK_COLOR_KNOB] = color_tbl[NK_COLOR_SLIDER];
    color_tbl[NK_COLOR_KNOB_CURSOR] = color_tbl[NK_COLOR_SLIDER_CURSOR];
    color_tbl[NK_COLOR_KNOB_CURSOR_HOVER] = color_tbl[NK_COLOR_SLIDER_CURSOR_HOVER];
    color_tbl[NK_COLOR_KNOB_CURSOR_ACTIVE] = color_tbl[NK_COLOR_SLIDER_CURSOR_ACTIVE];
    nk_style_from_table(ctx, color_tbl);
}
