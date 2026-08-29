
/*
File: 2pacwav2.cpp
Date: Thu 24 Apr 2025 04:24:08 PM EEST
*/

#include <stdio.h>
#include <limits.h>
#include <stdint.h>

#include "SDL.h"
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glext.h>

#define STBIMAGE_ENABLED 1
#if STBIMAGE_ENABLED
    #define STB_IMAGE_IMPLEMENTATION 1
    #define STBI_FAILURE_USERMSG 1
    #include "stb/stb_image.h"
#endif

#include "2pacwav2.h"

#if _2PACWAV_LINUX
    #include "linux_2pacwav2.h"
#elif _2PACWAV_WIN32
#endif

#include "2pacwav2_visualizer.cpp"
#include "2pacwav2_config.cpp"
#include "2pacwav2_playlist.cpp"
#include "2pacwav2_sort.cpp"

//#include "2pacmixer.h"

static void pac_nop() { return; }

static void get_version_string(char *buffer)
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

static void show_version()
{
    char verbuf[128];
    get_version_string(verbuf);
    int len = strlen(verbuf);
    char *ver_end = verbuf + len;
    sprintf(ver_end, ", compiled %s @ %s", __DATE__, __TIME__);
    platform_log("%s\n", verbuf);
}

static void show_help(char longhelp)
{
    show_version();
    platform_log("usage: 2w [options] [files]\n"
            "-- COMMAND LINE OPTIONS --\n"
            "-h | --help : print this message and exit\n"
            "--longhelp : print output of the above option as well as additional documentation\n"
            "-v | --version : print version and exit\n"
            "-vol <value> : set the volume\n"
            "-conf <path> : specify config file\n"
            "-noconf : do not look for a configuration file\n"
            "-ns : if the configuration file has instances of startup_path, skip adding them to the playlist on startup\n"
            "-font_size <value> : set point size for font\n");

    if (!longhelp) { return; }

    platform_log(
R"(
/*
-- 2PACWAV CONFIGURATION FILE --

The configuration file named "2wconf" should be placed either in $HOME/.config/2pacwav/
or in the same directory as the executable. The syntax of the configuration file is sort of
similar to C, meaning that '//' and /* */ are used for comments and lines end in semicolons.
Below is a complete list of variables that can be set and some information about them.
NOTE: you can copy and paste this output in its entirety to your configuration file.
Everything here is inside a comment block, so variables you actually intend to set have to be moved outside it.
---

startup_path = "/path/to/file"; //Sets a path to a file/folder/playlist that will be added to the list on startup.
                                //Note that multiple instances of this are perfectly valid.
font_path = "/path/to/font/file"; //Path to the desired font file (TrueType or OpenType)
font_size = value; //Sets point size for the font. (default: %g)
visualizer = 0 or 1; //Disables visualizer if value is 0 and enables it otherwise,
                                  //including when this variable isn't set.
                                  //Note that this can be re-enabled from the view menu at any time.
visualizer_color = {r, g, b, a}; //Sets the color that the visualizer is set to at startup.
                                 //Values inside {} must be floats between 0.0 and 1.0
                                 //Note: the visualizer color can be changed at any time from the settings menu.
volume = value; //Sets the volume level on startup. The value is clamped to 0-128
                //The -vol command option overrides this
volume_step = value; //Sets the amount by which volume level will be incremented or decremented by
                     //when using the keybinds to change it (0 and 9)
text_color = {r, g, b, a}; //Sets the text color. (floats between 0.0 and 1.0)
button_bg_color = {r, g, b, a}; //Sets the background color for buttons and text fields (floats between 0.0 and 1.0)
*/
)"
                , PAC_LATIN_FONTSIZE);
}

static void startup_push_path(Startup_Args *sargs, char *path)
{
    Startup_Args_Paths *p = &sargs->paths;
    if (p->count < PAC_MAX_DIRS)
    {
        int index = p->count;
        char *dest = p->ptrs[index];
        if (index)
        {
            dest = p->ptrs[index - 1];
            dest += strlen(dest);
            *dest++ = 0;
            p->ptrs[index] = dest; 
        }
        snprintf(dest, PATH_MAX, "%s", path);
        ++p->count; 
    }
}

static char pac_do_command_args(int arg_count, 
                                char **args, 
                                Startup_Args *sargs,
                                General_Buffer_Group *bufgroup)
{
    sargs->volume = -1;
    char *arg;
    char exit_after_ret = 0;
    for (int arg_index = 1; 
        arg_index < arg_count; 
        ++arg_index)
    {
        arg = args[arg_index];
        if (!strcmp("--", arg))
        { 
            break; 
        }
        else if (!strcmp("-v", arg) || !strcmp("--version", arg))
        {
            exit_after_ret = 1; 
            show_version(); 
            break; 
        }
        else if (!strcmp("-h", arg) || !strcmp("--help", arg))
        {
            exit_after_ret = 1;
            show_help(0);
            break;
        }
        else if (!strcmp("--longhelp", arg))
        {
            exit_after_ret = 1;
            show_help(1);
            break;
        }
        else if (!sargs->no_load_conf && !strcmp("-noconf", arg))
        {
            sargs->no_load_conf = 1;
        }
        else if (!sargs->no_load_startup_paths && !strcmp("-ns", arg))
        {
            sargs->no_load_startup_paths = 1;
        }
        else if ((sargs->font_size == PAC_LATIN_FONTSIZE) &&
            !strcmp("-font_size", arg))
        {
            if (args[arg_index + 1])
            {
                float value = strtof(args[arg_index + 1], 0);
                if (value != 0.0f)
                {
                    sargs->font_size = value;
                }
                else
                {
                    fprintf(stderr, "invalid argument given after %s, ignoring.\n", arg);
                }
            }
            else
            {
                fprintf(stderr, "no argument given after %s, ignoring.\n", arg);
            }
            ++arg_index;
        }
        else if ((-1 == sargs->volume) && !strcmp("-vol", arg))
        {
            if (args[arg_index + 1])
            {
                errno = 0;
                int value = strtol(args[arg_index + 1], 0, 10);
                if (errno != ERANGE)
                {
                    sargs->volume = value;
                }
                else
                {
                    fprintf(stderr, "invalid argument given after %s, ignoring.\n", arg);
                }
                ++arg_index;
            }
            else
            {
                fprintf(stderr, "no argument given after %s, ignoring.\n", arg);
            }
        }
        else if (!sargs->conf_path && !strcmp("-conf", arg))
        {
            if (args[arg_index + 1])
            { sargs->conf_path = args[arg_index + 1]; }
            else
            { fprintf(stderr, "no argument given after %s, ignoring.\n", arg); }
            ++arg_index;
        }
        else
        {
            if (platform_path_exists(arg))
            { startup_push_path(sargs, arg); }
            else
            {
                platform_log("unrecognized option: %s\n", arg);
                exit_after_ret = 1;
            }
        }
    }

    return exit_after_ret;
}

static void startup_add_paths(Startup_Args *sargs,
                            Runtime_Vars *rtvars, 
                            Music_Data *mdata)
{
    char *this_path, *this_end;
    for (int i = 0; i < sargs->paths.count; ++i)
    {
        this_path = sargs->paths.ptrs[i];
        add_to_music_list(this_path, mdata, rtvars); 
    }
}

static char pac_mousebtn_press(Mouse_State *mouse)
{
    char result = 0;
    if (mouse->down)
    {
        if (!mouse->wasdown_flags[mouse->down - 1])
        {
            mouse->wasdown_flags[mouse->down - 1] = 1;
            result = 1; 
        }
    }
    else
    { 
        *(int32_t *)mouse->wasdown_flags = 0; 
    }
    return result;
}

#if _2PACWAV_DEPRECATED
static char pac_btn_press(SDL_Scancode scan, 
                        char *wasdown, 
                        const uint8_t *kbd_state)
{
    char state = 0;
    if (kbd_state[scan])
    {
        if (!*wasdown)
        {
            state = 1; 
            *wasdown = 1; 
        }
    }
    else
    { 
        *wasdown = 0; 
    }
    return state;
}
#endif

static char pac_imgui_load_font(char *font_path,
                                float font_size,
                                Runtime_Vars *rtvars)
{
    char status = 0;
    char *latin_path = font_path, cjk_path[PATH_MAX];
#if 0
    snprintf(latin_path, PATH_MAX - 1, "%s/%s", 
            rtvars->resource_directory, font_name);
#endif
    snprintf(cjk_path, PATH_MAX - 1, "%s/%s", 
            rtvars->resource_directory, PAC_CJK_FONT_STRING);
    PAC_LOCAL_STATIC ImVector<ImWchar> latin_ranges_buffer;
    PAC_LOCAL_STATIC ImVector<ImWchar> cjk_ranges_buffer;

    if (platform_file_exists(latin_path) && 
        platform_file_exists(cjk_path))
    {
        ImGuiIO &io = ImGui::GetIO();
        ImFontConfig latin_conf;
        latin_conf.MergeMode = 0;
        ImFontGlyphRangesBuilder ranges_builder;
        ImWchar extra_range[] = { 0x25A0, 0x25A1, 0 }; //stop button glyph
        ranges_builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
        ranges_builder.AddRanges(io.Fonts->GetGlyphRangesCyrillic());
        ranges_builder.AddRanges(io.Fonts->GetGlyphRangesGreek());
        ranges_builder.AddRanges(extra_range);
        ranges_builder.BuildRanges(&latin_ranges_buffer);
        io.Fonts->AddFontFromFileTTF(latin_path,
                                    font_size, 
                                    &latin_conf, 
                                    latin_ranges_buffer.Data);

        ImFontConfig cjk_conf;
        float cjk_font_size = font_size + 1.0f;
        cjk_conf.MergeMode = true;
        cjk_conf.PixelSnapH = true;
        ranges_builder.AddRanges(io.Fonts->GetGlyphRangesJapanese());
        ranges_builder.AddRanges(io.Fonts->GetGlyphRangesKorean());
        ranges_builder.AddRanges(io.Fonts->GetGlyphRangesThai());
        ranges_builder.AddRanges(io.Fonts->GetGlyphRangesVietnamese());
        ranges_builder.AddRanges(io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
        ranges_builder.BuildRanges(&cjk_ranges_buffer);
        rtvars->main_font = io.Fonts->AddFontFromFileTTF(cjk_path,
                                                    cjk_font_size, 
                                                    &cjk_conf, 
                                                    cjk_ranges_buffer.Data);

        status = 1;
    }

    return status;
}

static void sdlapi_process_events(Runtime_Vars *rtvars, Sdl_Apidata *sdldata)
{
    SDL_Event event;
    rtvars->kbd_state = SDL_GetKeyboardState(0);
    char char_was_pressed = 0;
    Mouse_State *mouse = &rtvars->sflags.mouse;

    while (SDL_PollEvent(&event))
    {
        ImGui_ImplSDL2_ProcessEvent(&event);
        switch (event.type)
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

        case SDL_MOUSEBUTTONDOWN:
        {
            mouse->down = event.button.button;
            mouse->pos.x = event.button.x;
            mouse->pos.y = event.button.y;
        } break;

        case SDL_KEYDOWN: break;

        default:
        {
            mouse->down = 0;
        } break;
        }
    }
}

