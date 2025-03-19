
/*
File: 2pacwav.cpp
Date: Sat 22 Feb 2025 06:29:25 PM EET
*/

#include <stdio.h>
#include <limits.h>
#include <stdint.h>

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

#define STB_IMAGE_IMPLEMENTATION 1
//#define STBI_ONLY_PNG 1
#define STBI_FAILURE_USERMSG 1
#include "stb/stb_image.h"

void pac_nop(void) 
{
    return; 
}

void get_version_string(char *buffer)
{
#if _2PACWAV_DEBUG
    char buildtype[] = "(debug)";
#else
    char buildtype[] = "(release)";
#endif
    sprintf(buffer, "2PACWAV v%u.%u.%02u %s",
            _2PACWAV_VER_MAJOR,
            _2PACWAV_VER_MINOR,
            _2PACWAV_VER_PATCH,
            buildtype);
}

void show_version(void)
{
    char verbuf[128];
    get_version_string(verbuf);
    int len = strlen(verbuf);
    char *ver_end = verbuf + len;
    sprintf(ver_end, ", compiled %s @ %s", __DATE__, __TIME__);
    platform_log("%s\n", verbuf);
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

char pac_btn_press(SDL_Scancode scan, char *wasdown, const uint8_t *kbd_state) 
{
    char state = 0;
    if(kbd_state[scan]) 
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
    char char_was_pressed = 0;

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
            strncpy((char *)rtvars->bufgroup_ptr->inbuf_filename, 
                    event.drop.file, 
                    PATH_MAX);
        } break;

        case SDL_KEYDOWN:
        {
        } break;

        default:
        {
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

int pac_qsort_strcmp(const void *a, const void *b)
{
    int result = strcasecmp(*(const char **)a, *(const char **)b);
    return result;
}

void sort_file_list_alpha(file_list *flist)
{
    char **strings = flist->filenames_string_loclist;
    int sort_count = flist->entry_count;
    qsort(strings, sort_count, sizeof(char **), pac_qsort_strcmp);
}

char *update_info_buffer(music_data *mdata, general_buffer_group *bufgroup) 
{
    snprintf((char *)bufgroup->info_buffer, DEBUG_BUFFER_SIZE - 1, 
            "[srate:%dhz][pcm_bits:%d][chan:%d][vol:%d/128][pos:%6.1f/%6.1f][type:%s]\n[path:%s]", 
            mdata->sample_rate, 
            mdata->pcm_bits, 
            mdata->channels,
            mdata->volume,
            mdata->current_position,
            mdata->current_duration,
            mdata->music_type_buf,
            mdata->current_filename);

    //lol
#if 1
    return (char *)bufgroup->info_buffer;
#else
    int dbglen = strlen((char *)bufgroup->info_buffer);
    char *second_dbg_buf = ((char *)bufgroup->info_buffer + dbglen) + 2;
    snprintf(second_dbg_buf, (DEBUG_BUFFER_SIZE - 1) - dbglen,
            "[path:%s]", mdata->current_filename);
    return second_dbg_buf;
#endif
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
        case MUS_WAV: strcpy(mdata->music_type_buf, "WAV"); break;
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
        --temp_in; 
        ++chars;
    }
    return dir_in_out;
}

char check_dir_already_added(char *dir, file_list *flist)
{
    char result = 0, *current_dir;
    char **all_dirs = flist->dirnames_string_loclist;
    int dir_count = flist->dirs_added;
    for(int dir_index = 0; dir_index < dir_count; ++dir_index)
    {
        current_dir = all_dirs[dir_index];
        if(!strcmp(current_dir, dir))
        { result = 1; break; }
    }
    return result;
}

void query_bounds_info(struct nk_context *nuklear_ctx, widget_bounds_info *bound_info)
{
    struct nk_rect widget_bounds = nk_layout_widget_bounds(nuklear_ctx);
    bound_info->width = widget_bounds.w;
    bound_info->height = widget_bounds.h;
    bound_info->content_bounds = nk_window_get_content_region(nuklear_ctx);
    bound_info->pad = widget_bounds.y - bound_info->content_bounds.y;
    bound_info->content_bounds.h -= bound_info->pad;
    bound_info->y_alignment = 7.0f;
    bound_info->x_offset = 0.0f;
    bound_info->y_offset = 0.0f;
}

double conv_slide_value2songpos(music_data *mdata)
{
    double result = 0;
    if(mdata->sdlmixer_music && 
            mdata->current_position && 
            mdata->current_duration)
    { 
        result = (((double)(mdata->seek_value)) / 
                PAC_SEEK_VALUE_MAX) *
                mdata->current_duration; 
    }
    return result;
}

float conv_songpos2slide_value(music_data *mdata) 
{
    float result = 0.0f;
    if(mdata->sdlmixer_music && 
            mdata->current_position && 
            mdata->current_duration)
    { 
        result = ((float)(mdata->current_position / 
                mdata->current_duration)) * 
                PAC_SEEK_VALUE_MAX; 
    }
    return result;
}

void file_list_push_dirname(char *dirname, file_list *flist)
{
    int dirlen = strlen(dirname);
    char *write_ptr = flist->dirnames_string_loclist[flist->dirs_added];
#if _2PACWAV_LINUX
    if(dirname[dirlen] == '/')
#else
    if(dirname[dirlen] == '\\')
#endif
    { dirname[dirlen] = 0; }
    strncpy(write_ptr, dirname, PATH_MAX - 1);
    write_ptr[dirlen + 1] = 0x0;
    ++flist->dirs_added;
    flist->dirnames_string_loclist[flist->dirs_added] = write_ptr + dirlen + 1;
}

void add_single_file_to_music_list(char *path, music_data *mdata)
{
    char dir[PATH_MAX];
    char name[NAME_MAX];
    strncpy(dir, path, PATH_MAX - 1);
    separate_file_and_dir_name(dir, name);
    file_list *mlist = &mdata->music_list;

    char cont_dir_already_added = check_dir_already_added(path, &mdata->music_list);

    char *file_entry = mlist->filenames_string_loclist[mlist->entry_count];
    strncpy(file_entry, name, NAME_MAX - 1);
    int file_len = strlen(name);
    file_entry[file_len + 1] = 0x0;

    if(!cont_dir_already_added)
    { file_list_push_dirname(dir, mlist); }

    mlist->filenames_string_loclist[mlist->entry_count + 1] = file_entry + file_len + 1;
    ++mdata->music_list.entry_count;
}

void add_to_music_list(char *path, music_data *mdata, runtime_vars *rtvars)
{
    if(platform_file_exists(path))
    { add_single_file_to_music_list(path, mdata); }
    else if(platform_directory_exists(path))
    { 
        int pathlen = strlen(path);
        if(path[pathlen] == '/')
        { path[pathlen] = 0; }
        platform_get_directory_listing(path, &mdata->music_list); 
    }
    else 
    { 
        platform_log("%s: no such file or directory\n", path); 
        return;
    }
    set_match_flags((char *)rtvars->bufgroup_ptr->inbuf_search, mdata);
}

char *find_top_level_path4file(char *filename, file_list *flist)
{
    char *result = 0, *dirname;
    char pathbuf[PATH_MAX];
    for(uint32_t dir_index = 0; dir_index < flist->dirs_added; ++dir_index)
    {
        dirname = flist->dirnames_string_loclist[dir_index];
        snprintf(pathbuf, PATH_MAX - 1, "%s/%s", dirname, filename);
        if(platform_file_exists(pathbuf))
        { result = dirname; break; }
    }
    return result;
}

void file_list_play_file(char *selected_file, uint32_t file_index, music_data *mdata)
{
    char path_buf[PATH_MAX];
    char *toplevel_path = find_top_level_path4file(selected_file, &mdata->music_list);
    if(toplevel_path)
    {
        snprintf(path_buf, PATH_MAX - 1, "%s/%s", toplevel_path, selected_file);
        platform_dbg_log("trying to play %s\n", path_buf);
        strncpy(mdata->current_filename, path_buf, PATH_MAX - 1);
        mdata->music_list.current_index = file_index;
        load_file_from_path(path_buf, mdata);
    }
    else
    { platform_dbg_log("could not find containing directory for file %s.\n", selected_file); }
}

void goto_next_file(music_data *mdata)
{
    if(!mdata->sdlmixer_music || (mdata->music_list.entry_count < 1))
    { return; }

    file_list *mlist = &mdata->music_list;
    uint32_t cur_index = mlist->current_index;
    char *next_file;
    if(!mdata->shuffle_enabled)
    {
        if(mlist->current_index < (mlist->entry_count - 1))
        {
            next_file = mlist->filenames_string_loclist[cur_index + 1];
            file_list_play_file(next_file, cur_index + 1, mdata);
        }
    }
    else
    {
        uint32_t next_index = cur_index;
        if(mlist->entry_count != 2)
        {
            while(next_index == cur_index)
            { next_index = (uint32_t)(((double)(mlist->entry_count) - 1.0)*drand48()); }
        }
        else
        { next_index = !next_index; }
        next_file = mlist->filenames_string_loclist[next_index];
        file_list_play_file(next_file, next_index, mdata);
    }
}

void goto_prev_file(music_data *mdata)
{
    if(!mdata->sdlmixer_music || (mdata->music_list.current_index == 0))
    { return; }

    file_list *mlist = &mdata->music_list;
    uint32_t cur_index = mlist->current_index;
    char *prev_file = mlist->filenames_string_loclist[cur_index - 1];
    file_list_play_file(prev_file, cur_index -1, mdata);
}

void clear_file_list(music_data *mdata)
{
    if(!mdata->music_list.entry_count)
    { return; }

    file_list *mlist = &mdata->music_list;

    for(uint32_t clear_index = 1;
            clear_index < mlist->entry_count;
            ++clear_index)
    {
        mlist->filenames_string_loclist[clear_index] = 0;
        mlist->dirnames_string_loclist[clear_index] = 0;
    }

    memset(mlist->filenames_buf, 0, FILENAMES_BUFFER_SIZE);
    memset(mlist->dirnames_buf, 0, DIRNAMES_BUFFER_SIZE);
    memset(mlist->match_flags, 0, MATCH_FLAGS_BUFFER_SIZE);
    mlist->entry_count = 0;
    mlist->current_index = 0;
    mlist->dirs_added = 0;
    mlist->match_count = 0;
}

void menu_confirm_clear_file_list(runtime_vars *rtvars, 
                                widget_bounds_info *bound_info,
                                music_data *mdata)
{
    float confirm_width = 400.0f; 
    float confirm_height = (bound_info->height - bound_info->pad)*3;
    sdl_apidata *sdldata = rtvars->sdldata_ptr;
    struct nk_context *nkctx = rtvars->nuklear_ctx;
    struct nk_rect confirm_rect = nk_rect((sdldata->win_width/2.0f) - (confirm_width/2.0f),
                                        (sdldata->win_height/2.0f) - (confirm_height/2.0f),
                                        confirm_width,
                                        confirm_height);
    state_flags *dn_flags = &rtvars->sflags;

    if(nk_popup_begin(nkctx,
            NK_POPUP_STATIC, 
            "are you sure you want to clear the list?",
            NK_WINDOW_CLOSABLE, 
            confirm_rect))
    {
        nk_layout_row_dynamic(nkctx, bound_info->height - bound_info->pad*2, 2);
        if(nk_button_label(nkctx, "yes") ||
                pac_btn_press(SDL_SCANCODE_RETURN, &dn_flags->enter_wasdown, rtvars->kbd_state))
        { 
            dn_flags->clear_confirmation = 0;
            nk_popup_close(nkctx);
            clear_file_list(mdata);
        }
        if(nk_button_label(nkctx, "no") ||
               pac_btn_press(SDL_SCANCODE_ESCAPE, &dn_flags->escape_wasdown, rtvars->kbd_state))
        { 
            dn_flags->clear_confirmation = 0;
            nk_popup_close(nkctx); 
        }
        nk_popup_end(nkctx);
    } 
    else 
    { dn_flags->clear_confirmation = 0; }
}

//this is for testing
//TODO: clean this up and figure out how to extract the cover art and and render it
#if 0
typedef struct test_image_info
{
    struct nk_image nuk_image;
    uint8_t *data;
    int width;
    int height;
    int chan;
    GLuint ogl_tex_id;
} test_image_info;

static test_image_info global_imginfo;
#endif

void pac_init_bitmap(bitmap_info *bmpinfo, runtime_vars *rtvars)
{
    char path[PATH_MAX];
    snprintf(path, PATH_MAX - 1, "%s/test_scrat.jpg", rtvars->resource_directory);

    bmpinfo->data = stbi_load(path,
                            &bmpinfo->width, 
                            &bmpinfo->height, 
                            &bmpinfo->chan, 
                            4);
    //printf("[image info]:\nw=%d, h=%d, c=%d\n", 
    //        global_imginfo.width, global_imginfo.height, global_imginfo.chan);
    glGenTextures(1, &bmpinfo->ogl_tex_id);
    glBindTexture(GL_TEXTURE_2D, bmpinfo->ogl_tex_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 
            0, 
            GL_RGBA,
            bmpinfo->width,
            bmpinfo->height,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            bmpinfo->data);

    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(bmpinfo->data);
    bmpinfo->nuk_image = nk_image_id((int)bmpinfo->ogl_tex_id);
}

void pac_nk_draw_bitmap(runtime_vars *rtvars, bitmap_info *bmpinfo)
{
    nk_layout_row_static(rtvars->nuklear_ctx, 
            bmpinfo->width, 
            bmpinfo->height, 
            1);
    nk_image(rtvars->nuklear_ctx, bmpinfo->nuk_image);
}

void menu_do_current_file_info(runtime_vars *rtvars, 
                            music_data *mdata, 
                            general_buffer_group *bufgroup,
                            widget_bounds_info *bound_info)
{
    struct nk_context *nkctx = rtvars->nuklear_ctx;
    char *infobuf = (char *)bufgroup->info_buffer;
    char *path_begin = strchr(infobuf, '\n');
    *(path_begin++) = 0;
    char **title_ptr = &mdata->current_metadata.title_begin_in_buf;
    char **artist_ptr = &mdata->current_metadata.artist_begin_in_buf;
    char **album_ptr = &mdata->current_metadata.album_begin_in_buf;
#if 0 //not checking this for now
    if(mdata->metadata_should_update)
#else
#endif
    //this stuff is kinda spaghetti honestly
    if(sdlmixer_get_taginfo(mdata))
    {
        *title_ptr = path_begin + strlen(path_begin);
        *((*title_ptr)++) = 0;
        snprintf(*title_ptr, 1023, "title: %s\nartist: %s\nalbum: %s\000",
                mdata->current_metadata.tag_title,
                mdata->current_metadata.tag_artist,
                mdata->current_metadata.tag_album);
        *artist_ptr = strchr(*title_ptr, '\n');
        *((*artist_ptr)++) = 0;
        *album_ptr = strchr(*artist_ptr, '\n');
        *((*album_ptr)++) = 0;
    }

    int text_width = bound_info->width - (bound_info->height + bound_info->pad) - 30;

    nk_layout_space_push(nkctx, 
                        nk_rect(bound_info->height + bound_info->pad, 
                        bound_info->y_offset, 
                        bound_info->width - (bound_info->height + bound_info->pad), 
                        bound_info->content_bounds.h - bound_info->height*3));
    nk_group_begin(nkctx, "music", NK_WINDOW_BORDER);

    nk_layout_row_static(nkctx, 20, text_width, 1);
    nk_label(nkctx, (char *)bufgroup->info_buffer, NK_TEXT_ALIGN_LEFT|NK_TEXT_ALIGN_BOTTOM);
    nk_layout_row_static(nkctx, 20, text_width, 1);
    nk_label(nkctx, path_begin, NK_TEXT_ALIGN_LEFT|NK_TEXT_ALIGN_BOTTOM);

    if(*title_ptr && *artist_ptr && *album_ptr) //checks after title_ptr are probably redundant
    {
        nk_style_set_font(nkctx, &rtvars->big_font->handle);
        nk_layout_row_static(nkctx, 100, text_width, 1);
        nk_label(nkctx, *title_ptr, NK_TEXT_ALIGN_LEFT|NK_TEXT_ALIGN_BOTTOM);
        nk_layout_row_static(nkctx, 20, text_width, 1);
        nk_label(nkctx, *artist_ptr, NK_TEXT_ALIGN_LEFT|NK_TEXT_ALIGN_BOTTOM);
        nk_layout_row_static(nkctx, 20, text_width, 1);
        nk_label(nkctx, *album_ptr, NK_TEXT_ALIGN_LEFT|NK_TEXT_ALIGN_BOTTOM);
        nk_style_set_font(nkctx, &rtvars->small_font->handle);
    }

#if 1
    pac_nk_draw_bitmap(rtvars, &mdata->cover);
#endif

    nk_group_end(nkctx);
}

void menu_do_music_list(runtime_vars *rtvars, 
                    music_data *mdata, 
                    widget_bounds_info *bound_info)
{
    struct nk_context *nkctx = rtvars->nuklear_ctx;
    nk_layout_space_push(nkctx, 
                        nk_rect(bound_info->height + bound_info->pad, 
                        bound_info->y_offset, 
                        bound_info->width - (bound_info->height + bound_info->pad), 
                        bound_info->content_bounds.h - bound_info->height*3));
    nk_list_view_begin(nkctx, 
                    &mdata->music_list_view, 
                    "music", 
                    NK_WINDOW_BORDER, 
                    bound_info->height - bound_info->y_alignment, 
                    mdata->music_list.match_count);
    nkctx->style.button.text_alignment = NK_TEXT_LEFT;
    nk_layout_row_dynamic(nkctx, bound_info->height - bound_info->y_alignment, 1);

    //i think this works now but idk
    uint32_t render_index = 0, file_index = 0;
    for(int loop_index = 0;
            loop_index < mdata->music_list_view.count;
            ++loop_index)
    {
        render_index = file_index++ + mdata->music_list_view.begin;
        if(mdata->music_list.match_flags[render_index])
        { --loop_index; continue; }
        
        char *btntext = mdata->music_list.filenames_string_loclist[render_index];
        if(nk_button_label(nkctx, btntext))
        { file_list_play_file(btntext, render_index, mdata); }
    }

    nk_list_view_end(&mdata->music_list_view);
    nkctx->style.button.text_alignment = NK_TEXT_CENTERED;
}

//deprecated
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
    struct nk_color debug_col = nk_rgba(0xCC, 0xCC, 0x00, 0xFF);
    nk_text_colored(rtvars->nuklear_ctx, 
                    (char *)bufgroup->info_buffer, 
                    strlen((char *)bufgroup->info_buffer), 
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

void str2lowercase(char *string, int len) 
{
    if(!string)
    { return; }
    for(int i = 0; i < len; ++i)
    { string[i] = tolower(string[i]); }
}

char *pac_strcasestr(char *str, char *substr)
{
    if(!str || !substr)
    { return 0; }
    char lower_str[NAME_MAX], lower_substr[NAME_MAX];
    strncpy(lower_str, str, NAME_MAX);
    strncpy(lower_substr, substr, NAME_MAX);
    str2lowercase(lower_str, strlen(lower_str));
    str2lowercase(lower_substr, strlen(lower_substr));
    char *result = strstr(lower_str, lower_substr);
    return result;
}

void set_match_flags(char *searchbuf, music_data *mdata)
{
    file_list *mlist = &mdata->music_list;
    mlist->match_count = 0;

    if(searchbuf[0])
    {
        char *string;
        for(uint32_t file_index = 0;
                file_index < mlist->entry_count;
                ++file_index)
        {
            string = mlist->filenames_string_loclist[file_index];
#if 0
            if(strstr(string, searchbuf))
#else
            if(pac_strcasestr(string, searchbuf))
#endif
            {
                mlist->match_flags[file_index] = 0;
                ++mlist->match_count;
            }
            else
            { mlist->match_flags[file_index] = 1; }
        }
    }
    else
    {
        memset(mlist->match_flags, 0, mlist->entry_count);
        mlist->match_count = mlist->entry_count;
    }
}

void menu_do_search(runtime_vars *rtvars, 
                general_buffer_group *bufgroup,
                music_data *mdata,
                widget_bounds_info *bound_info, 
                float add_width)
{
    static char f_wasdown = 0;
    static int searchbuf_length = 0;
    int new_searchbuf_length;
    char got_input = 0;
    char *searchbuf = (char *)bufgroup->inbuf_search;

    nk_layout_space_push(rtvars->nuklear_ctx, 
                        nk_rect((bound_info->width - add_width*4),
                        bound_info->y_offset, 
                        add_width,
                        bound_info->height - (bound_info->y_alignment + bound_info->pad)));
    nk_label(rtvars->nuklear_ctx, "search:", NK_TEXT_CENTERED);
    nk_layout_space_push(rtvars->nuklear_ctx, 
                        nk_rect((bound_info->width - add_width*3),
                        bound_info->y_offset, 
                        add_width*3,
                        bound_info->height - (bound_info->y_alignment + bound_info->pad)));

    if(rtvars->kbd_state[SDL_SCANCODE_LCTRL] &&
            pac_btn_press(SDL_SCANCODE_F, &f_wasdown, rtvars->kbd_state))
    { nk_edit_focus(rtvars->nuklear_ctx, NK_TEXT_EDIT_MODE_INSERT); }

    nk_flags field_outflags = nk_edit_string_zero_terminated(rtvars->nuklear_ctx, 
                                    NK_EDIT_FIELD|NK_EDIT_GOTO_END_ON_ACTIVATE,
                                    (char *)bufgroup->inbuf_search, 
                                    SEARCH_BUFFER_SIZE - 1,
                                    nk_filter_default);
    bound_info->y_offset += bound_info->height - bound_info->y_alignment;

    if(field_outflags & NK_EDIT_ACTIVE)
    {
        new_searchbuf_length = strlen(searchbuf);
        got_input = (searchbuf_length != new_searchbuf_length);
        searchbuf_length = new_searchbuf_length;
    }

    if(got_input)
    { set_match_flags(searchbuf, mdata); }
}

void menu_do_list_control(runtime_vars *rtvars,
                        music_data *mdata,
                        widget_bounds_info *bound_info,
                        float add_width)
{
    struct nk_context *nkctx = rtvars->nuklear_ctx;
    state_flags *dn_flags = &rtvars->sflags;

    nk_layout_space_push(nkctx, 
                        nk_rect(bound_info->height + bound_info->pad, 
                        bound_info->y_offset, 
                        add_width, 
                        bound_info->height - (bound_info->y_alignment + bound_info->pad)));
    if(nk_button_label(nkctx, "sort (a-z)") ||
            ((rtvars->kbd_state[SDL_SCANCODE_LSHIFT] &&
            rtvars->kbd_state[SDL_SCANCODE_LCTRL]) &&
            pac_btn_press(SDL_SCANCODE_S, &dn_flags->s_wasdown, rtvars->kbd_state)))
    {
        if(mdata->music_list.entry_count)
        { sort_file_list_alpha(&mdata->music_list); }
    }

    nk_layout_space_push(nkctx, 
                        nk_rect(bound_info->height + (bound_info->pad*2) + add_width,
                        bound_info->y_offset, 
                        add_width, 
                        bound_info->height - (bound_info->y_alignment + bound_info->pad)));

    if(nk_button_label(nkctx, "clear list") ||
            ((rtvars->kbd_state[SDL_SCANCODE_LSHIFT] && 
            rtvars->kbd_state[SDL_SCANCODE_LCTRL]) &&
            pac_btn_press(SDL_SCANCODE_X, &dn_flags->x_wasdown, rtvars->kbd_state))) 
    { dn_flags->clear_confirmation = !dn_flags->clear_confirmation; }

    if(dn_flags->clear_confirmation)
    { menu_confirm_clear_file_list(rtvars, bound_info, mdata); }
}

void menu_do_volume_bar(runtime_vars *rtvars,
                    music_data *mdata,
                    widget_bounds_info *bound_info,
                    float vol_width)
{
    struct nk_context *nkctx = rtvars->nuklear_ctx; 
    nk_layout_space_push(nkctx, 
                        nk_rect(bound_info->x_offset, 
                        bound_info->content_bounds.h - bound_info->height - bound_info->pad, 
                        vol_width, bound_info->height));
    bound_info->x_offset += (bound_info->height + (vol_width - bound_info->y_alignment));

    if(nk_slider_int(nkctx, 0, &mdata->volume, MIX_MAX_VOLUME, 1))
    { Mix_VolumeMusic(mdata->volume); }
    if(rtvars->kbd_state[SDL_SCANCODE_LCTRL])
    {
        if(rtvars->kbd_state[SDL_SCANCODE_UP])
        { 
            if((mdata->volume + 2) < MIX_MAX_VOLUME)
            { mdata->volume += 2; }
            else
            { mdata->volume = MIX_MAX_VOLUME; }
        }
        if(rtvars->kbd_state[SDL_SCANCODE_DOWN])
        { 
            if((mdata->volume - 2) > 0)
            { mdata->volume -= 2; }
            else
            { mdata->volume = 0; }
        }
    }
}

void menu_do_seek_bar(runtime_vars *rtvars,
                    music_data *mdata,
                    widget_bounds_info *bound_info)
{
    struct nk_context *nkctx = rtvars->nuklear_ctx; 
    mdata->seek_value = conv_songpos2slide_value(mdata);
    nk_layout_space_push(nkctx, 
                        nk_rect(bound_info->x_offset, 
                        bound_info->content_bounds.h - bound_info->height - bound_info->pad, 
                        bound_info->width - bound_info->x_offset - bound_info->pad, 
                        bound_info->height));
    if(nk_slider_float(nkctx, 0.0f, &mdata->seek_value, PAC_SEEK_VALUE_MAX, 1.0f))
    {
        if(mdata->sdlmixer_music)
        { Mix_SetMusicPosition(conv_slide_value2songpos(mdata)); }
    }
}

void pac_main_loop(runtime_vars *rtvars, 
                sdl_apidata *sdldata, 
                general_buffer_group *bufgroup,
                music_data *mdata)
{
    static char playback_btn_text[4] = {};
    static char shuffle_btn_text[4] = {};

    state_flags *dn_flags = &rtvars->sflags;
    struct nk_context *nkctx = rtvars->nuklear_ctx;

    if(mdata->sdlmixer_music && (Mix_PlayingMusic() || Mix_PausedMusic()))
    { update_music_info(mdata); }
    char *second_debug_buf = update_info_buffer(mdata, bufgroup);

    pac_begin_frame(rtvars, sdldata);

    ////////////////////////////////

    nk_layout_space_begin(nkctx, NK_STATIC, 0, INT_MAX);
    widget_bounds_info bound_info = {};
    query_bounds_info(nkctx, &bound_info);
    float add_width = 100.0f;
    float vol_width = 180.0f;

#if 0
    menu_show_debuginfo(rtvars, bufgroup, &bound_info, second_debug_buf);
#endif

    nk_layout_space_push(nkctx, 
                        nk_rect(0, 
                        bound_info.y_offset, 
                        bound_info.width - add_width - bound_info.pad, 
                        bound_info.height - bound_info.y_alignment));

    if(rtvars->kbd_state[SDL_SCANCODE_LALT])
    {
        if(pac_btn_press(SDL_SCANCODE_D, &dn_flags->d_wasdown, rtvars->kbd_state))
        { nk_edit_focus(nkctx, NK_TEXT_EDIT_MODE_INSERT); }
    }

    nk_flags file_field_outflags = nk_edit_string_zero_terminated(nkctx, 
                                        NK_EDIT_FIELD|NK_EDIT_GOTO_END_ON_ACTIVATE, 
                                        (char *)bufgroup->inbuf_filename, 
                                        PATH_MAX - 1, 
                                        nk_filter_default);

    nk_layout_space_push(nkctx, 
                        nk_rect(bound_info.width - add_width, 
                        bound_info.y_offset, 
                        add_width, 
                        bound_info.height - bound_info.y_alignment));
    bound_info.y_offset += bound_info.height - 3.0f; //i just made this up

    if(nk_button_label(nkctx, "add") ||
            ((file_field_outflags & NK_EDIT_ACTIVE) &&
            pac_btn_press(SDL_SCANCODE_RETURN, &dn_flags->enter_wasdown, rtvars->kbd_state)))
    { add_to_music_list((char *)bufgroup->inbuf_filename, mdata, rtvars); }

    menu_do_list_control(rtvars, mdata, &bound_info, add_width);

    menu_do_search(rtvars, bufgroup, mdata, &bound_info, add_width);

    if(rtvars->kbd_state[SDL_SCANCODE_LCTRL] &&
            pac_btn_press(SDL_SCANCODE_L, &rtvars->sflags.l_wasdown, rtvars->kbd_state))
    { 
        mdata->metadata_should_update = 1;
        cycle_center_view_state(&rtvars->sflags.viewstate); 
    }

    //i think something with the scroll bar or something in nuklear
    //is busted since it crashes if the window is too small
    //stops happening with this check, amazingly (might be my fault as well idk)
    if((sdldata->win_height > MLIST_MIN_WIN_WIDTH) && 
            (sdldata->win_width > MLIST_MIN_WIN_HEIGHT))
    {
        if(rtvars->sflags.viewstate == CENTER_VIEW_STATE_MUSIC_LIST)
        { menu_do_music_list(rtvars, mdata, &bound_info); }
        else if(rtvars->sflags.viewstate == CENTER_VIEW_STATE_CURRENT_INFO)
        { menu_do_current_file_info(rtvars, mdata, bufgroup, &bound_info); }
    }

    nk_layout_space_push(nkctx, 
                        nk_rect(bound_info.x_offset, 
                        bound_info.content_bounds.h - (bound_info.height + bound_info.pad)*5, 
                        bound_info.height, 
                        bound_info.height));

    if(mdata->shuffle_enabled)
    { shuffle_btn_text[0] = '-'; }
    else 
    { shuffle_btn_text[0] = '~'; }
    if(nk_button_label(nkctx, shuffle_btn_text)) 
    { mdata->shuffle_enabled = !mdata->shuffle_enabled; }

    nk_layout_space_push(nkctx, 
                        nk_rect(bound_info.x_offset, 
                        bound_info.content_bounds.h - (bound_info.height + bound_info.pad)*4, 
                        bound_info.height, 
                        bound_info.height));
    if(nk_button_label(nkctx, ">>") ||
            (rtvars->kbd_state[SDL_SCANCODE_LCTRL] &&
            pac_btn_press(SDL_SCANCODE_RIGHT, &rtvars->sflags.right_wasdown, rtvars->kbd_state)))
    { 
        goto_next_file(mdata); 
    }

    nk_layout_space_push(nkctx, 
                        nk_rect(bound_info.x_offset, 
                        bound_info.content_bounds.h - (bound_info.height + bound_info.pad)*3, 
                        bound_info.height, 
                        bound_info.height));
    if(nk_button_label(nkctx, "<<") ||
            (rtvars->kbd_state[SDL_SCANCODE_LCTRL] &&
            pac_btn_press(SDL_SCANCODE_LEFT, &rtvars->sflags.left_wasdown, rtvars->kbd_state)))
    {
        goto_prev_file(mdata);
    }

    nk_layout_space_push(nkctx, 
                        nk_rect(bound_info.x_offset, 
                        bound_info.content_bounds.h - (bound_info.height + bound_info.pad)*2, 
                        bound_info.height, 
                        bound_info.height));
    //NOTE: some fonts dont have a codepoint for this glyph
#if 1
    if(nk_button_label(nkctx, (char *)_stop_btn_glyph)) //stop
#else
    if(nk_button_label(nkctx, "[]")) //stop
#endif
    { 
        sdlmixer_stop_music(mdata); 
        mdata->current_filename[0] = 0x0;
    }

    nk_layout_space_push(nkctx, 
                        nk_rect(bound_info.x_offset, 
                        bound_info.content_bounds.h - (bound_info.height + bound_info.pad), 
                        bound_info.height, 
                        bound_info.height));
    bound_info.x_offset += (bound_info.height + bound_info.pad);

    if(mdata->paused) 
    { playback_btn_text[0] = '>'; playback_btn_text[1] = 0; } 
    else 
    { playback_btn_text[0] = '|'; playback_btn_text[1] = '|'; }

    if(nk_button_label(nkctx, playback_btn_text) || 
            pac_btn_press(SDL_SCANCODE_SPACE, &dn_flags->space_wasdown, rtvars->kbd_state))
    {
        if(!mdata->paused && mdata->sdlmixer_music) 
        { mdata->paused = 1;  Mix_PauseMusic(); } 
        else if(mdata->paused && mdata->sdlmixer_music) 
        { mdata->paused = 0;  Mix_ResumeMusic(); }
    }

    menu_do_volume_bar(rtvars, mdata, &bound_info, vol_width);
    menu_do_seek_bar(rtvars, mdata, &bound_info);

    ////////////////////////////////

    mdata->metadata_should_update = 0;

    nk_layout_space_end(nkctx);
    pac_end_frame(rtvars, sdldata);
}

char sdlmixer_get_taginfo(music_data *mdata)
{
    if(!mdata || !mdata->sdlmixer_music)
    { return 0; }

    audio_metadata_group *amg = &mdata->current_metadata;
    amg->tag_title =    Mix_GetMusicTitleTag(mdata->sdlmixer_music);
    amg->tag_artist =   Mix_GetMusicArtistTag(mdata->sdlmixer_music);
    amg->tag_album =    Mix_GetMusicAlbumTag(mdata->sdlmixer_music);
    return 1;
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
    mdata->metadata_should_update = 1;
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
    mdata->metadata_should_update = 1;
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

        char wintitle[128];
        get_version_string(wintitle);
        sdldata->window_ptr = SDL_CreateWindow(wintitle,
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
    color_tbl[NK_COLOR_TEXT] = nk_rgba(0xDD, 0xDD, 0xDD, 0xFF);
    color_tbl[NK_COLOR_WINDOW] = nk_rgba(0, 0, 0, 0xFF);
    color_tbl[NK_COLOR_HEADER] = nk_rgba(51, 51, 56, 220);
    color_tbl[NK_COLOR_BORDER] = nk_rgba(0xAA, 0xAA, 0xAA, 0xFF);
    color_tbl[NK_COLOR_BUTTON] = nk_rgba(0x15, 0x15, 0x15, 0xFF);
    color_tbl[NK_COLOR_BUTTON_HOVER] = nk_rgba(0x33, 0x33, 0x55, 0xFF);
    color_tbl[NK_COLOR_BUTTON_ACTIVE] = nk_rgba(63, 98, 126, 0xFF);
    color_tbl[NK_COLOR_TOGGLE] = nk_rgba(50, 58, 61, 0xFF);
    color_tbl[NK_COLOR_TOGGLE_HOVER] = nk_rgba(45, 53, 56, 0xFF);
    color_tbl[NK_COLOR_TOGGLE_CURSOR] = nk_rgba(48, 83, 111, 0xFF);
    color_tbl[NK_COLOR_SELECT] = nk_rgba(57, 67, 61, 0xFF);
    color_tbl[NK_COLOR_SELECT_ACTIVE] = nk_rgba(48, 83, 111, 0xFF);
    color_tbl[NK_COLOR_SLIDER] = nk_rgba(50, 58, 61, 0xFF);
    color_tbl[NK_COLOR_SLIDER_CURSOR] = nk_rgba(0xAA, 0xAA, 0xAA, 245);
    color_tbl[NK_COLOR_SLIDER_CURSOR_HOVER] = nk_rgba(53, 88, 116, 0xFF);
    color_tbl[NK_COLOR_SLIDER_CURSOR_ACTIVE] = nk_rgba(58, 93, 121, 0xFF);
    color_tbl[NK_COLOR_PROPERTY] = nk_rgba(50, 58, 61, 0xFF);
    color_tbl[NK_COLOR_EDIT] = nk_rgba(0x15, 0x15, 0x15, 0xFF);
    color_tbl[NK_COLOR_EDIT_CURSOR] = nk_rgba(210, 210, 210, 0xFF);
    color_tbl[NK_COLOR_COMBO] = nk_rgba(50, 58, 61, 0xFF);
    color_tbl[NK_COLOR_CHART] = nk_rgba(50, 58, 61, 0xFF);
    color_tbl[NK_COLOR_CHART_COLOR] = nk_rgba(48, 83, 111, 0xFF);
    color_tbl[NK_COLOR_CHART_COLOR_HIGHLIGHT] = nk_rgba(0xFF, 0, 0, 0xFF);
    color_tbl[NK_COLOR_SCROLLBAR] = nk_rgba(50, 58, 61, 0xFF);
    color_tbl[NK_COLOR_SCROLLBAR_CURSOR] = nk_rgba(48, 83, 111, 0xFF);
    color_tbl[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = nk_rgba(53, 88, 116, 0xFF);
    color_tbl[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgba(58, 93, 121, 0xFF);
    color_tbl[NK_COLOR_TAB_HEADER] = nk_rgba(48, 83, 111, 0xFF);
    color_tbl[NK_COLOR_KNOB] = color_tbl[NK_COLOR_SLIDER];
    color_tbl[NK_COLOR_KNOB_CURSOR] = color_tbl[NK_COLOR_SLIDER_CURSOR];
    color_tbl[NK_COLOR_KNOB_CURSOR_HOVER] = color_tbl[NK_COLOR_SLIDER_CURSOR_HOVER];
    color_tbl[NK_COLOR_KNOB_CURSOR_ACTIVE] = color_tbl[NK_COLOR_SLIDER_CURSOR_ACTIVE];
    nk_style_from_table(ctx, color_tbl);
}