static void sdlapi_correct_gl_viewport_and_clear(Sdl_Apidata *sdldata)
{
    SDL_GetWindowSize(sdldata->window_ptr, &sdldata->win_width, &sdldata->win_height);
    glViewport(0, 0, sdldata->win_width, sdldata->win_height);
    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(0, 0, 0, 1.0f);
}

static void pac_begin_frame(Runtime_Vars *rtvars, Sdl_Apidata *sdldata)
{
    sdlapi_correct_gl_viewport_and_clear(sdldata);
    sdlapi_process_events(rtvars, sdldata);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();


    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::SetNextWindowBgAlpha(0);

    ImGui::Begin("2PACWAV", 0, ImGuiWindowFlags_NoTitleBar
                            |ImGuiWindowFlags_NoResize
                            |ImGuiWindowFlags_NoMove
                            |ImGuiWindowFlags_NoScrollbar
                            |ImGuiWindowFlags_NoSavedSettings
                            |ImGuiWindowFlags_NoDecoration
                            |ImGuiWindowFlags_MenuBar);

    Ui_Vars *uiv = &rtvars->uivars;
    int button_color = IM_COL32((unsigned char)((float)0xFF*uiv->button_bg_color[0]),
                                (unsigned char)((float)0xFF*uiv->button_bg_color[1]),
                                (unsigned char)((float)0xFF*uiv->button_bg_color[2]),
                                (unsigned char)((float)0xFF*uiv->button_bg_color[3]));
    int text_color = IM_COL32((unsigned char)((float)0xFF*uiv->text_color[0]),
                              (unsigned char)((float)0xFF*uiv->text_color[1]),
                              (unsigned char)((float)0xFF*uiv->text_color[2]),
                              (unsigned char)((float)0xFF*uiv->text_color[3]));
    //platform_dbg_log("%X\n", text_color);

    auto imgui_frame_start_push_color = [uiv]
        (ImGuiCol element_type, int color32)
    {
        ImGui::PushStyleColor(element_type, color32);
        ++uiv->frame_start_color_stack_pushes;
    };

    //NOTE: default values are set in 2pacwav2_config.cpp:set_default_convars
    uiv->frame_start_color_stack_pushes = 0;
    imgui_frame_start_push_color(ImGuiCol_Text, text_color);
    imgui_frame_start_push_color(ImGuiCol_Button, button_color);
    imgui_frame_start_push_color(ImGuiCol_FrameBg, button_color);
    imgui_frame_start_push_color(ImGuiCol_WindowBg,  IM_COL32(0x00, 0x00, 0x00, 0xFF));
    imgui_frame_start_push_color(ImGuiCol_MenuBarBg, IM_COL32(0x22, 0x22, 0x33, 0xFF));

    ImGui::SetShortcutRouting(ImGuiMod_Ctrl|ImGuiKey_Tab, 0, ImGuiButtonFlags_NoSetKeyOwner);
    ImGui::SetShortcutRouting(ImGuiMod_Ctrl|ImGuiMod_Shift|ImGuiKey_Tab, 0, ImGuiButtonFlags_NoSetKeyOwner);
}

static void pac_end_frame(Runtime_Vars *rtvars, Sdl_Apidata *sdldata)
{
    State_Flags *sflags = &rtvars->sflags;
    General_Buffer_Group *bufgroup = rtvars->bufgroup_ptr;
    Music_Data *mdata = rtvars->mdata_ptr;

    if (sflags->searchwindow_open)
    { menu_do_search(rtvars, bufgroup, mdata); }

    if (sflags->metadata_editor_open)
    { menu_do_metadata_editor(rtvars, mdata, bufgroup); }

    ImGui::PopStyleColor(rtvars->uivars.frame_start_color_stack_pushes);

    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    Audio_Stream *astream = &rtvars->mdata_ptr->astream;
    Pacmxr_Context *pac_ctx = pacmxr_get_context();
    astream->stream = pac_ctx->aqueue.sdl_stream;
    astream->stream_size = pac_ctx->aqueue.sdl_stream_len;

    if ((rtvars->sflags.viewstate == CENTER_VIEW_STATE_CURRENT_INFO) &&
        (rtvars->sflags.visualizer_enabled))
    { do_visualizer(rtvars, sdldata); }
    SDL_GL_SwapWindow(sdldata->window_ptr);
}

static void update_audio_time(Music_Data *mdata)
{
    mdata->current_duration = pacmxr_stream_duration();
    mdata->current_position = pacmxr_seconds_played();
    Pacmxr_Context *pac_ctx = pacmxr_get_context();
    if ((!pac_ctx->paused && pacmxr_file_is_open()) && 
        ((mdata->current_position + 0.1) >= mdata->current_duration))
    { goto_next_file(mdata); }
}

static void update_info_buffer(Music_Data *mdata, General_Buffer_Group *bufgroup)
{
    snprintf((char *)bufgroup->music_info_buffer, DEBUG_BUFFER_SIZE,
            "[srate:%dhz][pcm_bits:%d][chan:%d]"
            "[vol:%d/128][pos:%06.1f/%06.1f][acodec:%s]"
            "\n[path:%s]", 
            mdata->sample_rate, mdata->pcm_bits, mdata->channels,
            mdata->volume, mdata->current_position, mdata->current_duration, mdata->music_type_buf,
            mdata->current_filename);
}

static void tupacmixer_get_audio_type(Music_Data *mdata)
{
    Pacmxr_Context *pac_ctx = pacmxr_get_context();
    if (pac_ctx->fctx.codec && pac_ctx->fctx.codec->name)
    {
        snprintf(mdata->music_type_buf,
                sizeof(mdata->music_type_buf), "%s",
                pac_ctx->fctx.codec->name);
    }
    else
    {
        snprintf(mdata->music_type_buf, sizeof(mdata->music_type_buf), "unknown");
    }
}

static void load_file_from_path(char *path, Music_Data *mdata)
{
    if (platform_file_exists(path))
    {
        tupacmixer_start_music(mdata, path);
    }
    else
    { 
        platform_dbg_log("%s: no such file or directory\n", path); 
        set_userinfo(mdata->rtvars_ptr, 
                "[error]: attempted to play file which doesn't exist.",
                USERINFO_TYPE_ERROR);
    }
}

static char *separate_file_and_dir_name(char *dir_in_out, 
                                        char *name_out,
                                        int dirlen)
{
    char *temp_in = dir_in_out + dirlen;
    int chars = 0;
    if (strchr(dir_in_out, '/'))
    {
        while (temp_in != dir_in_out)
        {
            if (*temp_in == '/')
            {
                *temp_in = 0x0;
                snprintf(name_out, chars, "%s", temp_in + 1);
                break;
            }
            --temp_in;
            ++chars;
        }
    }
    else
    {
        snprintf(name_out, dirlen + 1, "%s", dir_in_out);
        snprintf(dir_in_out, dirlen, ".");
    }
    return dir_in_out;
}

static char check_dir_already_added(char *dir, File_List *flist)
{
    char result = 0, *current_dir;
    char **all_dirs = flist->dirnames_string_loclist;
    int dir_count = flist->dirs_added;
    for (int dir_index = 0; dir_index < dir_count; ++dir_index)
    {
        current_dir = all_dirs[dir_index];
        if (!strcmp(current_dir, dir))
        {
            result = 1; 
            break; 
        }
    }
    return result;
}

static float conv_slide_value2songpos(Music_Data *mdata)
{
    float result = 0;
    if (pacmxr_file_is_open() &&
        mdata->current_position &&
        mdata->current_duration)
    {
        result = (((double)(mdata->seek_value)) /
                PAC_SEEK_VALUE_MAX) *
                mdata->current_duration;
    }
    return result;
}

static float conv_songpos2slide_value(Music_Data *mdata)
{
    float result = 0.0f;
    if (mdata->current_position &&
        mdata->current_duration)
    {
        result = ((float)(mdata->current_position / 
                mdata->current_duration)) * 
                PAC_SEEK_VALUE_MAX; 
    }
    return result;
}

static void file_list_push_dirname(char *dirname, File_List *flist)
{
    int dirlen = strlen(dirname);
    char *write_ptr = flist->dirnames_string_loclist[flist->dirs_added];
#if _2PACWAV_LINUX
    if (dirname[dirlen] == '/')
#else
    if (dirname[dirlen] == '\\')
#endif
    { dirname[dirlen] = 0; }
    strncpy(write_ptr, dirname, PATH_MAX - 1);
    write_ptr[dirlen + 1] = 0x0;
    ++flist->dirs_added;
    flist->dirnames_string_loclist[flist->dirs_added] = write_ptr + dirlen + 1;
}

static void metadata_string_push(char *src_buf,
                            int member_offset,
                            char *dest_buf,
                            int file_index,
                            Music_Data *mdata)
{
    File_List *ml = &mdata->music_list;
    char *write_ptr = dest_buf + (file_index*METADATA_STRING_SIZE);
    //genius
    void *ptr2set = (void *)((uintptr_t)&ml->file_strings[file_index].metadata + member_offset);
    memcpy(ptr2set, &write_ptr, sizeof(char *));
    strncpy(write_ptr, src_buf, METADATA_STRING_SIZE - 1);
}

static void get_metadata_for_file(Audio_File *file, Music_Data *mdata)
{
    Pacmxr_Metadata pm;
    char path[PATH_MAX];
    snprintf(path, PATH_MAX, "%s/%s", file->containing_dir, file->filename);
    char buf[METADATA_STRING_SIZE];
    File_List *mlist = &mdata->music_list;
    if (pacmxr_meta_open_file(&pm, path))
    {
        buf[0] = 0; pacmxr_meta_get_title(&pm, buf, METADATA_STRING_SIZE - 1);
        metadata_string_push(buf,
                offsetof(List_File_Metadata, title),
                mlist->titles_buf,
                mlist->entry_count,
                mdata);
        buf[0] = 0; pacmxr_meta_get_artist(&pm, buf, METADATA_STRING_SIZE - 1);
        metadata_string_push(buf,
                offsetof(List_File_Metadata, artist),
                mlist->artist_names_buf,
                mlist->entry_count,
                mdata);
        buf[0] = 0; pacmxr_meta_get_album(&pm, buf, METADATA_STRING_SIZE - 1);
        metadata_string_push(buf,
                offsetof(List_File_Metadata, album),
                mlist->album_names_buf,
                mlist->entry_count,
                mdata);

        pacmxr_meta_close_file(&pm);
    }
}

static char add_single_file_to_music_list(char *path, Music_Data *mdata)
{
    char dir[PATH_MAX];
    char name[NAME_MAX];
    snprintf(dir, PATH_MAX, "%s", path);
    separate_file_and_dir_name(dir, name, strlen(dir));
    File_List *mlist = &mdata->music_list;
    Audio_File *afs = mlist->file_strings;
    char cont_dir_already_added = check_dir_already_added(path, &mdata->music_list);

    char *file_entry = afs[mlist->entry_count].filename;
    strncpy(file_entry, name, NAME_MAX - 1);
    int file_len = strlen(name);
    file_entry[file_len + 1] = 0x0;

    if (!cont_dir_already_added) 
    { file_list_push_dirname(dir, mlist); } 
    afs[mlist->entry_count].containing_dir = mlist->dirnames_string_loclist[mlist->dirs_added - 1];
    afs[mlist->entry_count].containing_dir_hash = hash_fnv1a16((uint8_t *)dir, strlen(dir));

    afs[mlist->entry_count + 1].filename = file_entry + file_len + 1;
    get_metadata_for_file(&mlist->file_strings[mlist->entry_count], mdata);
    ++mdata->music_list.entry_count;

    return cont_dir_already_added;
}

static inline char file_is_playlist(char *path)
{
    char result = 0;
    int path_len = strlen(path);
    char *extension = path + (1 + path_len - sizeof(PLAYLIST_EXTENSION));
    if (!strcmp(extension, PLAYLIST_EXTENSION)) { result = 1; }
    return result;
}

static void add_to_music_list(char *path, Music_Data *mdata, Runtime_Vars *rtvars)
{
    int old_file_count = mdata->music_list.entry_count, 
                            new_file_count, 
                            added_files;

    if (platform_file_exists(path))
    {
        if (file_is_playlist(path))
        {
            //process_playlist recursively calls this with file path or directoryso it should just get handled
            process_playlist(rtvars, path);
        }
        else
        {
            add_single_file_to_music_list(path, mdata); 
        }
    }
    else if (platform_directory_exists(path))
    {
        int pathlen = strlen(path);
        
#if _2PACWAV_LINUX
        while (path[--pathlen] == '/')
#elif _2PACWAV_WIN32
        while (path[--pathlen] == '\\')
#endif
        { path[pathlen] = 0; }
        platform_list_files_mlist(path, &mdata->music_list); 
        platform_get_metadata_bulk(rtvars, path);
    }
    else
    {
        platform_dbg_log("%s: no such file or directory\n", path); 
        set_userinfo(rtvars, "[error]: no such file or directory.", USERINFO_TYPE_ERROR);
        return;
    }
    set_match_flags((char *)rtvars->bufgroup_ptr->inbuf_search, mdata);

    new_file_count = mdata->music_list.entry_count;
    added_files = new_file_count - old_file_count;
    char info_buf[USERINFO_BUFFER_SIZE];
    snprintf(info_buf, USERINFO_BUFFER_SIZE - 1,
            "[info]: added %d file(s). (%d total)",
            added_files, new_file_count);
    set_userinfo(rtvars, info_buf, USERINFO_TYPE_NOTE);
}

//it is truly embarrassing that this function was ever written
//i will keep it here as an act of humility
static char *find_top_level_path4file(char *filename, File_List *flist)
{
#if 0
    char *result = 0, *dirname;
    char pathbuf[PATH_MAX];
    for (int dir_index = 0; 
        dir_index < flist->dirs_added; 
        ++dir_index)
    {
        dirname = flist->dirnames_string_loclist[dir_index];
        snprintf(pathbuf, PATH_MAX - 1, "%s/%s", dirname, filename);
        if (platform_file_exists(pathbuf))
        {
            result = dirname; 
            break; 
        }
    }
#else
    printf("xd\n");
    PAC_ASSERT(0);
    return 0;
#endif
}

static void file_list_play_file(Audio_File *selected_file, 
                                uint32_t file_index, 
                                Music_Data *mdata)
{
    char path_buf[PATH_MAX];
    //char *toplevel_path = find_top_level_path4file(selected_file, &mdata->music_list);
    char *containing_dir = selected_file->containing_dir;
    if (containing_dir)
    {
        snprintf(path_buf, PATH_MAX - 1, "%s/%s", containing_dir, selected_file->filename);
        //platform_dbg_log("trying to play %s\n", path_buf);

        char info_buf[USERINFO_BUFFER_SIZE];
        snprintf(info_buf, USERINFO_BUFFER_SIZE - 1, "[info]: playing %s", selected_file->filename);
        set_userinfo(mdata->rtvars_ptr, info_buf, USERINFO_TYPE_NOTE);

        strncpy(mdata->current_filename, path_buf, PATH_MAX - 1);
        mdata->music_list.current_index = file_index;
        prev_file_list_push(mdata, selected_file->filename);
        load_file_from_path(path_buf, mdata);
    }
    else
    {
        platform_dbg_log("could not find containing directory for file %s.\n", selected_file->filename); 
        char info[USERINFO_BUFFER_SIZE];
        snprintf(info, USERINFO_BUFFER_SIZE - 1,
                "could not find containing directory for file %s.", 
                selected_file->filename);
        set_userinfo(mdata->rtvars_ptr, info,USERINFO_TYPE_ERROR);
    }
}

static int get_random_file_index(Music_Data *mdata)
{
    File_List *mlist = &mdata->music_list;
    int cur_index = (int)mlist->current_index;
    int next_index = cur_index;
    while (next_index == cur_index) 
    { next_index = ((double)(mlist->entry_count) - 1.0)*drand48(); }
    return next_index;
}

static void goto_next_file(Music_Data *mdata)
{
    if (mdata->music_list.entry_count < 1)
    { return; }

    File_List *mlist = &mdata->music_list;
    uint32_t cur_index = mlist->current_index;
    mdata->previous_index = cur_index;
    Audio_File *next_file;
    if (!mdata->loop_enabled)
    {
        if (!mdata->shuffle_enabled)
        {
            if (mlist->current_index < (mlist->entry_count - 1))
            {
                next_file = &mlist->file_strings[cur_index + 1];
                file_list_play_file(next_file, cur_index + 1, mdata);
            }
        }
        else
        {
            uint32_t next_index = cur_index;
            if (mlist->entry_count != 2) 
            { next_index = get_random_file_index(mdata); }
            else 
            { next_index = !next_index; }
            next_file = &mlist->file_strings[next_index];
            file_list_play_file(next_file, next_index, mdata);
        }
    }
    else
    {
        next_file = &mlist->file_strings[cur_index];
        file_list_play_file(next_file, cur_index, mdata);
    }
}

static void goto_prev_file(Music_Data *mdata)
{
    if (0 == mdata->music_list.current_index) 
    { return; }
    File_List *mlist = &mdata->music_list;
    uint32_t cur_index = mlist->current_index;
    
    char *prev_file = prev_file_list_get_last(&mdata->music_list.prev_files);
    int index;
    Audio_File *af = file_list_get_audio_file(prev_file, &index, &mdata->music_list);

    //int index = file_list_get_index(&mdata->music_list, af);
    file_list_play_file(af, index, mdata);

#if 0
    int goto_index = cur_index - 1;
    if ((goto_index < 0) || (goto_index >= (int)mlist->entry_count)) 
    { goto_index = get_random_file_index(mdata); }

    char *prev_file = mlist->filenames_string_loclist[goto_index];
    file_list_play_file(prev_file, cur_index - 1, mdata);
#endif
}

static void debug_print_prev_files(Prev_File_List *pfl)
{
    char c;
    printf("PREV_FILE_LIST - file_count=%d\n", pfl->file_count);
    for (int i = 0; i < pfl->file_count; ++i)
    {
        c = ' ';
        if (pfl->current_index == i) { c = '>'; }
        printf("%c  %s\n", c, pfl->filenames[i]);
    }
}

//this is so shit
static Audio_File *file_list_get_audio_file(char *path,
                                        int *out_index,
                                        File_List *flist)
{
    Audio_File *result = 0;
    Audio_File *array = flist->file_strings;
    for (int file_index = 0;
        file_index < flist->entry_count;
        ++file_index)
    {
        if (!strcmp(array[file_index].filename, path))
        {
            result = &array[file_index];
            if (out_index) { *out_index = file_index; }
            break;
        }
    }
    return result;
}

static char *prev_file_list_get_last(Prev_File_List *pfl)
{
    char *result;
    if (pfl->current_index > 0)
    {
        --pfl->current_index;
        result = pfl->filenames[pfl->current_index];
    }
    else
    {
        result = pfl->filenames[PREV_FILES_LIST_MAX_FILES - 1];
        pfl->current_index = PREV_FILES_LIST_MAX_FILES - 1;
    }
#if 1 && _2PACWAV_DEBUG 
    debug_print_prev_files(pfl);
#endif
    return result;
}

static void prev_file_list_push(Music_Data *mdata, char *path)
{
    Prev_File_List *pfl = &mdata->music_list.prev_files;
    if (pfl->file_count &&
        !strcmp(path, pfl->filenames[pfl->current_index]))
    { return; }
    if (pfl->current_index >= (PREV_FILES_LIST_MAX_FILES - 1))
    { pfl->current_index = -1; }
    ++pfl->current_index;
    memset(pfl->filenames[pfl->current_index], 0, NAME_MAX);
    strncpy(pfl->filenames[pfl->current_index], path, NAME_MAX - 1);
    if (pfl->file_count < PREV_FILES_LIST_MAX_FILES)
    { ++pfl->file_count; }

#if 1 && _2PACWAV_DEBUG
    debug_print_prev_files(&mdata->music_list.prev_files);
#endif
}

static void init_prev_file_list(Prev_File_List *pfl)
{
    pfl->filenames[0] = (char *)pfl->buffer;
    memset(pfl->buffer, 0, PREV_FILES_BUFFER_SIZE);
    for (int i = 0; i < PREV_FILES_LIST_MAX_FILES; ++i)
    {
        pfl->filenames[i] = (char *)(((uintptr_t)pfl->buffer + (NAME_MAX*i)) + 1);
    }
    pfl->current_index = -1;
}


static void clear_file_list(Music_Data *mdata)
{
    if (!mdata->music_list.entry_count)
    { return; }
    File_List *mlist = &mdata->music_list;

    for (int clear_index = 1;
        clear_index < mlist->entry_count;
        ++clear_index)
    {
        mlist->file_strings[clear_index].filename = 0;
        mlist->file_strings[clear_index].metadata.artist = 0;
        mlist->file_strings[clear_index].metadata.title = 0;
        mlist->file_strings[clear_index].metadata.album = 0;
        mlist->dirnames_string_loclist[clear_index] = 0;
    }

    memset(mlist->filenames_buf, 0, FILENAMES_BUFFER_SIZE);
    memset(mlist->dirnames_buf, 0, DIRNAMES_BUFFER_SIZE);
    memset(mlist->titles_buf, 0, METADATA_STRINGS_BUFSIZE);
    memset(mlist->artist_names_buf, 0, METADATA_STRINGS_BUFSIZE);
    memset(mlist->album_names_buf, 0, METADATA_STRINGS_BUFSIZE);
    //memset(mlist->match_flags, 0, MATCH_FLAGS_BUFFER_SIZE);
    mlist->entry_count = 0;
    mlist->current_index = 0;
    mlist->dirs_added = 0;
    mlist->match_count = 0;
}

void set_userinfo(Runtime_Vars *rtvars, char *notice, Userinfo_Type notice_type)
{
    rtvars->sflags.last_userinfo_type = notice_type;
    char *note_buf = (char *)rtvars->bufgroup_ptr->userinfo_buffer;
    strncpy(note_buf, notice, USERINFO_BUFFER_SIZE - 1);
}

static void set_userinfo_color(Runtime_Vars *rtvars)
{
    switch (rtvars->sflags.last_userinfo_type) {
    case USERINFO_TYPE_ERROR:
    { 
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0xFF, 0x20, 0x20, 0xFF));
    } break;
    case USERINFO_TYPE_WARNING:
    {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0xFF, 0x90, 0x00, 0xFF));
    } break;
    case USERINFO_TYPE_NOTE:
    default:
    {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0xFF, 0xFF, 0x00, 0xFF));
    } break;
    }
}

static void tupacmixer_get_cover(Bitmap_Info *bmpinfo, Runtime_Vars *rtvars)
{
    bmpinfo->img_data = pacmxr_meta_get_cover(0, &bmpinfo->img_data_bytes);
    if (bmpinfo->img_data)
    {
        pac_init_bitmap(bmpinfo, rtvars);
    }
}

#if STBIMAGE_ENABLED
static void pac_init_bitmap(Bitmap_Info *bmpinfo, Runtime_Vars *rtvars)
{
    bmpinfo->img_data = stbi_load_from_memory(bmpinfo->img_data,
                            bmpinfo->img_data_bytes,
                            &bmpinfo->width,
                            &bmpinfo->height,
                            &bmpinfo->chan,
                            4);
    if (bmpinfo->ogl_tex_id) 
    { glDeleteTextures(1, &bmpinfo->ogl_tex_id); }

    glGenTextures(1, &bmpinfo->ogl_tex_id);
    glBindTexture(GL_TEXTURE_2D, bmpinfo->ogl_tex_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0,
            GL_RGBA,
            bmpinfo->width,
            bmpinfo->height,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            bmpinfo->img_data);

    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(bmpinfo->img_data);
}
#endif

static void menu_do_current_file_info(Runtime_Vars *rtvars, 
                                    Music_Data *mdata, 
                                    General_Buffer_Group *bufgroup)
{
    update_info_buffer(mdata, bufgroup);
    char *infobuf = (char *)bufgroup->music_info_buffer;
    char *path_begin = strchr(infobuf, '\n');
    *path_begin = 0; ++path_begin;
    Sdl_Apidata *sdldata = rtvars->sdldata_ptr;

    tupacmixer_get_taginfo(mdata);

    ImGui::Text("%s\n%s", (char *)bufgroup->music_info_buffer, path_begin);
    
    if (mdata->cover.img_data &&
        mdata->cover.img_data_bytes &&
        mdata->cover.ogl_tex_id)
    {
        const float dimension = 500.0f, heightpad = 220.0f, alpha = 0.5f;
        Sdl_Apidata *sdldata = rtvars->sdldata_ptr;
        ImGui::SetCursorPos(ImVec2((sdldata->win_width/2) - (dimension/2),
                        (sdldata->win_height) - (dimension + heightpad)));
        ImVec2 cover_dims = ImVec2(dimension, dimension);
        //NOTE: apparently this overload of ImGui::Image is deprecated/obsolete or some shit
        //but it works on our imgui version and is the easiest way i found to change the alpha.
        //so if imgui is updated some day for whatever reason then this might no longer compile

        ImGui::ImageWithBg(mdata->cover.ogl_tex_id,
                cover_dims,
                ImVec2(0, 0),
                ImVec2(1, 1),
                ImVec4(0, 0, 0, 0),
                ImVec4(1, 1, 1, alpha));
    }

    char *taginfo_buffer = mdata->current_metadata.tagbuffer;
    ImVec2 tdims = ImGui::CalcTextSize(taginfo_buffer);
    ImGui::SetCursorPos(ImVec2(((float)sdldata->win_width/2.0f) - (tdims.x/2.0f), 
            sdldata->win_height - 200.0f));
    ImGui::Text("%s", taginfo_buffer);
}

static void menu_do_search(Runtime_Vars *rtvars,
                        General_Buffer_Group *bufgroup,
                        Music_Data *mdata)
{
    Sdl_Apidata *sdldata = rtvars->sdldata_ptr;
    //NOTE: this was moved to the end of the main loop which fixed the window being impossible to move
    //ImVec2 winsize = ImVec2(400, 70);
    //ImGui::SetNextWindowSize(winsize);
    //ImVec2 winpos = ImVec2(sdldata->win_width - winsize.x - 10, winsize.y);
    //ImGui::SetNextWindowPos(winpos);

    if (ImGui::Begin("search list", 0, 
        ImGuiWindowFlags_NoScrollbar
        |ImGuiWindowFlags_NoResize
        |ImGuiWindowFlags_NoCollapse))
    {
        ImGui::SetKeyboardFocusHere(0);
        if (!ImGui::IsWindowHovered() &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        { rtvars->sflags.searchwindow_open = 0; }

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::InputText("##search_query", 
            (char *)bufgroup->inbuf_search,
            SEARCH_BUFFER_SIZE - 1))
        {
            set_match_flags((char *)bufgroup->inbuf_search, mdata); 
            char info_buf[USERINFO_BUFFER_SIZE];
            snprintf(info_buf, 
                    USERINFO_BUFFER_SIZE, 
                    "[search]: %d results", 
                    mdata->music_list.match_count);
            set_userinfo(rtvars, info_buf, USERINFO_TYPE_NOTE);
        }

        ImGui::End();
    }
}

static void imgui_window_init(float main_win_w,
                            float main_win_h,
                            float wanted_win_w,
                            float wanted_win_h)
{
    ImVec2 size = ImVec2(wanted_win_w, wanted_win_h);
    ImVec2 pos = ImVec2((main_win_w/2) - (size.x/2),
                        (main_win_h/2) - (size.y/2));
    ImGui::SetNextWindowSize(size);
    ImGui::SetNextWindowPos(pos);
}

static inline bool imgui_window_should_close(void)
{
    char result = ((!ImGui::IsWindowHovered() &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left)) ||
            ImGui::IsKeyPressed(ImGuiKey_Escape));
    return result;
}

static void update_in_memory_metadata(char *src_buf,
                            char *dest_buf,
                            int member_offset,
                            int file_index,
                            Music_Data *mdata)
{
    if (!dest_buf)
    {
        strncpy(dest_buf, src_buf, META_EDITOR_BUFSIZE - 1);
    }
    else
    {
        metadata_string_push(src_buf,
                member_offset,
                dest_buf,
                file_index,
                mdata);
    }
};

static void menu_do_metadata_editor(Runtime_Vars *rtvars, 
                                    Music_Data *mdata, 
                                    General_Buffer_Group *bufgroup)
{
    Metadata_Editor *meta = &mdata->metaed;
    static char window_initialized = false;
    State_Flags *sflags = &rtvars->sflags;
    File_List *mlist = &mdata->music_list;
    char *file_path = meta->editor_current;
    char *filename = meta->selected_audio_file.filename;
    Pacmxr_Metadata *pm = &meta->meta_struct;

    ImVec2 path_dimensions = ImGui::CalcTextSize(file_path);
    float win_width = path_dimensions.x + ImGui::CalcTextSize("file:").x;
    float row_height = path_dimensions.y, pad = 15.0f;
    int rows = 8;
    if (!window_initialized)
    {
        Sdl_Apidata *sdld = rtvars->sdldata_ptr;
        ImVec2 size = ImVec2(win_width + 40.0f, ((row_height + pad)*rows));
        ImGui::SetNextWindowSize(size);
        ImVec2 center = ImVec2(((sdld->win_width/2) - (size.x/2)),
                                (sdld->win_height/2) - (size.y/2));
        ImGui::SetNextWindowPos(center);

        window_initialized = true;
    }

    if (ImGui::Begin("metadata editor", 0,
        //ImGuiWindowFlags_NoScrollbar|
        ImGuiWindowFlags_NoCollapse|
        ImGuiWindowFlags_NoResize))
    {
        if (imgui_window_should_close())
        { sflags->metadata_editor_open = 0; }

        if (file_path[0])
        {
            ImGui::Text("file:");
            ImGui::SameLine();
            ImGui::PushItemWidth(path_dimensions.x);
            ImGui::PushItemWidth(path_dimensions.x + 10.0f);
            ImGui::InputText("##metadata_filename", 
                    file_path, 
                    PATH_MAX - 1, 
                    ImGuiInputTextFlags_ReadOnly);
            ImGui::PopItemWidth();
            //ImGui::SetCursorPosX(50);
            ImGui::InputText("title##meta_title", 
                    meta->inbuf_title, 
                    META_EDITOR_BUFSIZE - 1);

            //ImGui::SetCursorPosX(50);
            ImGui::InputText("artist##meta_artist", 
                    meta->inbuf_artist, 
                    META_EDITOR_BUFSIZE - 1);

            //ImGui::SetCursorPosX(50);
            ImGui::InputText("album##meta_album", 
                    meta->inbuf_album, 
                    META_EDITOR_BUFSIZE - 1);
            //ImGui::SetCursorPosX(50);
            ImGui::InputText("genre##meta_genre", 
                    meta->inbuf_genre, 
                    META_EDITOR_BUFSIZE - 1);

            ImGui::PushItemWidth(100);

            //ImGui::SetCursorPosX(50);
            ImGui::InputText("year##meta_year", 
                    meta->inbuf_date, 
                    sizeof(meta->inbuf_date) - 1);

            //ImGui::SetCursorPosX(50);
            ImGui::InputText("track##meta_tracknum", 
                    meta->inbuf_tracknum, 
                    sizeof(meta->inbuf_tracknum) - 1);
            
            ImGui::PopItemWidth();
            ImGui::PopItemWidth();
            //ImGui::SetCursorPosX(50);

            if (ImGui::Button("save metadata"))
            {
                if (meta->meta_struct.in_avf_ctx)
                {
                    pacmxr_meta_set_title(pm, meta->inbuf_title);
                    pacmxr_meta_set_artist(pm, meta->inbuf_artist);
                    pacmxr_meta_set_album(pm, meta->inbuf_album);
                    pacmxr_meta_set_genre(pm, meta->inbuf_genre);
                    pacmxr_meta_set_year(pm, strtol(meta->inbuf_date, 0, 10));
                    pacmxr_meta_set_track(&meta->meta_struct, strtol(meta->inbuf_tracknum, 0, 10));

                    if (pacmxr_meta_save(&meta->meta_struct))
                    {
                        Audio_File *af = &meta->selected_audio_file;
                        update_in_memory_metadata(meta->inbuf_title,
                                af->metadata.title,
                                offsetof(List_File_Metadata, title),
                                meta->selected_file_index,
                                mdata);
                        update_in_memory_metadata(meta->inbuf_artist,
                                af->metadata.artist,
                                offsetof(List_File_Metadata, artist),
                                meta->selected_file_index,
                                mdata);
                        update_in_memory_metadata(meta->inbuf_album,
                                af->metadata.album,
                                offsetof(List_File_Metadata, album),
                                meta->selected_file_index,
                                mdata);

                        set_userinfo(rtvars, 
                                "updated metadata.", 
                                USERINFO_TYPE_NOTE);
                    }
                    else
                    {
                        set_userinfo(rtvars, 
                                "failed to update metadata.", 
                                USERINFO_TYPE_ERROR);
                    }
                rtvars->sflags.metadata_editor_open = 0;
                }
                else
                {
                    set_userinfo(rtvars, 
                            "[error]: failed to update metadata.", 
                            USERINFO_TYPE_ERROR);
                }
            }
        }
        else
        {
            ImGui::Text("invalid file selected for metadata editor");
        }
        ImGui::End();
    }

    if (!rtvars->sflags.metadata_editor_open)
    { window_initialized = false; }
}

static void init_metadata_editor(Runtime_Vars *rtvars, Music_Data *mdata)
{
    File_List *mlist = &mdata->music_list;
    Metadata_Editor *meta = &mdata->metaed;
    Audio_File *file = &mlist->file_strings[mlist->context_index];
    //Sdl_Apidata *sdld = rtvars->sdldata_ptr;
    //ImVec2 size = ImVec2(600, 600);
    //ImGui::SetNextWindowSize(size);
    //ImVec2 center = ImVec2(((sdld->win_width/2) - (size.x/2)),
    //                        (sdld->win_height/2) - (size.y/2));
    //ImGui::SetNextWindowPos(center);

    if (file->containing_dir && file->containing_dir[0])
    {
        snprintf(meta->editor_current, PATH_MAX,
#if _2PACWAV_LINUX
                "%s/%s", 
#elif _2PACWAV_WIN32
                "%s\\%s", 
#endif
                file->containing_dir, file->filename);
        if (meta->meta_struct.in_avf_ctx)
        {
            pacmxr_meta_close_file(&meta->meta_struct); 
        }

        //memset((void *)&meta->selected_audio_file, 0, sizeof(Audio_File));
        memcpy((void *)&meta->selected_audio_file, (void *)file, sizeof(Audio_File));
        meta->selected_file_index = mlist->context_index;

        meta->inbuf_title[0] = 0;
        meta->inbuf_artist[0] = 0;
        meta->inbuf_album[0] = 0;
        meta->inbuf_genre[0] = 0;
        meta->inbuf_tracknum[0] = 0;
        meta->inbuf_date[0] = 0;
        if (pacmxr_meta_open_file(&meta->meta_struct, meta->editor_current))
        {
            pacmxr_meta_get_title(&meta->meta_struct,
                    meta->inbuf_title,
                    META_EDITOR_BUFSIZE);
            pacmxr_meta_get_artist(&meta->meta_struct,
                    meta->inbuf_artist,
                    META_EDITOR_BUFSIZE);
            pacmxr_meta_get_album(&meta->meta_struct,
                    meta->inbuf_album,
                    META_EDITOR_BUFSIZE);
            pacmxr_meta_get_genre(&meta->meta_struct,
                    meta->inbuf_genre,
                    META_EDITOR_BUFSIZE);
            int date = pacmxr_meta_get_year(&meta->meta_struct);
            if (date >= 0)
            {
                snprintf(meta->inbuf_date, sizeof(meta->inbuf_date), "%d", date);
            } 
            int track = pacmxr_meta_get_track(&meta->meta_struct);
            if (track >= 0)
            {
                snprintf(meta->inbuf_tracknum, sizeof(meta->inbuf_tracknum), "%d", track);
            }
        }
        else
        {
            set_userinfo(rtvars,
                    "[error]: couldn't get metadata information",
                    USERINFO_TYPE_ERROR);
        }
    }
    else
    { 
        meta->editor_current[0] = 0; 
    }
}

static void sort_list_by_column(ImGuiTableSortSpecs *specs, Runtime_Vars *rtvars)
{
    Music_Data *mdata = rtvars->mdata_ptr;
    Audio_File *files = mdata->music_list.file_strings;
    File_List *list = &mdata->music_list;
    State_Flags *sflags = &rtvars->sflags;

    enum Col_Index
    {
        COL_INDEX_ARTIST = 0,
        COL_INDEX_TITLE = 1,
        COL_INDEX_ALBUM = 2,
    };

    Sort_Comp_Func cmpf = 0;
    int direction = specs->Specs->SortDirection;

    switch (specs->Specs->ColumnIndex)
    {
    case COL_INDEX_ARTIST:
    {
        if (ImGuiSortDirection_Ascending == direction)
        {
            cmpf = pac_qsort_artist_strcmp;
            sflags->sort_state = SORT_STATE_ARTIST_ASCENDING;

        }
        else
        {
            cmpf = pac_qsort_artist_strcmp_rev;
            sflags->sort_state = SORT_STATE_ARTIST_DESCENDING;
        }
    } break;

    case COL_INDEX_TITLE:
    {
        if (ImGuiSortDirection_Ascending == direction)
        {
            cmpf = pac_qsort_title_strcmp;
            sflags->sort_state = SORT_STATE_TITLE_ASCENDING;
        }
        else
        {
            cmpf = pac_qsort_title_strcmp_rev;
            sflags->sort_state = SORT_STATE_TITLE_DESCENDING;
        }
    } break;

    case COL_INDEX_ALBUM:
    {
        if (ImGuiSortDirection_Ascending == direction)
        {
            cmpf = pac_qsort_album_strcmp;
            sflags->sort_state = SORT_STATE_ALBUM_ASCENDING;
        }
        else
        {
            cmpf = pac_qsort_album_strcmp_rev;
            sflags->sort_state = SORT_STATE_ALBUM_DESCENDING;
        }
    } break;
    default: break;
    }

    //platform_dbg_log("sorted by %d\n", specs->Specs->ColumnIndex);

    if (cmpf)
    { qsort(files, list->entry_count, sizeof(Audio_File), cmpf); }
}

static void menu_do_music_list(Runtime_Vars *rtvars, Music_Data *mdata)
{
    File_List *mlist = &mdata->music_list;
    State_Flags *sflags = &rtvars->sflags;
    uint32_t render_index = 0, file_index = 0;
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0, 0.5f));
    char btntext[NAME_MAX];
    char *filename, *title, *artist, *album;
    Audio_File *current;
    int current_index;
    const uint8_t *kbd = rtvars->kbd_state;
    char ctrl, shift;

    ImVec2 width;
    width.x = ImGui::GetColumnWidth(-1);
    width.y = 0;
    ImGuiStyle &im_style = ImGui::GetStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, {3, 3});

    char searching = *(char *)rtvars->bufgroup_ptr->inbuf_search;

    if (ImGui::BeginTable("music list table", 3,
        (ImGuiTableFlags_Resizable
        |ImGuiTableFlags_Reorderable
        |ImGuiTableFlags_Sortable
        |ImGuiTableFlags_RowBg
        |ImGuiTableFlags_Borders
        |ImGuiTableFlags_PadOuterX)))
    {
        ImGui::TableSetupColumn("artist", ImGuiTableColumnFlags_WidthFixed, 250.0f);
        ImGui::TableSetupColumn("title", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableSetupColumn("album", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableHeadersRow();

        //TODO: this is a band-aid solution to the fact that without this check
        //while metadata is being retrieved the list can be sorted which causes some files
        //to be skipped. one option would be to make a copy of the entire list for themetadata
        //processor and then when its done the copied list can be sorted into the same
        //order as the actual list and then copied preferably in synchronous fashion

        if (!rtvars->sflags.metadata_getter_thread_working)
        {
            ImGuiTableSortSpecs *sort_specs = ImGui::TableGetSortSpecs();
            if (sort_specs->SpecsDirty)
            {
                sort_list_by_column(sort_specs, rtvars);
                sort_specs->SpecsDirty = false;
            }
        }

        //ImGuiListClipper is actually gangsta as fuck
        ImGuiListClipper clipper;
        clipper.Begin(mlist->match_count);
        while (clipper.Step())
        {
            for (int file_index = clipper.DisplayStart;
                file_index < clipper.DisplayEnd;
                ++file_index)
            {
                ImGui::TableNextRow();
                ImGui::PushID(file_index);

                int current_index = file_index;
                if (searching)
                {
                    current_index = mlist->matching_indices[file_index];
                }
                current = &mlist->file_strings[current_index];
                filename = current->filename;
                title = current->metadata.title;
                artist = current->metadata.artist;
                album = current->metadata.album;

                btntext[0] = '-'; btntext[1] = 0;
                if (artist && artist[0])
                {
                    snprintf(btntext, NAME_MAX, "%s", artist);
                }
                ImGui::TableNextColumn();
                ImGui::Text("%s", btntext);

                snprintf(btntext, NAME_MAX, "%s",
                        (title && title[0]) ? title : filename);
                ImGui::TableNextColumn();
                ImGui::Text("%s", btntext);

                btntext[0] = '-'; btntext[1] = 0;
                if (album && album[0])
                {
                    snprintf(btntext, NAME_MAX, "%s", album);
                }
                ImGui::TableNextColumn();
                ImGui::Text("%s", btntext);

                ImGui::TableSetColumnIndex(0);
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(im_style.ItemSpacing.x, im_style.CellPadding.y*2));
                if (ImGui::Selectable("",
                        false,
                        ImGuiSelectableFlags_SpanAllColumns))
                {
                    file_list_play_file(current, file_index, mdata);
                }

                ImGui::PopStyleVar();
                if (ImGui::BeginPopupContextItem(0))
                {
                    mlist->context_index = current_index;

                    if (ImGui::Button("edit metadata"))
                    {
                        sflags->metadata_editor_open = true;
                        init_metadata_editor(rtvars, mdata);
                    }
                    ImGui::EndPopup();
                }
                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleVar();
}

static void str2lowercase(char *string, int len)
{
    for (int i = 0; i < len; ++i)
    {
        string[i] = tolower(string[i]);
    }
}

static void set_match_flags(char *searchbuf, Music_Data *mdata)
{
    File_List *mlist = &mdata->music_list;
    Audio_File *files = mdata->music_list.file_strings;
    mlist->match_count = 0;
    static char fileinfo_buffer[4096];
    fileinfo_buffer[0] = 0;
#define _2PACWAV_SEARCH_METRICS 0
#if _2PACWAV_SEARCH_METRICS
    uint64_t start = platform_get_timestamp();
#endif

    if (searchbuf[0])
    {
        Audio_File *file;
        char *filename, *artist, *title, *album;
        for (int file_index = 0;
            file_index < mlist->entry_count;
            ++file_index)
        {
            file = &mlist->file_strings[file_index];
            filename = file->filename;
            artist = file->metadata.artist;
            title = file->metadata.title;
            album = file->metadata.album;
            snprintf(fileinfo_buffer, sizeof(fileinfo_buffer)- 1,
                    "%s\n%s\n%s\n%s",
                    filename,
                    artist ? artist : "",
                    title ? title : "",
                    album ? album : "");

            //NOTE: for some reason i made it so that 0 means this shit IS included in the search results
            if (SDL_strcasestr(fileinfo_buffer, searchbuf))
            {
                //mlist->match_flags[file_index] = 0;
                mlist->matching_indices[mlist->match_count] = file_index;
                ++mlist->match_count;
            }
            //else
            //{ 
            //    mlist->match_flags[file_index] = 1; 
            //}
        }
    }
    else
    {
        //memset(mlist->match_flags, 0, mlist->entry_count);
        memset(mlist->matching_indices, 0, sizeof(uint16_t)*mlist->entry_count);
        //for (int file_index = 0;
        //    file_index < mlist->entry_count;
        //    ++file_index)
        //{
        //    mlist->matching_indices[file_index] = file_index;
        //}
        mlist->match_count = mlist->entry_count;
    }

#if _2PACWAV_SEARCH_METRICS
    uint64_t end = platform_get_timestamp(), delta;
    delta = end - start;
    platform_dbg_log("[search] completed search in %.3f ms (%d matches)\n",
            (float)delta/1000.0f, mlist->match_count);
#endif
}

static void menu_do_volume_bar(Runtime_Vars *rtvars,
                            Music_Data *mdata,
                            float vol_width)
{
    State_Flags *sflags = &rtvars->sflags;
    int vol_step = rtvars->sargs_ptr->volume_step;
    //if(rtvars->kbd_state[SDL_SCANCODE_LCTRL])
    Keybinds *keys = &rtvars->keybinds;
    if (!ImGui::GetIO().WantTextInput)
    {
        if (ImGui::Shortcut(keys->vol_up, ImGuiInputFlags_RouteGlobal))
        { 
            if ((mdata->volume + vol_step) < SDL_MIX_MAXVOLUME)
            { 
                mdata->volume += vol_step; 
            }
            else
            {
                mdata->volume = SDL_MIX_MAXVOLUME;
            }
            pacmxr_set_volume(mdata->volume);
        }
        if (ImGui::Shortcut(keys->vol_down, ImGuiInputFlags_RouteGlobal))
        {
            if (((int)mdata->volume - vol_step) > 0)
            {
                mdata->volume -= vol_step;
            }
            else
            {
                mdata->volume = 0;
            }
            pacmxr_set_volume(mdata->volume);
        }
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(vol_width);
    if (ImGui::SliderInt("##vol_slider", 
        &mdata->volume, 0, 
        SDL_MIX_MAXVOLUME, "vol: %d",
        ImGuiSliderFlags_NoInput)) 
    { pacmxr_set_volume(mdata->volume); }
}

static void menu_do_seek_bar(Runtime_Vars *rtvars, Music_Data *mdata)
{
    State_Flags *sflags = &rtvars->sflags;
    const uint8_t *kbd = rtvars->kbd_state;
    mdata->seek_value = conv_songpos2slide_value(mdata);
    Keybinds *keys = &rtvars->keybinds;

    if (!ImGui::GetIO().WantTextInput)
    {
        float new_pos;
        //if (pac_btn_press(SDL_SCANCODE_RIGHT, &sflags->right_wasdown, kbd))
        if (ImGui::Shortcut(keys->seek_forward, ImGuiInputFlags_RouteGlobal))
        {
            new_pos = mdata->current_position + (double)mdata->seek_increment;
            //new_pos = mdata->seek_value + 0.05f;
            if (new_pos < mdata->current_duration)
            {
                pacmxr_seek(pacmxr_seconds_to_seek_value(new_pos));
            }
            else
            {
                goto_next_file(mdata); 
            }
        }
        else if (ImGui::Shortcut(keys->seek_backward, ImGuiInputFlags_RouteGlobal))
        {
            new_pos = mdata->current_position - (double)mdata->seek_increment;
            //new_pos = mdata->seek_value - 0.05f;
            if (new_pos > 0.0)
            {
                pacmxr_seek(pacmxr_seconds_to_seek_value(new_pos));
            }
            else
            {
                pacmxr_seek(0.0f);
            }
        }
        else if (ImGui::Shortcut(keys->seek_to_start, ImGuiInputFlags_RouteGlobal))
        {
            //NOTE: this doesn't work for the home key on my laptop keypad for example
            pacmxr_seek(0.0f);
        }
    }

    char current_pos_buf[32];
    snprintf(current_pos_buf, sizeof(current_pos_buf), "%06.1f/%06.1f",
                mdata->current_position, mdata->current_duration);
    float pos_width = ImGui::CalcTextSize(current_pos_buf).x;
    ImGui::SameLine();
    float width_left = ImGui::GetContentRegionAvail().x - pos_width - 10;
    ImGui::SetNextItemWidth(width_left);
    if (ImGui::SliderInt("##seeker", 
        &mdata->seek_value, 0, 
        PAC_SEEK_VALUE_MAX, "",
        ImGuiSliderFlags_NoInput|ImGuiSliderFlags_AlwaysClamp))
    { pacmxr_seek(mdata->seek_value/(float)PAC_SEEK_VALUE_MAX); }
    ImGui::SameLine();
    ImGui::Text("%s", current_pos_buf);
}

static void do_path_autocomplete(char *current, Runtime_Vars *rtvars)
{
#if 0
    File_List *alist = &rtvars->autocomp_list;
    PAC_LOCAL_STATIC int suggest_index = -1;
    char tempbuf[PATH_MAX];
    int len = strlen(current);
    strncpy(tempbuf, current, PATH_MAX - 1);
    int temp_len = len;
    while (--temp_len)
    {
        tempbuf[temp_len] = 0;
#if _2PACWAV_LINUX
        if (tempbuf[temp_len] == '/')
#elif _2PACWAV_WIN32
        if (tempbuf[temp_len] == '\\')
#endif
        { break; }
    }

    if (platform_path_exists(tempbuf))
    {
        alist->entry_count = 0;
        suggest_index = -1;
        platform_list_files_simple(tempbuf, alist, 1);
#if 0
        printf("%d files found in %s\n", alist->entry_count, current);
        for (int i = 0; i < (int)alist->entry_count; ++i)
        {
            printf("%s\n", alist->filenames_string_loclist[i]);
        }
#endif
        //printf("dir %s exists. it has %d things\n", tempbuf, alist->entry_count);
        char *pattern = current;
        int pattern_len = strlen(pattern);
        while (temp_len--)
        {
#if _2PACWAV_LINUX
            if (current[temp_len] == '/')
#elif _2PACWAV_WIN32
            if ((current[temp_len] == '/') || (current[temp_len] == '\\'))
#endif
            {
                pattern = &current[temp_len] + 1;
                break;
            }
        }

        if (suggest_index >= (int)alist->entry_count) 
        { suggest_index = -1; }

        char *i_file;
        //printf("files:\n");
        for (int i = suggest_index + 1; i < (int)alist->entry_count; ++i)
        {
            temp_len = 0;
            i_file = alist->filenames_string_loclist[i];
            //printf("%s\n", i_file);
            do
            {
                if (temp_len == pattern_len - 1)
                {
                    suggest_index = i;
                    i = alist->entry_count + 1; //break out of outer loop
                    break;
                }
                else if (pattern[temp_len] != i_file[temp_len])
                {
                    break; 
                } 
            } while (++temp_len);
        }

        //printf("whole:%s pattern: %s idx=%d fname=%s templ=%d patl=%d\n", 
        //        current, 
        //        pattern, 
        //        suggest_index, 
        //        alist->filenames_string_loclist[suggest_index],
        //        temp_len, 
        //        pattern_len); 

        //lmao
        if (suggest_index != -1)
        {
            strncpy(tempbuf, current, PATH_MAX - 1);
            strncat(tempbuf, alist->filenames_string_loclist[suggest_index], PATH_MAX - 1);
            strncpy(current, tempbuf, PATH_MAX - 1);
            //printf("tempbuf: %s\n", tempbuf);
        }
        //snprintf(current, PATH_MAX - 1, "%s/%s", 
        //        tempbuf, alist->filenames_string_loclist[suggestion_index]);
    }
#endif
}

static void handle_sort_hotkey(Runtime_Vars *rtvars, File_List *list)
{
    State_Flags *sflags = &rtvars->sflags;

    char *searchbuf = (char *)rtvars->bufgroup_ptr->inbuf_search;
    char *sort_text = (char *)rtvars->bufgroup_ptr->sort_text;

    char info[USERINFO_BUFFER_SIZE];
    cycle_sort_state(&sflags->sort_state);
    switch (sflags->sort_state)
    {
    case SORT_STATE_ARTIST_ASCENDING:
    {
        sort_list_by_artist(list, 0);
        snprintf(info, USERINFO_BUFFER_SIZE, "[sort]: artist name ascending");
    } break;

    case SORT_STATE_ARTIST_DESCENDING:
    {
        sort_list_by_artist(list, 1);
        snprintf(info, USERINFO_BUFFER_SIZE, "[sort]: artist name descending");
    } break;

    case SORT_STATE_TITLE_ASCENDING:
    {
        sort_list_by_title(list, 0);
        snprintf(info, USERINFO_BUFFER_SIZE, "[sort]: title ascending");
    } break;

    case SORT_STATE_TITLE_DESCENDING:
    {
        sort_list_by_title(list, 1);
        snprintf(info, USERINFO_BUFFER_SIZE, "[sort]: title descending");
    } break;

    case SORT_STATE_ALBUM_ASCENDING:
    {
        sort_list_by_album(list, 0);
        snprintf(info, USERINFO_BUFFER_SIZE, "[sort]: album name ascending");
    } break;

    case SORT_STATE_ALBUM_DESCENDING:
    {
        sort_list_by_album(list, 1);
        snprintf(info, USERINFO_BUFFER_SIZE, "[sort]: album name descending");
    } break;

    case SORT_STATE_MOD_DATE_ASCENDING:
    {
        sort_list_by_mod_date(list, 0);
        snprintf(info, USERINFO_BUFFER_SIZE, "[sort]: file modification date ascending");
    } break;

    case SORT_STATE_MOD_DATE_DESCENDING:
    {
        sort_list_by_mod_date(list, 1);
        snprintf(info, USERINFO_BUFFER_SIZE, "[sort]: file modification date descending");
    } break;
    default: break;
    }

    if (searchbuf[0]) { set_match_flags(searchbuf, rtvars->mdata_ptr); }
    set_userinfo(rtvars, info, USERINFO_TYPE_NOTE);
}

static void list_reload_metadata(Runtime_Vars *rtvars)
{
    File_List *list = &rtvars->mdata_ptr->music_list;
    char **dirs = list->dirnames_string_loclist;

    for (int clear_index = 1;
        clear_index < list->entry_count;
        ++clear_index)
    {
        list->file_strings[clear_index].metadata.artist = 0;
        list->file_strings[clear_index].metadata.title = 0;
        list->file_strings[clear_index].metadata.album = 0;
    }

    memset(list->titles_buf, 0, METADATA_STRINGS_BUFSIZE);
    memset(list->artist_names_buf, 0, METADATA_STRINGS_BUFSIZE);
    memset(list->album_names_buf, 0, METADATA_STRINGS_BUFSIZE);

    for (int dir_index = 0;
        dir_index < list->dirs_added;
        ++dir_index)
    {
        while (rtvars->sflags.metadata_getter_thread_working)
        { platform_sleep_ms(1); }
        platform_get_metadata_bulk(rtvars, dirs[dir_index]);
    }
}

static void menu_do_menubar(Runtime_Vars *rtvars, Music_Data *mdata)
{
    State_Flags *sflags = &rtvars->sflags;
    General_Buffer_Group *bufgroup = rtvars->bufgroup_ptr;
    const uint8_t *kbd = rtvars->kbd_state;
    char *sort_text = bufgroup->sort_text;
    char *ls_toggle_text = bufgroup->ls_toggle_text;
    char *play_toggle_text = bufgroup->play_toggle_text;
    char *shuf_toggle_text = bufgroup->shuf_toggle_text;
    char *repeat_toggle_text = bufgroup->loop_toggle_text;
    char *vis_toggle_text = bufgroup->vis_toggle_text;
    char *searchbuf = (char *)bufgroup->inbuf_search;
    char ctrl = kbd[SDL_SCANCODE_LCTRL];
    char shift = kbd[SDL_SCANCODE_LSHIFT];
    Center_View_State *vs = &rtvars->sflags.viewstate;
    File_List *list = &mdata->music_list;
    Keybinds *keys = &rtvars->keybinds;

    if (*vs == CENTER_VIEW_STATE_MUSIC_LIST)
    {
        strcpy(ls_toggle_text, "hide list"); 
    }
    else if (*vs == CENTER_VIEW_STATE_CURRENT_INFO)
    {
        strcpy(ls_toggle_text, "show list");
    }

    char sort_was_pressed = ImGui::Shortcut(keys->cycle_sort, ImGuiInputFlags_RouteGlobal);
    char clear_was_pressed = ImGui::Shortcut(keys->clear_list, ImGuiInputFlags_RouteGlobal);

    char lstoggle_was_pressed = ImGui::Shortcut(keys->toggle_list_vis, ImGuiInputFlags_RouteGlobal);
    char reptoggle_was_pressed = ImGui::Shortcut(keys->toggle_repeat, ImGuiInputFlags_RouteGlobal);
    char reload_metadata = ImGui::Shortcut(keys->reload_metadata, ImGuiInputFlags_RouteGlobal);
    char reload_config = ImGui::Shortcut(keys->reload_config, ImGuiInputFlags_RouteGlobal);

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("list"))
        {
            if (ImGui::BeginMenu("sort"))
            {
                ImGui::Text("use ctrl-shift-s to cycle sorting");
                float width = ImGui::CalcTextSize("modification date").x + 40.0f;
                ImGui::Text("artist");
                ImGui::SameLine(width - ImGui::GetCursorPosX());
                if (ImGui::Button("ascending##artist ascending"))
                {
                    sflags->sort_state = SORT_STATE_ARTIST_ASCENDING;
                    sort_list_by_artist(list, 0);
                }
                ImGui::SameLine();
                if (ImGui::Button("descending##artist descending"))
                {
                    sflags->sort_state = SORT_STATE_ARTIST_DESCENDING;
                    sort_list_by_artist(list, 1);
                }

                ImGui::Text("title");
                ImGui::SameLine(width - ImGui::GetCursorPosX());
                if (ImGui::Button("ascending##title ascending"))
                {
                    sflags->sort_state = SORT_STATE_TITLE_ASCENDING;
                    sort_list_by_title(list, 0);
                }
                ImGui::SameLine();
                if (ImGui::Button("descending##title descending"))
                {
                    sflags->sort_state = SORT_STATE_TITLE_DESCENDING;
                    sort_list_by_title(list, 1);
                }

                ImGui::Text("album");
                ImGui::SameLine(width - ImGui::GetCursorPosX());
                if (ImGui::Button("ascending##album ascending"))
                {
                    sflags->sort_state = SORT_STATE_ALBUM_ASCENDING;
                    sort_list_by_album(list, 0);
                }
                ImGui::SameLine();
                if (ImGui::Button("descending##album descending"))
                {
                    sflags->sort_state = SORT_STATE_ALBUM_DESCENDING;
                    sort_list_by_album(list, 1);
                }

                ImGui::Text("modification date");
                ImGui::SameLine(width - ImGui::GetCursorPosX());
                if (ImGui::Button("ascending##mod date ascending"))
                {
                    sflags->sort_state = SORT_STATE_MOD_DATE_ASCENDING;
                    sort_list_by_mod_date(list, 0);
                }
                ImGui::SameLine();
                if (ImGui::Button("descending##mod date descending"))
                {
                    sflags->sort_state = SORT_STATE_MOD_DATE_DESCENDING;
                    sort_list_by_mod_date(list, 1);
                }

                ImGui::EndMenu();
            }

            if (ImGui::MenuItem("clear", "ctrl-shift-x"))
            {
                clear_was_pressed = true;
            }
            if (ImGui::MenuItem(ls_toggle_text, "ctrl-l"))
            {
                lstoggle_was_pressed = true;
            }
            if (ImGui::MenuItem("search", "ctrl-f"))
            {
                sflags->searchwindow_open = true;
            }
            if (ImGui::BeginMenu("information"))
            {
                ImGui::Text("files added: %d\n"
                            "directories added: %d\n",
                            list->entry_count,
                            list->dirs_added);
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("reload metadata", "ctrl-shift-m"))
            {
                reload_metadata = true;
            }

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("playback"))
        {
            if (ImGui::MenuItem(play_toggle_text, "space"))
            {
                if (pacmxr_file_is_open())
                {
                    pacmxr_toggle_playback();
                    //mdata->paused = !mdata->paused;
                    //pacmxr_pause(mdata->paused);
                }
            }
            if (ImGui::MenuItem("halt"))
            {
                if (pacmxr_file_is_open()) { pacmxr_close_file(); }
            }
            if (ImGui::MenuItem("previous", "ctrl-p"))
            { goto_prev_file(mdata); }
            if (ImGui::MenuItem("next", "ctrl-n"))
            { goto_next_file(mdata); }
            if (ImGui::MenuItem(shuf_toggle_text, "ctrl-shift-r"))
            { mdata->shuffle_enabled = !mdata->shuffle_enabled; }
            if (ImGui::MenuItem(repeat_toggle_text, "ctrl-shift-l"))
            { reptoggle_was_pressed = 1; }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("settings"))
        {
            if (ImGui::MenuItem(vis_toggle_text))
            {
                sflags->visualizer_enabled = !sflags->visualizer_enabled;
                if (sflags->visualizer_enabled)
                {
                    strcpy(vis_toggle_text, "disable visualizer");
                }
                else
                {
                    strcpy(vis_toggle_text, "enable visualizer");
                }
            }
            if (ImGui::MenuItem("visualizer color"))
            {
                sflags->colorpicker_open = !sflags->colorpicker_open;
                if (sflags->colorpicker_open)
                {
                    ImVec2 wdim = ImVec2(400, 400);
                    ImVec2 wpos = ImVec2((rtvars->sdldata_ptr->win_width/2) - (wdim.x/2), 100.0f);
                    ImGui::SetNextWindowSize(wdim);
                    ImGui::SetNextWindowPos(wpos);
                }
            }
            if (ImGui::MenuItem("reload configuration", "ctrl-shift-g"))
            {
                reload_config = true;
            }

            ImGui::EndMenu();
        }

        set_userinfo_color(rtvars);
        ImGui::SetCursorPosX(rtvars->sdldata_ptr->win_width - 
                (ImGui::CalcTextSize((char *)bufgroup->userinfo_buffer).x) - 5);
        ImGui::Text("%s", (char *)bufgroup->userinfo_buffer);
        ImGui::PopStyleColor();
        ImGui::EndMenuBar();
    } 

    if (sort_was_pressed)
    {
        if (!sflags->metadata_getter_thread_working)
        {
            handle_sort_hotkey(rtvars, list);
        }
        else
        {
            char info[USERINFO_BUFFER_SIZE] = "[sort]: list cannot be sorted during metadata retrieval.";
            set_userinfo(rtvars, info, USERINFO_TYPE_WARNING);
        }
    }

    if (clear_was_pressed)
    { clear_file_list(mdata); }

    if (lstoggle_was_pressed)
    {
        do
        {
            cycle_center_view_state(vs); 
        } while ((*vs != CENTER_VIEW_STATE_MUSIC_LIST) &&
                (*vs != CENTER_VIEW_STATE_CURRENT_INFO));
    }

    if (reptoggle_was_pressed)
    {
        mdata->loop_enabled = !mdata->loop_enabled;
        if (mdata->loop_enabled)
        { strcpy(repeat_toggle_text, "disable looping"); }
        else
        { strcpy(repeat_toggle_text, "enable looping"); }
    }

    if (reload_metadata)
    { list_reload_metadata(rtvars); }

    if (reload_config)
    {
        platform_dbg_log("reloading config %s\n", rtvars->conf_directory);
        parse_and_apply_config(rtvars,
                (char *)rtvars->bufgroup_ptr->conf_file_buffer,
                CONFBUFFER_SIZE);
    }
}

static void menu_do_colorpicker(Runtime_Vars *rtvars)
{
    Sdl_Apidata *sdldata = rtvars->sdldata_ptr;
    State_Flags *sflags = &rtvars->sflags;

    if (ImGui::Begin("visualizer color", 0,
        ImGuiWindowFlags_NoScrollbar
        |ImGuiWindowFlags_NoResize
        |ImGuiWindowFlags_NoCollapse))
    {
        if (imgui_window_should_close())
        { sflags->colorpicker_open = 0; }

        ImGui::ColorPicker4("##visualizer_color",
                rtvars->uivars.vis_color,
                ImGuiColorEditFlags_Float
                |ImGuiColorEditFlags_DisplayHex
                |ImGuiColorEditFlags_DisplayRGB
                |ImGuiColorEditFlags_NoLabel
                |ImGuiColorEditFlags_AlphaBar);
        ImGui::End();
    }
}

static void pac_main_loop(Runtime_Vars *rtvars, 
                        Sdl_Apidata *sdldata, 
                        General_Buffer_Group *bufgroup,
                        Music_Data *mdata)
{
    PAC_LOCAL_STATIC char playback_btn_text[8] = {0};
    PAC_LOCAL_STATIC char shuffle_btn_text[8] = {0};
    PAC_LOCAL_STATIC char loop_btn_text[8] = {0};
    float btn_side_len = 30;
    float btn_height = btn_side_len + (ImGui::GetStyle().CellPadding.y*2.0f);

    float add_width = 100.0f + rtvars->sargs_ptr->font_size;
    float vol_width = 200.0f;
    State_Flags *sflags = &rtvars->sflags;
    Keybinds *keys = &rtvars->keybinds;

    if (pacmxr_file_is_open())
    { update_audio_time(mdata); }

    pac_begin_frame(rtvars, sdldata);

    menu_do_menubar(rtvars, mdata);
    if (sflags->colorpicker_open)
    { menu_do_colorpicker(rtvars); }

    ImGui::PushItemWidth(ImGui::GetColumnWidth(-1) - add_width);
    if (ImGui::Shortcut(ImGuiMod_Alt|ImGuiKey_D, ImGuiInputFlags_RouteGlobal))
    { ImGui::SetKeyboardFocusHere(0); }

    ImGui::Text("path:");
    ImGui::SameLine();
    //PAC_LOCAL_STATIC int _inpath_len = 0;

    if (ImGui::InputText("##in_path", 
            (char *)bufgroup->inbuf_filename, 
            PATH_MAX - 1))
    {
        //int new_inpath_len = strlen((char *)bufgroup->inbuf_filename);
        //if((new_inpath_len > _inpath_len) || tab_was_pressed) {
        //    do_path_autocomplete((char *)bufgroup->inbuf_filename, rtvars);
        //}
        //_inpath_len = new_inpath_len;
    }
    ImGui::PopItemWidth();

    ImGui::SameLine();
    if (ImGui::Button("add", ImVec2(ImGui::GetColumnWidth(-1), 0)))
    {
        add_to_music_list((char *)bufgroup->inbuf_filename, mdata, rtvars); 
    }

    ImGui::Separator();
    ImGui::SetCursorPosX(50);
    if (ImGui::Shortcut(ImGuiMod_Ctrl|ImGuiMod_Shift|ImGuiKey_Q,
            ImGuiInputFlags_RouteGlobal))
    {
        sflags->searchwindow_open = 0;
        ImGui::SetKeyboardFocusHere(0);
    }
    ImGui::BeginChild("##center_thing", 
            ImVec2(ImGui::GetColumnWidth(-1), 
            rtvars->sdldata_ptr->win_height - 
            ImGui::GetTextLineHeight() - 100),
            0);

    switch (rtvars->sflags.viewstate)
    {
    case CENTER_VIEW_STATE_MUSIC_LIST:
    {
        menu_do_music_list(rtvars, mdata);
    } break;

    case CENTER_VIEW_STATE_CURRENT_INFO:
    {
        menu_do_current_file_info(rtvars, mdata, bufgroup);
    } break;

    default: break;
    }

    ImGui::EndChild();
    ImGui::SetCursorPosX(50);
    ImGui::Separator();

    if (mdata->shuffle_enabled)
    {
        strcpy(shuffle_btn_text, "-");
        strcpy((char *)bufgroup->shuf_toggle_text, "disable shuffle");
    }
    else
    { 
        strcpy(shuffle_btn_text, "sh");
        strcpy((char *)bufgroup->shuf_toggle_text, "enable shuffle");
    }

    if (mdata->loop_enabled)
    {
        strcpy(loop_btn_text, "nl");
        strcpy((char *)bufgroup->loop_toggle_text, "disable looping");
    }
    else
    {
        strcpy(loop_btn_text, "lo");
        strcpy((char *)bufgroup->loop_toggle_text, "enable looping");
    }

    int buttons2draw = 6, pad = 5;
    ImGui::SetCursorPosY(sdldata->win_height - (btn_height*buttons2draw) - pad);

    if (ImGui::Button(loop_btn_text, ImVec2(btn_side_len, btn_side_len))) 
    { mdata->loop_enabled = !mdata->loop_enabled; }

    char shuf_toggle_pressed = ImGui::Shortcut(keys->toggle_shuffle,
                                    ImGuiInputFlags_RouteGlobal);
    if (ImGui::Button(shuffle_btn_text, ImVec2(btn_side_len, btn_side_len)) ||
            shuf_toggle_pressed)
    { mdata->shuffle_enabled = !mdata->shuffle_enabled; }

    //char n_was_pressed = pac_btn_press(SDL_SCANCODE_N, 
    //                        &rtvars->sflags.n_wasdown, 
    //                        rtvars->kbd_state);
    char n_was_pressed = ImGui::Shortcut(keys->go_next, ImGuiInputFlags_RouteGlobal);
    char p_was_pressed = ImGui::Shortcut(keys->go_prev, ImGuiInputFlags_RouteGlobal);

    if (ImGui::Button(">>", ImVec2(btn_side_len, btn_side_len)) || n_was_pressed) 
    { goto_next_file(mdata); } 
    if (ImGui::Button("<<", ImVec2(btn_side_len, btn_side_len)) || p_was_pressed)
    { goto_prev_file(mdata); }

    //halt
    if (ImGui::Button((char *)_stop_btn_glyph, ImVec2(btn_side_len, btn_side_len)))
    {
        if (pacmxr_file_is_open())
        { pacmxr_close_file(); }
    }

    if (rtvars->pacmxr_ctx->paused)
    {
        playback_btn_text[0] = '>'; 
        playback_btn_text[1] = 0; 
        strcpy((char *)bufgroup->play_toggle_text, "play");
    }
    else
    {
        playback_btn_text[0] = '|'; 
        playback_btn_text[1] = '|'; 
        playback_btn_text[2] = 0; 
        strcpy((char *)bufgroup->play_toggle_text, "pause");
    }

    char pause_was_pressed = ImGui::Shortcut(keys->pause) &&
                            !ImGui::GetIO().WantTextInput;

    if (ImGui::Button(playback_btn_text, ImVec2(btn_side_len, btn_side_len)) || 
        pause_was_pressed)
    {
        if (pacmxr_file_is_open())
        { pacmxr_toggle_playback(); }
    }

    menu_do_volume_bar(rtvars, mdata, vol_width);
    menu_do_seek_bar(rtvars, mdata);

#if 1
    char searchkey_was_pressed = ImGui::Shortcut(keys->search, ImGuiInputFlags_RouteGlobal);
    char esc_was_pressed = ImGui::IsKeyPressed(ImGuiKey_Escape);
    char enter_was_pressed = ImGui::IsKeyPressed(ImGuiKey_Enter);
    if (searchkey_was_pressed)
    {
        sflags->searchwindow_open = !sflags->searchwindow_open; 
        ImVec2 winsize = ImVec2(400, 70);
        ImGui::SetNextWindowSize(winsize);
        ImVec2 winpos = ImVec2(sdldata->win_width - winsize.x - 10, winsize.y);
        ImGui::SetNextWindowPos(winpos);
    }
    else if (esc_was_pressed || enter_was_pressed)
    {
        sflags->searchwindow_open = 0;
    }
#endif
    pac_end_frame(rtvars, sdldata);
}

static void tupacmixer_get_taginfo(Music_Data *mdata)
{
    if (!pacmxr_file_is_open()) { return; }
    Audio_Metadata_Group *amg = &mdata->current_metadata;
    int taginfo_len = 0;
    taginfo_len += pacmxr_meta_get_title(0, amg->tagbuffer, sizeof(amg->tagbuffer));
    if (taginfo_len)
    {
        taginfo_len += snprintf(amg->tagbuffer + taginfo_len,
                            sizeof(amg->tagbuffer) - taginfo_len,
                            "  -  ");
        taginfo_len += pacmxr_meta_get_artist(0,
                            amg->tagbuffer + taginfo_len,
                            META_EDITOR_BUFSIZE - taginfo_len);
        taginfo_len += snprintf(amg->tagbuffer + taginfo_len,
                            sizeof(amg->tagbuffer) - taginfo_len,
                            "  -  ");
        taginfo_len += pacmxr_meta_get_album(0,
                            amg->tagbuffer + taginfo_len,
                            META_EDITOR_BUFSIZE - taginfo_len);
    }
    else
    {
        File_List *mlist = &mdata->music_list;
        snprintf(amg->tagbuffer, sizeof(amg->tagbuffer),
                "%s", mlist->file_strings[mlist->current_index].filename);
    }
}

static void tupacmixer_start_music(Music_Data *mdata, char *music_path)
{
    pacmxr_close_file();
    if (pacmxr_open_file(music_path))
    {
        pacmxr_pause(0);
        //prev_file_list_push(mdata, music_path);
        tupacmixer_get_audio_type(mdata);
        tupacmixer_get_cover(&mdata->cover, mdata->rtvars_ptr);
        //SDL_UnlockAudioDevice(global_pacmxr_ctx.au_dev);
    }
    else
    {
        platform_dbg_log("failed to play file \n");
        char info[USERINFO_BUFFER_SIZE];
        snprintf(info, USERINFO_BUFFER_SIZE - 1,
                "[error]: failed to play file %s, skipping.", //NOTE: this will (almost) never show up since it just gets overwritten
                mdata->music_list.file_strings[mdata->music_list.current_index].filename);
        set_userinfo(mdata->rtvars_ptr, info, USERINFO_TYPE_ERROR);
        //goto_next_file(mdata);
    }
}

static void tupacmixer_stop_music(Music_Data *mdata)
{
    pacmxr_close_file();
}

void pac_sdlmixer_music_finished_callback(void) {}

static char pac_init_tupacmixer(Music_Data *mdata)
{
    Pacmxr_Init_Options init_opts = {};
    init_opts.sample_rate = PACMXR_RESAMPLE_SAMPLERATE;
    init_opts.no_init_sdl = 1;
#if _2PACWAV_DEBUG
    //init_opts.av_loglevel = AV_LOG_DEBUG;
    init_opts.av_loglevel = AV_LOG_ERROR;
#else
    init_opts.av_loglevel = AV_LOG_ERROR;
#endif
    if (!pacmxr_init(&init_opts))
    {
        platform_log("FATAL ERROR: 2pacwav failed to initialize its audio mixer\n");
        return 0;
    }

    Pacmxr_Context *pacmxr_context = pacmxr_get_context();
    SDL_AudioSpec wanted_spec, obtained_spec;
    SDL_zero(wanted_spec);
    wanted_spec.freq = PACMXR_RESAMPLE_SAMPLERATE;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.channels = 2;
    wanted_spec.samples = PACMXR_SDL_AUDIO_BUFFER_SIZE;
    wanted_spec.callback = pacmxr__sdl_audio_callback;
    wanted_spec.userdata = (void *)pacmxr_context;

    pacmxr_context->au_dev = SDL_OpenAudioDevice(0, 0, &wanted_spec, &obtained_spec, 0);
    if (!pacmxr_context->au_dev)
    {
        platform_log("pacmxr_init failed to open audio device: %s\n", SDL_GetError());
        return 0;
    }
    SDL_PauseAudioDevice(pacmxr_context->au_dev, 1);
    SDL_LockAudioDevice(pacmxr_context->au_dev);
    //rtvars->pacmxr_ctx->paused = 1;
    mdata->sample_rate = obtained_spec.freq;
    mdata->pcm_bits = obtained_spec.format & 0xFF;
    mdata->channels = obtained_spec.channels;
    mdata->volume = SDL_MIX_MAXVOLUME/2;
    mdata->chunk_size = 8192;
    mdata->seek_increment = PAC_DEFAULT_SEEK_INCREMENT;
    //pacmxr_pause(1);
    return 1;
}

static char pac_init_sdl(Sdl_Apidata *sdldata)
{
    char result = 0;
    if (!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO))
    {
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

        char wintitle[128];
        get_version_string(wintitle);

        SDL_SetHintWithPriority(SDL_HINT_APP_NAME, wintitle, SDL_HINT_OVERRIDE);
        SDL_SetHintWithPriority(SDL_HINT_AUDIO_DEVICE_APP_NAME, wintitle, SDL_HINT_OVERRIDE);
        SDL_SetHintWithPriority(SDL_HINT_AUDIO_DEVICE_STREAM_NAME, "audio stream", SDL_HINT_OVERRIDE);
        SDL_SetHintWithPriority(SDL_HINT_AUDIO_DEVICE_STREAM_ROLE, "music player", SDL_HINT_OVERRIDE);

        sdldata->window_ptr = SDL_CreateWindow(wintitle,
                                    SDL_WINDOWPOS_UNDEFINED, 
                                    SDL_WINDOWPOS_UNDEFINED, 
                                    WINDOW_WIDTH, 
                                    WINDOW_HEIGHT, 
                                    SDL_WINDOW_RESIZABLE|SDL_WINDOW_OPENGL);
        if (sdldata->window_ptr)
        {
            sdldata->ogl_context = SDL_GL_CreateContext(sdldata->window_ptr);
            if (sdldata->ogl_context)
            {
                result = 1; 
            }
            else
            {
                fprintf(stderr, "SDL failed to create OpenGL context\n"); 
            }
        }
        else
        {
            fprintf(stderr, "SDL failed to create window\n"); 
        }
    }
    else
    {
        fprintf(stderr, "SDL failed to init\n"); 
    }
    return result;
}
