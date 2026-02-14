
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
#include "2pacwav2_confparser.cpp"
#include "2pacwav2_playlist.cpp"

//#include "2pacmixer.h"

PAC_INTERNAL void pac_nop()
{
    return; 
}

PAC_INTERNAL void get_version_string(char *buffer)
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

PAC_INTERNAL void show_version()
{
    char verbuf[128];
    get_version_string(verbuf);
    int len = strlen(verbuf);
    char *ver_end = verbuf + len;
    sprintf(ver_end, ", compiled %s @ %s", __DATE__, __TIME__);
    platform_log("%s\n", verbuf);
}

PAC_INTERNAL void show_help(char longhelp)
{
    show_version();
    platform_log("usage: 2w [options] [files]\n"
            "\n-- COMMAND LINE OPTIONS --\n\n"
            "-h | --help : print this message and exit\n"
            "--longhelp : print output of the above option as well as additional documentation\n"
            "-v | --version : print version and exit\n"
            "-vol <value> : set the volume\n"
            "-conf <path> : specify config file\n"
            "-noconf : do not look for a configuration file\n"
            "-nosup : if the configuration file has instances of startup_path, skip adding them to the playlist on startup\n"
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
You can copy this section in its entirety into your 2wconf, but some of the example
values must be changed. 
---
*/
startup_path = "/path/to/file"; //Sets a path to a file or folder that will be added to the list on startup.
                                //Note that multiple instances of this are perfectly valid.
volume = value; //Sets the volume level on startup. The value is clamped to 0-128
                //The -vol command option overrides this
font_path = "/path/to/font/file"; //Path to the desired font file (TrueType or OpenType)
font_size = value; //Sets point size for the font. (default: %g)
visualizer = 0 or 1; //Disables visualizer if value is 0 and enables it otherwise,
                                  //including when this variable isn't set.
                                  //Note that this can be re-enabled from the view menu at any time.
visualizer_color = {r, g, b, a}; //Sets the color that the visualizer is set to at startup.
                                 //Values inside {} must be floats between 0.0 and 1.0
                                 //Note: the visualizer color can be changed at any time from the settings menu.
)"
                , PAC_LATIN_FONTSIZE);
}

PAC_INTERNAL void startup_push_path(Startup_Args *sargs, char *path)
{
    Startup_Args_Paths *p = &sargs->paths;
    if (p->count < PAC_MAX_DIRS) {
        int index = p->count;
        char *dest = p->ptrs[index];
        if (index) {
            dest = p->ptrs[index - 1];
            dest += strlen(dest);
            *dest++ = 0;
            p->ptrs[index] = dest; 
        }
        snprintf(dest, PATH_MAX, "%s", path);
        ++p->count; 
    }
}

PAC_INTERNAL char pac_do_command_args(int arg_count, 
                                    char **args, 
                                    Startup_Args *sargs,
                                    General_Buffer_Group *bufgroup)
{
    sargs->volume = -1;
    char *arg;
    char exit_after_ret = 0;
    for (int arg_index = 1; 
        arg_index < arg_count; 
        ++arg_index) {
        arg = args[arg_index];
        if (!strcmp("--", arg)) { 
            break; 
        } else if (!strcmp("-v", arg) || !strcmp("--version", arg)) {
            exit_after_ret = 1; 
            show_version(); 
            break; 
        } else if (!strcmp("-h", arg) || !strcmp("--help", arg)) {
            exit_after_ret = 1;
            show_help(0);
            break;
        } else if (!strcmp("--longhelp", arg)) {
            exit_after_ret = 1;
            show_help(1);
            break;
        } else if (!sargs->no_load_conf && !strcmp("-noconf", arg)) {
            sargs->no_load_conf = 1;
        } else if (!sargs->no_load_startup_paths && !strcmp("-nosup", arg)) {
            sargs->no_load_startup_paths = 1;
        } else if ((sargs->font_size == PAC_LATIN_FONTSIZE) &&
            !strcmp("-font_size", arg)) {
            if (args[arg_index + 1]) {
                float value = strtof(args[arg_index + 1], 0);
                if (value != 0.0f) {
                    sargs->font_size = value;
                } else {
                    fprintf(stderr, "invalid argument given after %s, ignoring.\n", arg);
                }
            } else {
                fprintf(stderr, "no argument given after %s, ignoring.\n", arg);
            }
            ++arg_index;
        } else if ((-1 == sargs->volume) && !strcmp("-vol", arg)) {
            if (args[arg_index + 1]) {
                errno = 0;
                int value = strtol(args[arg_index + 1], 0, 10);
                if (errno != ERANGE) {
                    sargs->volume = value;
                } else {
                    fprintf(stderr, "invalid argument given after %s, ignoring.\n", arg);
                }
                ++arg_index;
            } else {
                fprintf(stderr, "no argument given after %s, ignoring.\n", arg);
            }
        } else if (!sargs->conf_path && !strcmp("-conf", arg)) {
            if (args[arg_index + 1]) {
                sargs->conf_path = args[arg_index + 1];
            } else {
                fprintf(stderr, "no argument given after %s, ignoring.\n", arg);
            }
            ++arg_index;
        } else {
            if (platform_path_exists(arg)) {
                startup_push_path(sargs, arg);
            } else {
                platform_log("unrecognized option: %s\n", arg);
                exit_after_ret = 1;
            }
        }
    }

    return exit_after_ret;
}

PAC_INTERNAL size_t startup_load_conf(Runtime_Vars *rtvars, 
                                    char *confbuf, 
                                    int confbuf_bytes)
{
    char confpath[PATH_MAX]; confpath[0] = 0;
    const int infosize = PATH_MAX + sizeof("loaded config: ");
    char infobuf[infosize];
    size_t bytes_read = 0;

#if _2PACWAV_LINUX
    if (rtvars->sargs_ptr->conf_path) {
        snprintf(confpath, PATH_MAX, "%s", rtvars->sargs_ptr->conf_path);
    } else {
        snprintf(confpath, PATH_MAX, "%s/%s", 
                rtvars->working_directory, PAC_CONFNAME_STRING);
    }
#elif _2PACWAV_WIN32
    snprintf(confpath, PATH_MAX - 1, "%s\\%s", 
            rtvars->working_directory, PAC_CONFNAME_STRING);
#endif

    if (platform_file_exists(confpath)) {
        snprintf(rtvars->conf_directory, PATH_MAX, "%s", confpath);
        bytes_read = platform_read_file(confpath, confbuf, confbuf_bytes - 1);
        confbuf[bytes_read] = 0;
        snprintf(infobuf, infosize, "loaded config: %s", confpath);
        set_userinfo(rtvars, infobuf, USERINFO_TYPE_NOTE);
    } else {
#if _2PACWAV_LINUX
        char *username = getlogin();
        if (username) {
            snprintf(confpath, PATH_MAX,
                    "/home/%s/.config/2pacwav/%s",
                    username, PAC_CONFNAME_STRING);

            if (platform_file_exists(confpath)) {
                snprintf(rtvars->conf_directory, PATH_MAX, "%s", confpath);
                bytes_read = platform_read_file(confpath, confbuf, confbuf_bytes - 1);
                confbuf[bytes_read] = 0;
                snprintf(infobuf, infosize, "loaded config: %s", confpath);
                set_userinfo(rtvars, infobuf, USERINFO_TYPE_NOTE);
            }
        }
#endif
    }

    if (confpath[0]) {
        platform_dbg_log("config path: %s\n", confpath);
    }

    return bytes_read;
}

PAC_INTERNAL void startup_add_paths(Startup_Args *sargs,
                                Runtime_Vars *rtvars, 
                                Music_Data *mdata) 
{
    char *this_path, *this_end;
    for (int i = 0; i < sargs->paths.count; ++i) {
        this_path = sargs->paths.ptrs[i];
        add_to_music_list(this_path, mdata, rtvars); 
    }
}

PAC_INTERNAL char pac_mousebtn_press(Mouse_State *mouse)
{
    char result = 0;
    if (mouse->down) {
        if (!mouse->wasdown_flags[mouse->down - 1]) {
            mouse->wasdown_flags[mouse->down - 1] = 1;
            result = 1; 
        }
    } else { 
        *(int32_t *)mouse->wasdown_flags = 0; 
    }
    return result;
}

PAC_INTERNAL char pac_btn_press(SDL_Scancode scan, 
                            char *wasdown, 
                            const uint8_t *kbd_state) 
{
    char state = 0;
    if (kbd_state[scan]) {
        if (!*wasdown) {
            state = 1; 
            *wasdown = 1; 
        }
    } else { 
        *wasdown = 0; 
    }
    return state;
}

PAC_INTERNAL char pac_imgui_load_font(char *font_path,
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
        platform_file_exists(cjk_path)) {
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

PAC_INTERNAL void sdlapi_process_events(Runtime_Vars *rtvars, Sdl_Apidata *sdldata) 
{
    SDL_Event event;
    rtvars->kbd_state = SDL_GetKeyboardState(0);
    char char_was_pressed = 0;
    Mouse_State *mouse = &rtvars->sflags.mouse;

    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);
        switch(event.type) {
        case SDL_QUIT: { 
            rtvars->keep_running = 0; 
        } break;

        case SDL_DROPFILE: {
            strncpy((char *)rtvars->bufgroup_ptr->inbuf_filename, 
                    event.drop.file, 
                    PATH_MAX);
        } break;

        case SDL_MOUSEBUTTONDOWN: {
            mouse->down = event.button.button;
            mouse->pos.x = event.button.x;
            mouse->pos.y = event.button.y;
        } break;

        case SDL_KEYDOWN: break;

        default: {
            mouse->down = 0;
        } break;
        }
    }
}

PAC_INTERNAL void sdlapi_correct_gl_viewport_and_clear(Sdl_Apidata *sdldata)
{
    SDL_GetWindowSize(sdldata->window_ptr, &sdldata->win_width, &sdldata->win_height);
    glViewport(0, 0, sdldata->win_width, sdldata->win_height);
    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(0, 0, 0, 1.0f);
}

PAC_INTERNAL void pac_begin_frame(Runtime_Vars *rtvars, Sdl_Apidata *sdldata) 
{
    sdlapi_correct_gl_viewport_and_clear(sdldata);
    sdlapi_process_events(rtvars, sdldata);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::SetNextWindowBgAlpha(0);

    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(    0x22, 0x22, 0x33, 0xFF));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(   0x22, 0x22, 0x33, 0xFF));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(  0x0,  0x00, 0x00, 0xFF));
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, IM_COL32( 0x22, 0x22, 0x33, 0xFF));

    ImGui::Begin("2PACWAV", 0, ImGuiWindowFlags_NoTitleBar
                            |ImGuiWindowFlags_NoResize
                            |ImGuiWindowFlags_NoMove
                            |ImGuiWindowFlags_NoScrollbar
                            |ImGuiWindowFlags_NoSavedSettings
                            |ImGuiWindowFlags_NoDecoration
                            |ImGuiWindowFlags_MenuBar);

    ImGui::SetShortcutRouting(ImGuiMod_Ctrl|ImGuiKey_Tab, 0, ImGuiButtonFlags_NoSetKeyOwner);
    ImGui::SetShortcutRouting(ImGuiMod_Ctrl|ImGuiMod_Shift|ImGuiKey_Tab, 0, ImGuiButtonFlags_NoSetKeyOwner);
}

PAC_INTERNAL void pac_end_frame(Runtime_Vars *rtvars, Sdl_Apidata *sdldata) 
{
    State_Flags *sflags = &rtvars->sflags;
    General_Buffer_Group *bufgroup = rtvars->bufgroup_ptr;
    Music_Data *mdata = rtvars->mdata_ptr;

    ImGui::PopStyleColor();
    ImGui::PopStyleColor();
    ImGui::PopStyleColor();

    ImGui::End();

    if (sflags->searchwindow_open)
    { menu_do_search(rtvars, bufgroup, mdata); }

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

PAC_INTERNAL void update_audio_time(Music_Data *mdata)
{
    mdata->current_duration = pacmxr_stream_duration();
    mdata->current_position = pacmxr_seconds_played();
    Pacmxr_Context *pac_ctx = pacmxr_get_context();
    if ((!pac_ctx->paused && pacmxr_file_is_open()) && 
        ((mdata->current_position + 0.1) >= mdata->current_duration))
    { goto_next_file(mdata); }
}

PAC_INTERNAL int pac_qsort_strcmp(const void *a, const void *b)
{
    int result = strcasecmp(*(const char **)a, *(const char **)b);
    return result;
}

PAC_INTERNAL int pac_qsort_strcmp_rev(const void *a, const void *b)
{
    int result = strcasecmp(*(const char **)b, *(const char **)a);
    return result;
}

typedef int (*Sort_Comp_Func)(const void *a, const void *b);

PAC_INTERNAL void sort_file_list_alpha(File_List *flist, char reversed)
{
    char **strings = flist->filenames_string_loclist;
    int sort_count = flist->entry_count;
    Sort_Comp_Func cmpf = pac_qsort_strcmp;
    if (reversed) { cmpf = pac_qsort_strcmp_rev; }
    qsort(strings, sort_count, sizeof(char **), cmpf);
}

PAC_INTERNAL void update_info_buffer(Music_Data *mdata, General_Buffer_Group *bufgroup)
{
    snprintf((char *)bufgroup->music_info_buffer, DEBUG_BUFFER_SIZE,
            "[srate:%dhz][pcm_bits:%d][chan:%d]"
            "[vol:%d/128][pos:%06.1f/%06.1f][enc:%s]"
            "\n[path:%s]", 
            mdata->sample_rate, mdata->pcm_bits, mdata->channels,
            mdata->volume, mdata->current_position, mdata->current_duration, mdata->music_type_buf,
            mdata->current_filename);
}

PAC_INTERNAL void tupacmixer_get_audio_type(Music_Data *mdata)
{
    Pacmxr_Context *pac_ctx = pacmxr_get_context();
    if (pac_ctx->fctx.codec && pac_ctx->fctx.codec->name) {
        snprintf(mdata->music_type_buf,
                sizeof(mdata->music_type_buf), "%s",
                pac_ctx->fctx.codec->name);
    } else {
        snprintf(mdata->music_type_buf, sizeof(mdata->music_type_buf), "unknown");
    }
}

PAC_INTERNAL void load_file_from_path(char *path, Music_Data *mdata)
{
    if (platform_file_exists(path)) {
        tupacmixer_start_music(mdata, path);
    } else { 
        platform_dbg_log("%s: no such file or directory\n", path); 
        set_userinfo(mdata->rtvars_ptr, 
                "[error]: attempted to play file which doesn't exist.",
                USERINFO_TYPE_ERROR);
    }
}

PAC_INTERNAL char *separate_file_and_dir_name(char *dir_in_out, 
                                            char *name_out,
                                            int dirlen)
{
    char *temp_in = dir_in_out + dirlen;
    int chars = 0;
    if (strchr(dir_in_out, '/')) {
        while (temp_in != dir_in_out) {
            if (*temp_in == '/') {
                *temp_in = 0x0;
                snprintf(name_out, chars, "%s", temp_in + 1);
                break;
            }
            --temp_in;
            ++chars;
        }
    } else {
        snprintf(name_out, dirlen + 1, "%s", dir_in_out);
        snprintf(dir_in_out, dirlen, ".");
    }
    return dir_in_out;
}

PAC_INTERNAL char check_dir_already_added(char *dir, File_List *flist)
{
    char result = 0, *current_dir;
    char **all_dirs = flist->dirnames_string_loclist;
    int dir_count = flist->dirs_added;
    for (int dir_index = 0; dir_index < dir_count; ++dir_index) {
        current_dir = all_dirs[dir_index];
        if (!strcmp(current_dir, dir)) {
            result = 1; 
            break; 
        }
    }
    return result;
}

PAC_INTERNAL float conv_slide_value2songpos(Music_Data *mdata) 
{
    float result = 0;
    if (pacmxr_file_is_open() &&
        mdata->current_position &&
        mdata->current_duration) {
        result = (((double)(mdata->seek_value)) /
                PAC_SEEK_VALUE_MAX) *
                mdata->current_duration;
    }
    return result;
}

PAC_INTERNAL float conv_songpos2slide_value(Music_Data *mdata) 
{
    float result = 0.0f;
    if (mdata->current_position &&
        mdata->current_duration) {
        result = ((float)(mdata->current_position / 
                mdata->current_duration)) * 
                PAC_SEEK_VALUE_MAX; 
    }
    return result;
}

PAC_INTERNAL void file_list_push_dirname(char *dirname, File_List *flist)
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

PAC_INTERNAL char add_single_file_to_music_list(char *path, Music_Data *mdata)
{
    char dir[PATH_MAX];
    char name[NAME_MAX];
    snprintf(dir, PATH_MAX, "%s", path);
    separate_file_and_dir_name(dir, name, strlen(dir));
    File_List *mlist = &mdata->music_list;
    char cont_dir_already_added = check_dir_already_added(path, &mdata->music_list);

    char *file_entry = mlist->filenames_string_loclist[mlist->entry_count];
    strncpy(file_entry, name, NAME_MAX - 1);
    int file_len = strlen(name);
    file_entry[file_len + 1] = 0x0;

    if (!cont_dir_already_added) 
    { file_list_push_dirname(dir, mlist); } 

    mlist->filenames_string_loclist[mlist->entry_count + 1] = file_entry + file_len + 1;
    ++mdata->music_list.entry_count;
    return cont_dir_already_added;
}

PAC_INLINE char file_is_playlist(char *path)
{
    char result = 0;
    int path_len = strlen(path);
    char *extension = path + (1 + path_len - sizeof(PLAYLIST_EXTENSION));
    if (!strcmp(extension, PLAYLIST_EXTENSION)) { result = 1; }
    return result;
}

PAC_INTERNAL void add_to_music_list(char *path, Music_Data *mdata, Runtime_Vars *rtvars)
{
    int old_file_count = mdata->music_list.entry_count, 
            new_file_count, 
            added_files;

    if (platform_file_exists(path)) {
        if (file_is_playlist(path)) {
            process_playlist(rtvars, path);
        } else {
            add_single_file_to_music_list(path, mdata); 
        }
    } else if (platform_directory_exists(path)) {
        int pathlen = strlen(path);
        
#if _2PACWAV_LINUX
        while(path[--pathlen] == '/')
#elif _2PACWAV_WIN32
        while(path[--pathlen] == '\\')
#endif
        { path[pathlen] = 0; }
        platform_list_files_mlist(path, &mdata->music_list); 
    } else { 
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

PAC_INTERNAL char *find_top_level_path4file(char *filename, File_List *flist) 
{
    char *result = 0, *dirname;
    char pathbuf[PATH_MAX];
    for (int dir_index = 0; 
        dir_index < flist->dirs_added; 
        ++dir_index) {
        dirname = flist->dirnames_string_loclist[dir_index];
        snprintf(pathbuf, PATH_MAX - 1, "%s/%s", dirname, filename);
        if (platform_file_exists(pathbuf)) {
            result = dirname; 
            break; 
        }
    }
    return result;
}

PAC_INTERNAL void file_list_play_file(char *selected_file, 
                                    uint32_t file_index, 
                                    Music_Data *mdata)
{
    char path_buf[PATH_MAX];
    char *toplevel_path = find_top_level_path4file(selected_file, &mdata->music_list);
    if (toplevel_path) {
        snprintf(path_buf, PATH_MAX - 1, "%s/%s", toplevel_path, selected_file);
        //platform_dbg_log("trying to play %s\n", path_buf);

        char info_buf[USERINFO_BUFFER_SIZE];
        snprintf(info_buf, USERINFO_BUFFER_SIZE - 1, "[info]: playing %s", selected_file);
        set_userinfo(mdata->rtvars_ptr, info_buf, USERINFO_TYPE_NOTE);

        strncpy(mdata->current_filename, path_buf, PATH_MAX - 1);
        mdata->music_list.current_index = file_index;
        load_file_from_path(path_buf, mdata);
    } else {
        platform_dbg_log("could not find containing directory for file %s.\n", selected_file); 
        char info[USERINFO_BUFFER_SIZE];
        snprintf(info, USERINFO_BUFFER_SIZE - 1,
                "could not find containing directory for file %s.", 
                selected_file);
        set_userinfo(mdata->rtvars_ptr, info,USERINFO_TYPE_ERROR);
    }
}

PAC_INTERNAL int get_random_file_index(Music_Data *mdata) 
{
    File_List *mlist = &mdata->music_list;
    int cur_index = (int)mlist->current_index;
    int next_index = cur_index;
    while (next_index == cur_index) 
    { next_index = ((double)(mlist->entry_count) - 1.0)*drand48(); }
    return next_index;
}

PAC_INTERNAL void goto_next_file(Music_Data *mdata)
{
    if (mdata->music_list.entry_count < 1)
    { return; }

    File_List *mlist = &mdata->music_list;
    uint32_t cur_index = mlist->current_index;
    mdata->previous_index = cur_index;
    char *next_file;
    if (!mdata->loop_enabled) {
        if (!mdata->shuffle_enabled) {
            if (mlist->current_index < (mlist->entry_count - 1)) {
                next_file = mlist->filenames_string_loclist[cur_index + 1];
                file_list_play_file(next_file, cur_index + 1, mdata);
            }
        } else {
            uint32_t next_index = cur_index;
            if (mlist->entry_count != 2) 
            { next_index = get_random_file_index(mdata); }
            else 
            { next_index = !next_index; }
            next_file = mlist->filenames_string_loclist[next_index];
            file_list_play_file(next_file, next_index, mdata);
        }
    } else {
        next_file = mlist->filenames_string_loclist[cur_index];
        file_list_play_file(next_file, cur_index, mdata);
    }
}

//for now this is kinda bogus if youre using shuffle
PAC_INTERNAL void goto_prev_file(Music_Data *mdata)
{
    if (0 == mdata->music_list.current_index) 
    { return; }
    File_List *mlist = &mdata->music_list;
    uint32_t cur_index = mlist->current_index;
    int goto_index = cur_index - 1;
    if ((goto_index < 0) || (goto_index >= (int)mlist->entry_count)) 
    { goto_index = get_random_file_index(mdata); }

    char *prev_file = mlist->filenames_string_loclist[goto_index];
    file_list_play_file(prev_file, cur_index - 1, mdata);
}

PAC_INTERNAL void clear_file_list(Music_Data *mdata)
{
    if (!mdata->music_list.entry_count)
    { return; }
    File_List *mlist = &mdata->music_list;

    for (int clear_index = 1;
        clear_index < mlist->entry_count;
        ++clear_index) {
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

void set_userinfo(Runtime_Vars *rtvars, char *notice, Userinfo_Type notice_type) 
{
    rtvars->sflags.last_userinfo_type = notice_type;
    char *note_buf = (char *)rtvars->bufgroup_ptr->userinfo_buffer;
    strncpy(note_buf, notice, USERINFO_BUFFER_SIZE - 1);
}

PAC_INTERNAL void set_userinfo_color(Runtime_Vars *rtvars)
{
    switch (rtvars->sflags.last_userinfo_type) {
    case USERINFO_TYPE_ERROR: { 
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0xFF, 0x20, 0x20, 0xFF));
    } break;
    case USERINFO_TYPE_WARNING: {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0xFF, 0x90, 0x00, 0xFF));
    } break;
    case USERINFO_TYPE_NOTE:
    default: {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0xFF, 0xFF, 0x00, 0xFF));
    } break;
    }
}

PAC_INTERNAL void tupacmixer_get_cover(Bitmap_Info *bmpinfo, Runtime_Vars *rtvars)
{
    bmpinfo->img_data = pacmxr_meta_get_cover(0, &bmpinfo->img_data_bytes);
    if (bmpinfo->img_data) {
        pac_init_bitmap(bmpinfo, rtvars);
    }
}

#if STBIMAGE_ENABLED
PAC_INTERNAL void pac_init_bitmap(Bitmap_Info *bmpinfo, Runtime_Vars *rtvars)
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

PAC_INTERNAL void menu_do_current_file_info(Runtime_Vars *rtvars, 
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
        mdata->cover.ogl_tex_id) {
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

PAC_INTERNAL void menu_do_search(Runtime_Vars *rtvars,
                                General_Buffer_Group *bufgroup,
                                Music_Data *mdata)
{
    Sdl_Apidata *sdldata = rtvars->sdldata_ptr;
    ImVec2 winsize = ImVec2(400, 70);
    ImGui::SetNextWindowSize(winsize);
    ImVec2 winpos = ImVec2(sdldata->win_width - winsize.x - 10, winsize.y);
    ImGui::SetNextWindowPos(winpos);

    if (ImGui::Begin("search list", 0, 
        ImGuiWindowFlags_NoScrollbar
        |ImGuiWindowFlags_NoResize)) {
        ImGui::SetKeyboardFocusHere(0);
        if (!ImGui::IsWindowHovered() &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        { rtvars->sflags.searchwindow_open = 0; }

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::InputText("##search_query", 
            (char *)bufgroup->inbuf_search,
            SEARCH_BUFFER_SIZE - 1)) { 
            rtvars->sflags.search_changed = 1;
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

PAC_INTERNAL void menu_do_metadata_editor(Runtime_Vars *rtvars, 
                                        Music_Data *mdata, 
                                        General_Buffer_Group *bufgroup)
{
    Metadata_Editor *meta = &mdata->metaed;
    State_Flags *sflags = &rtvars->sflags;
    File_List *mlist = &mdata->music_list;
    char *filename = meta->editor_current;

    if (pac_btn_press(SDL_SCANCODE_ESCAPE,
        &rtvars->sflags.esc_wasdown,
        rtvars->kbd_state)) {
        rtvars->sflags.viewstate = CENTER_VIEW_STATE_MUSIC_LIST;
        return;
    }

    //ImGui::Separator();
    ImGui::SetCursorPosX(50);
    if (filename[0]) {
        ImGui::Text("editing metadata for:");
        ImGui::SameLine();
        //ImGui::SetNextItemWidth(ImGui::CalcTextSize(filename).x + 10.0f);
        ImGui::PushItemWidth(ImGui::CalcTextSize(filename).x + 10.0f);
        ImGui::InputText("##metadata_filename", 
                filename, 
                PATH_MAX - 1, 
                ImGuiInputTextFlags_ReadOnly);
        ImGui::SetCursorPosX(50);
        ImGui::InputText("title##meta_title", 
                meta->inbuf_title, 
                META_EDITOR_BUFSIZE - 1);

        ImGui::SetCursorPosX(50);
        ImGui::InputText("artist##meta_artist", 
                meta->inbuf_artist, 
                META_EDITOR_BUFSIZE - 1);

        ImGui::SetCursorPosX(50);
        ImGui::InputText("album##meta_album", 
                meta->inbuf_album, 
                META_EDITOR_BUFSIZE - 1);
        ImGui::SetCursorPosX(50);
        ImGui::InputText("genre##meta_genre", 
                meta->inbuf_genre, 
                META_EDITOR_BUFSIZE - 1);

        ImGui::PushItemWidth(100);

        ImGui::SetCursorPosX(50);
        ImGui::InputText("year##meta_year", 
                meta->inbuf_date, 
                sizeof(meta->inbuf_date) - 1);

        ImGui::SetCursorPosX(50);
        ImGui::InputText("track##meta_tracknum", 
                meta->inbuf_tracknum, 
                sizeof(meta->inbuf_tracknum) - 1);
        
        ImGui::PopItemWidth();
        ImGui::PopItemWidth();
        ImGui::SetCursorPosX(50);

        if (ImGui::Button("save metadata")) {
            if (meta->meta_struct.in_avf_ctx) {
                if (meta->inbuf_title[0]) {
                    pacmxr_meta_set_title(&meta->meta_struct, meta->inbuf_title);
                } if (meta->inbuf_artist[0]) {
                    pacmxr_meta_set_artist(&meta->meta_struct, meta->inbuf_artist);
                } if (meta->inbuf_album[0]) {
                    pacmxr_meta_set_album(&meta->meta_struct, meta->inbuf_album);
                } if (meta->inbuf_genre[0]) {
                    pacmxr_meta_set_genre(&meta->meta_struct, meta->inbuf_genre);
                } if (meta->inbuf_date[0]) {
                    pacmxr_meta_set_year(&meta->meta_struct, strtol(meta->inbuf_date, 0, 10));
                } if (meta->inbuf_tracknum[0]) {
                    pacmxr_meta_set_track(&meta->meta_struct, strtol(meta->inbuf_tracknum, 0, 10));
                }

                if (pacmxr_meta_save(&meta->meta_struct)) {
                    set_userinfo(rtvars, 
                            "updated metadata.", 
                            USERINFO_TYPE_NOTE);
                } else {
                    set_userinfo(rtvars, 
                            "failed to update metadata.", 
                            USERINFO_TYPE_ERROR);
                }
            } else {
                set_userinfo(rtvars, 
                        "[error]: failed to update metadata.", 
                        USERINFO_TYPE_ERROR);
            }
        }
    } else {
        ImGui::Text("invalid file selected for metadata editor");
    }
}

#if 1
PAC_INTERNAL void init_metadata_editor(Runtime_Vars *rtvars, Music_Data *mdata)
{
    File_List *mlist = &mdata->music_list;
    Metadata_Editor *meta = &mdata->metaed;
    char *filename = mlist->filenames_string_loclist[mlist->context_index];
    char *cont = find_top_level_path4file(filename, mlist);
    if (cont) {
        snprintf(meta->editor_current, PATH_MAX,
#if _2PACWAV_LINUX
                "%s/%s", 
#elif _2PACWAV_WIN32
                "%s\\%s", 
#endif
                cont, filename);
        if (meta->meta_struct.in_avf_ctx) {
            pacmxr_meta_close_file(&meta->meta_struct); 
        }
        
        meta->inbuf_title[0] = 0;
        meta->inbuf_artist[0] = 0;
        meta->inbuf_album[0] = 0;
        meta->inbuf_genre[0] = 0;
        meta->inbuf_tracknum[0] = 0;
        meta->inbuf_date[0] = 0;
        if (pacmxr_meta_open_file(&meta->meta_struct, meta->editor_current)) {
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
            if (date >= 0) {
                snprintf(meta->inbuf_date, sizeof(meta->inbuf_date), "%d", date);
            } 
            int track = pacmxr_meta_get_track(&meta->meta_struct);
            if (track >= 0) {
                snprintf(meta->inbuf_tracknum, sizeof(meta->inbuf_tracknum), "%d", track);
            }
        } else {
            set_userinfo(rtvars,
                    "[error]: couldn't get metadata information",
                    USERINFO_TYPE_ERROR);
        }
    } else { 
        *meta->editor_current = 0; 
    }
}
#endif

PAC_INTERNAL void menu_do_music_list(Runtime_Vars *rtvars, Music_Data *mdata)
{
    File_List *mlist = &mdata->music_list;
    State_Flags *sflags = &rtvars->sflags;
    uint32_t render_index = 0, file_index = 0;
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0, 0.5f));
    char btntext[NAME_MAX];
    char *filename;
    const uint8_t *kbd = rtvars->kbd_state;
    char ctrl, shift;

    ImVec2 width;
    width.x = ImGui::GetColumnWidth(-1);
    width.y = 0;
    //ImGui::SetCurrentContext(GImGui);
    for (int loop_index = 0; loop_index < (int)mlist->entry_count; ++loop_index) {
        if (!mlist->match_flags[loop_index]) {
            filename = mlist->filenames_string_loclist[loop_index];

            snprintf(btntext, NAME_MAX, "%s##%d", filename, loop_index);
            if (ImGui::Button(btntext, width)) { 
                file_list_play_file(filename, loop_index, mdata); 
            }

            if (ImGui::BeginPopupContextItem(0 /*btntext*/)) {
                mlist->context_index = loop_index;
                if (ImGui::Button("edit metadata")) {
                    sflags->viewstate = CENTER_VIEW_STATE_METADATA_EDITOR;
                    init_metadata_editor(rtvars, mdata);
                }
                ImGui::EndPopup();
            }
        }
    }
    ImGui::PopStyleVar();
}

PAC_INTERNAL void str2lowercase(char *string, int len) 
{
    for (int i = 0; i < len; ++i) {
        string[i] = tolower(string[i]);
    }
}

PAC_INTERNAL char *pac_strcasestr(char *str, char *substr) 
{
    //WARNING: remember that this returns a pointer to lower_str
    //and not the actual shit you passed in
    PAC_LOCAL_STATIC char lower_str[NAME_MAX + 1], lower_substr[NAME_MAX + 1];
    strncpy(lower_str, str, NAME_MAX);
    strncpy(lower_substr, substr, NAME_MAX);
    str2lowercase(lower_str, strlen(lower_str));
    str2lowercase(lower_substr, strlen(lower_substr));
    char *result = strstr(lower_str, lower_substr);
    return result;
}

PAC_INTERNAL void set_match_flags(char *searchbuf, Music_Data *mdata)
{
    File_List *mlist = &mdata->music_list;
    mlist->match_count = 0;

    if (searchbuf[0]) {
        char *string;
        for (int file_index = 0;
            file_index < mlist->entry_count;
            ++file_index) {
            string = mlist->filenames_string_loclist[file_index];
            //NOTE: for some reason i made it so that 0 means this shit IS included in the search results
            if (pac_strcasestr(string, searchbuf)) {
                mlist->match_flags[file_index] = 0;
                ++mlist->match_count;
            } else { 
                mlist->match_flags[file_index] = 1; 
            }
        }
    } else {
        memset(mlist->match_flags, 0, mlist->entry_count);
        mlist->match_count = mlist->entry_count;
    }
}

PAC_INTERNAL void menu_do_list_control(Runtime_Vars *rtvars, Music_Data *mdata)
{
    PAC_LOCAL_STATIC char sort_reversed = 0;
    State_Flags *sflags = &rtvars->sflags;
    char *sort_text = rtvars->bufgroup_ptr->sort_text;

    ImGui::SetCursorPosX(50);
#if 0
    char s_was_pressed = pac_btn_press(SDL_SCANCODE_S, 
                            &sflags->s_wasdown, 
                            rtvars->kbd_state);
    if (ImGui::Button(sort_text) ||
            ((rtvars->kbd_state[SDL_SCANCODE_LSHIFT] &&
            rtvars->kbd_state[SDL_SCANCODE_LCTRL]) &&
            s_was_pressed)) {
        if (mdata->music_list.entry_count) {
            if (!sort_reversed) {
                sort_file_list_alpha(&mdata->music_list, 0); 
                strcpy(sort_text, "sort (z-a)##1");
                sort_reversed = 1;
            } else {
                sort_file_list_alpha(&mdata->music_list, 1); 
                strcpy(sort_text, "sort (a-z)##0");
                sort_reversed = 0;
            }
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("clear list")) {
        sflags->clear_confirmation = !sflags->clear_confirmation;
    }

    PAC_LOCAL_STATIC char toggle_btn[16];
    Center_View_State vs = rtvars->sflags.viewstate;
    if (vs == CENTER_VIEW_STATE_MUSIC_LIST) { 
        strcpy(toggle_btn, "hide list"); 
    } else if (vs == CENTER_VIEW_STATE_CURRENT_INFO) { 
        strcpy(toggle_btn, "show list"); 
    }

    ImGui::SameLine();
    if (ImGui::Button(toggle_btn) ||
            (rtvars->kbd_state[SDL_SCANCODE_LCTRL] &&
            pac_btn_press(SDL_SCANCODE_L, &sflags->l_wasdown, rtvars->kbd_state))) {
        do {
            cycle_center_view_state(&sflags->viewstate); 
        } while ((sflags->viewstate != CENTER_VIEW_STATE_MUSIC_LIST) &&
                (sflags->viewstate != CENTER_VIEW_STATE_CURRENT_INFO));
    }

    if (sflags->clear_confirmation) {
        //TODO
        //menu_confirm_clear_file_list(rtvars, mdata);
        clear_file_list(mdata);
        sflags->clear_confirmation = 0;
    }
#endif
}

PAC_INTERNAL void menu_do_volume_bar(Runtime_Vars *rtvars,
                                    Music_Data *mdata,
                                    float vol_width)
{
    State_Flags *sflags = &rtvars->sflags;
    int vol_step = rtvars->sargs_ptr->volume_step;
    //if(rtvars->kbd_state[SDL_SCANCODE_LCTRL])
    if (!ImGui::GetIO().WantTextInput) {
        if (pac_btn_press(SDL_SCANCODE_0, &sflags->zero_wasdown, rtvars->kbd_state)) { 
            if ((mdata->volume + vol_step) < SDL_MIX_MAXVOLUME) { 
                mdata->volume += vol_step; 
            } else {
                mdata->volume = SDL_MIX_MAXVOLUME;
            }
            pacmxr_set_volume(mdata->volume);
        } if (pac_btn_press(SDL_SCANCODE_9, &sflags->nine_wasdown, rtvars->kbd_state)) { 
            if (((int)mdata->volume - vol_step) > 0) {
                mdata->volume -= vol_step;
            } else { 
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

PAC_INTERNAL void menu_do_seek_bar(Runtime_Vars *rtvars, Music_Data *mdata)
{
    State_Flags *sflags = &rtvars->sflags;
    const uint8_t *kbd = rtvars->kbd_state;
    mdata->seek_value = conv_songpos2slide_value(mdata);

    if (kbd[SDL_SCANCODE_LCTRL] || kbd[SDL_SCANCODE_RCTRL]) {
        float new_pos;
        if (pac_btn_press(SDL_SCANCODE_RIGHT, &sflags->right_wasdown, kbd)) {
            new_pos = mdata->current_position + (double)mdata->seek_increment;
            //new_pos = mdata->seek_value + 0.05f;
            if (new_pos < mdata->current_duration) { 
                pacmxr_seek(pacmxr_seconds_to_seek_value(new_pos));
            } else { 
                goto_next_file(mdata); 
            }
        } else if (pac_btn_press(SDL_SCANCODE_LEFT, &sflags->left_wasdown, kbd)) {
            new_pos = mdata->current_position - (double)mdata->seek_increment;
            //new_pos = mdata->seek_value - 0.05f;
            if (new_pos > 0.0) { 
                pacmxr_seek(pacmxr_seconds_to_seek_value(new_pos));
            } else { 
                pacmxr_seek(0.0f);
            }
        } else if (pac_btn_press(SDL_SCANCODE_HOME, &sflags->home_wasdown, kbd)) {
            //NOTE: this doesn't work for the home key on my laptop keypad for example
            pacmxr_seek(0.0f);
        }
    }

    ImGui::SameLine();
    float width_left = ImGui::GetContentRegionAvail().x;
    ImGui::SetNextItemWidth(width_left);
    if (ImGui::SliderInt("##vol_seeker", 
        &mdata->seek_value, 0, 
        PAC_SEEK_VALUE_MAX, "",
        ImGuiSliderFlags_NoInput))
    { pacmxr_seek(mdata->seek_value/(float)PAC_SEEK_VALUE_MAX); }
}

PAC_INTERNAL void do_path_autocomplete(char *current, Runtime_Vars *rtvars)
{
#if 0x0
    File_List *alist = &rtvars->autocomp_list;
    PAC_LOCAL_STATIC int suggest_index = -1;
    char tempbuf[PATH_MAX];
    int len = strlen(current);
    strncpy(tempbuf, current, PATH_MAX - 1);
    int temp_len = len;
    while (--temp_len) {
        tempbuf[temp_len] = 0;
#if _2PACWAV_LINUX
        if (tempbuf[temp_len] == '/')
#elif _2PACWAV_WIN32
        if (tempbuf[temp_len] == '\\')
#endif
        { break; }
    }

    if (platform_path_exists(tempbuf)) {
        alist->entry_count = 0;
        suggest_index = -1;
        platform_list_files_simple(tempbuf, alist, 1);
#if 0
        printf("%d files found in %s\n", alist->entry_count, current);
        for (int i = 0; i < (int)alist->entry_count; ++i) {
            printf("%s\n", alist->filenames_string_loclist[i]);
        }
#endif
        //printf("dir %s exists. it has %d things\n", tempbuf, alist->entry_count);
        char *pattern = current;
        int pattern_len = strlen(pattern);
        while (temp_len--) {
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
        for (int i = suggest_index + 1; i < (int)alist->entry_count; ++i) {
            temp_len = 0;
            i_file = alist->filenames_string_loclist[i];
            //printf("%s\n", i_file);
            do {
                if (temp_len == pattern_len - 1) {
                    suggest_index = i;
                    i = alist->entry_count + 1; //break out of outer loop
                    break;
                } else if (pattern[temp_len] != i_file[temp_len]) {
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
        if (suggest_index != -1) {
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

PAC_INTERNAL void menu_do_menubar(Runtime_Vars *rtvars, Music_Data *mdata)
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

    if (*vs == CENTER_VIEW_STATE_MUSIC_LIST) {
        strcpy(ls_toggle_text, "hide list"); 
    } else if (*vs == CENTER_VIEW_STATE_CURRENT_INFO) {
        strcpy(ls_toggle_text, "show list");
    }

    char sort_was_pressed = (ctrl && shift &&
                            pac_btn_press(SDL_SCANCODE_S, 
                            &sflags->s_wasdown, 
                            kbd));
    char clear_was_pressed = (ctrl && shift &&
                            pac_btn_press(SDL_SCANCODE_X,
                            &sflags->x_wasdown,
                            kbd));
    char lkey = pac_btn_press(SDL_SCANCODE_L, &sflags->l_wasdown, kbd);
    char lstoggle_was_pressed = ((ctrl && lkey) && !shift);
    char reptoggle_was_pressed = (ctrl && shift && lkey);

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("view")) {
            if (ImGui::MenuItem(sort_text, "ctrl-shift-s")) {
                sort_was_pressed = 1;
            } if (ImGui::MenuItem("clear", "ctrl-shift-x")) {
                clear_was_pressed = 1;
            } if (ImGui::MenuItem(ls_toggle_text, "ctrl-l")) {
                lstoggle_was_pressed = 1;
            } if (ImGui::MenuItem("search", "ctrl-f")) {
                sflags->searchwindow_open = 1;
            } if (ImGui::MenuItem(vis_toggle_text)) {
                sflags->visualizer_enabled = !sflags->visualizer_enabled;
                if (sflags->visualizer_enabled) {
                    strcpy(vis_toggle_text, "disable visualizer");
                } else {
                    strcpy(vis_toggle_text, "enable visualizer");
                }
            }
            ImGui::EndMenu();
        } if (ImGui::BeginMenu("playback")) {
            if (ImGui::MenuItem(play_toggle_text, "space")) {
                if (pacmxr_file_is_open()) {
                    pacmxr_toggle_playback();
                    //mdata->paused = !mdata->paused;
                    //pacmxr_pause(mdata->paused);
                }
            } if (ImGui::MenuItem("halt")) {
                if (pacmxr_file_is_open())
                { pacmxr_close_file(); }
            } if (ImGui::MenuItem("previous", "ctrl-p")) {
                goto_prev_file(mdata);
            } if (ImGui::MenuItem("next", "ctrl-n")) {
                goto_next_file(mdata);
            } if (ImGui::MenuItem(shuf_toggle_text, "ctrl-shift-r")) {
                mdata->shuffle_enabled = !mdata->shuffle_enabled;
            } if (ImGui::MenuItem(repeat_toggle_text, "ctrl-shift-l")) {
                reptoggle_was_pressed = 1;
            }
            ImGui::EndMenu();
        } if (ImGui::BeginMenu("settings")) {
            if (ImGui::MenuItem("visualizer color")) {
                sflags->colorpicker_open = !sflags->colorpicker_open;
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

    if (sort_was_pressed) {
        cycle_sort_state(&sflags->sort_state);

        switch(sflags->sort_state) {
        case SORT_STATE_ALPHA_ASCENDING: {
            strcpy(sort_text, "sort (z-a)");
            sort_file_list_alpha(&mdata->music_list, 0); 
            if (searchbuf[0]) {
                set_match_flags((char *)rtvars->bufgroup_ptr->inbuf_search, mdata);
            }
        } break;

        case SORT_STATE_ALPHA_DESCENDING: {
            strcpy(sort_text, "sort (mod date asc)");
            sort_file_list_alpha(&mdata->music_list, 1); 
            if (searchbuf[0]) {
                set_match_flags((char *)rtvars->bufgroup_ptr->inbuf_search, mdata);
            }
        } break;

        case SORT_STATE_MOD_DATE_ASCENDING: {
            strcpy(sort_text, "sort (mod date desc)");
            platform_sort_mlist_mod_date_janky(&mdata->music_list, 1, rtvars);
        } break;

        case SORT_STATE_MOD_DATE_DESCENDING: {
            strcpy(sort_text, "sort (a-z)");
            platform_sort_mlist_mod_date_janky(&mdata->music_list, 0, rtvars);
        } break;

        default: break;
        }
    } if (clear_was_pressed) {
        clear_file_list(mdata);
    } if (lstoggle_was_pressed) {
        do {
            cycle_center_view_state(vs); 
        } while ((*vs != CENTER_VIEW_STATE_MUSIC_LIST) &&
                (*vs != CENTER_VIEW_STATE_CURRENT_INFO));
    } if (reptoggle_was_pressed) {
        mdata->loop_enabled = !mdata->loop_enabled;
        if (mdata->loop_enabled) {
            strcpy(repeat_toggle_text, "disable looping");
        } else {
            strcpy(repeat_toggle_text, "enable looping");
        }
    }
    ImGui::PopStyleColor();
}

PAC_INTERNAL void menu_do_colorpicker(Runtime_Vars *rtvars)
{
    Sdl_Apidata *sdldata = rtvars->sdldata_ptr;
    State_Flags *sflags = &rtvars->sflags;
    ImVec2 wdim = ImVec2(400, 400);
    ImVec2 wpos = ImVec2((sdldata->win_width/2) - (wdim.x/2), 100.0f);
    ImGui::SetNextWindowSize(wdim);
    ImGui::SetNextWindowPos(wpos);

    if (ImGui::Begin("visualizer color", 0,
        ImGuiWindowFlags_NoScrollbar
        |ImGuiWindowFlags_NoResize)) {
        if ((!ImGui::IsWindowHovered() &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left)) ||
            pac_btn_press(SDL_SCANCODE_ESCAPE,
                    &sflags->esc_wasdown,
                    rtvars->kbd_state))
        { sflags->colorpicker_open = 0; }

        ImGui::ColorPicker4("##visualizer_color",
                rtvars->vis_color,
                ImGuiColorEditFlags_Float
                |ImGuiColorEditFlags_DisplayHex
                |ImGuiColorEditFlags_DisplayRGB
                |ImGuiColorEditFlags_NoLabel
                |ImGuiColorEditFlags_AlphaBar);
        ImGui::End();
    }
}

PAC_INTERNAL void pac_main_loop(Runtime_Vars *rtvars, 
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

    if (pacmxr_file_is_open())
    { update_audio_time(mdata); }

    pac_begin_frame(rtvars, sdldata);
    menu_do_menubar(rtvars, mdata);
    if (sflags->colorpicker_open)
    { menu_do_colorpicker(rtvars); }

    ImGui::PushItemWidth(ImGui::GetColumnWidth(-1) - add_width);
    char d_was_pressed = pac_btn_press(SDL_SCANCODE_D, 
                            &sflags->d_wasdown, 
                            rtvars->kbd_state);
    if (rtvars->kbd_state[SDL_SCANCODE_LALT] && d_was_pressed)
    { ImGui::SetKeyboardFocusHere(0); }

    ImGui::Text("path:");
    ImGui::SameLine();
    //PAC_LOCAL_STATIC int _inpath_len = 0;

    if (ImGui::InputText("##in_path", 
            (char *)bufgroup->inbuf_filename, 
            PATH_MAX - 1)) {
        //int new_inpath_len = strlen((char *)bufgroup->inbuf_filename);
        //if((new_inpath_len > _inpath_len) || tab_was_pressed) {
        //    do_path_autocomplete((char *)bufgroup->inbuf_filename, rtvars);
        //}
        //_inpath_len = new_inpath_len;
    }
    ImGui::PopItemWidth();

    ImGui::SameLine();
    if (ImGui::Button("add", ImVec2(ImGui::GetColumnWidth(-1), 0))) {
        add_to_music_list((char *)bufgroup->inbuf_filename, mdata, rtvars); 
    }

    ImGui::Separator();
    ImGui::SetCursorPosX(50);
    if (rtvars->kbd_state[SDL_SCANCODE_LCTRL] && 
            pac_btn_press(SDL_SCANCODE_Q, &sflags->q_wasdown, rtvars->kbd_state)) {
        sflags->searchwindow_open = 0;
        ImGui::SetKeyboardFocusHere(0);
    }
    ImGui::BeginChild("##center_thing", 
            ImVec2(ImGui::GetColumnWidth(-1), 
            rtvars->sdldata_ptr->win_height - 
            ImGui::GetTextLineHeight() - 100),
            0);

    //this check probably hasn't been necessary since the imgui port
    //if ((sdldata->win_height > MLIST_MIN_WIN_WIDTH) && 
    //    (sdldata->win_width > MLIST_MIN_WIN_HEIGHT)) {
        switch (rtvars->sflags.viewstate) {
        case CENTER_VIEW_STATE_MUSIC_LIST: {
            menu_do_music_list(rtvars, mdata);
        } break;

        case CENTER_VIEW_STATE_CURRENT_INFO: {
            menu_do_current_file_info(rtvars, mdata, bufgroup);
        } break;

        case CENTER_VIEW_STATE_METADATA_EDITOR: {
            menu_do_metadata_editor(rtvars, mdata, bufgroup);
        } break;

        default: break;
        }
    //}
    ImGui::EndChild();
    ImGui::SetCursorPosX(50);
    ImGui::Separator();

    if (mdata->shuffle_enabled) { 
        strcpy(shuffle_btn_text, "-");
        strcpy((char *)bufgroup->shuf_toggle_text, "disable shuffle");
    } else { 
        strcpy(shuffle_btn_text, "sh");
        strcpy((char *)bufgroup->shuf_toggle_text, "enable shuffle");
    }

    if (mdata->loop_enabled) {
        strcpy(loop_btn_text, "nl");
        strcpy((char *)bufgroup->loop_toggle_text, "disable looping");
    } else {
        strcpy(loop_btn_text, "lo");
        strcpy((char *)bufgroup->loop_toggle_text, "enable looping");
    }

    int btns2draw = 6;
    ImGui::SetCursorPosY(sdldata->win_height - (btn_height*btns2draw) - 5);

    if (ImGui::Button(loop_btn_text, ImVec2(btn_side_len, btn_side_len))) 
    { mdata->loop_enabled = !mdata->loop_enabled; }

    char r_was_pressed = pac_btn_press(SDL_SCANCODE_R, 
                            &rtvars->sflags.r_wasdown, 
                            rtvars->kbd_state);
    if (ImGui::Button(shuffle_btn_text, ImVec2(btn_side_len, btn_side_len)) ||
            ((rtvars->kbd_state[SDL_SCANCODE_LCTRL] && 
            rtvars->kbd_state[SDL_SCANCODE_LSHIFT]) &&
            r_was_pressed)) 
    { mdata->shuffle_enabled = !mdata->shuffle_enabled; }

    char n_was_pressed = pac_btn_press(SDL_SCANCODE_N, 
                            &rtvars->sflags.n_wasdown, 
                            rtvars->kbd_state);
    char p_was_pressed = pac_btn_press(SDL_SCANCODE_P, 
                            &rtvars->sflags.p_wasdown, 
                            rtvars->kbd_state);
    if (ImGui::Button(">>", ImVec2(btn_side_len, btn_side_len)) ||
            (rtvars->kbd_state[SDL_SCANCODE_LCTRL] &&
            n_was_pressed)) 
    { goto_next_file(mdata); } 
    if (ImGui::Button("<<", ImVec2(btn_side_len, btn_side_len)) ||
            (rtvars->kbd_state[SDL_SCANCODE_LCTRL] &&
            p_was_pressed)) 
    { goto_prev_file(mdata); }

    //halt
    if (ImGui::Button((char *)_stop_btn_glyph, ImVec2(btn_side_len, btn_side_len))) {
        if (pacmxr_file_is_open())
        { pacmxr_close_file(); }
    }

    if (rtvars->pacmxr_ctx->paused) { 
        playback_btn_text[0] = '>'; 
        playback_btn_text[1] = 0; 
        strcpy((char *)bufgroup->play_toggle_text, "play");
    } else { 
        playback_btn_text[0] = '|'; 
        playback_btn_text[1] = '|'; 
        playback_btn_text[2] = 0; 
        strcpy((char *)bufgroup->play_toggle_text, "pause");
    }

    char space_was_pressed = pac_btn_press(SDL_SCANCODE_SPACE, 
                                &sflags->space_wasdown, 
                                rtvars->kbd_state);
    if (ImGui::Button(playback_btn_text, ImVec2(btn_side_len, btn_side_len)) || 
        (space_was_pressed && !ImGui::GetIO().WantTextInput)) {
        if (pacmxr_file_is_open())
        { pacmxr_toggle_playback(); }
    }

    menu_do_volume_bar(rtvars, mdata, vol_width);
    menu_do_seek_bar(rtvars, mdata);

#if 1
    char searchkey_was_pressed = (rtvars->kbd_state[SDL_SCANCODE_LCTRL] &&
                                pac_btn_press(SDL_SCANCODE_F, 
                                &sflags->f_wasdown, 
                                rtvars->kbd_state));
    char esc_was_pressed = pac_btn_press(SDL_SCANCODE_ESCAPE,
                                &sflags->esc_wasdown,
                                rtvars->kbd_state);
    char enter_was_pressed = pac_btn_press(SDL_SCANCODE_RETURN,
                                &sflags->enter_wasdown,
                                rtvars->kbd_state);
    if (searchkey_was_pressed) { 
        sflags->searchwindow_open = !sflags->searchwindow_open; 
    } else if (esc_was_pressed || enter_was_pressed) {
        sflags->searchwindow_open = 0;
    }
#endif
    pac_end_frame(rtvars, sdldata);
}

PAC_INTERNAL void tupacmixer_get_taginfo(Music_Data *mdata)
{
    if (!pacmxr_file_is_open()) { return; }
    Audio_Metadata_Group *amg = &mdata->current_metadata;
    int taginfo_len = 0;
    taginfo_len += pacmxr_meta_get_title(0, amg->tagbuffer, sizeof(amg->tagbuffer));
    if (taginfo_len) {
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
    } else {
        File_List *mlist = &mdata->music_list;
        snprintf(amg->tagbuffer, sizeof(amg->tagbuffer),
                "%s", mlist->filenames_string_loclist[mlist->current_index]);
    }
}

PAC_INTERNAL void tupacmixer_start_music(Music_Data *mdata, char *music_path) 
{
    pacmxr_close_file();
    if (pacmxr_open_file(music_path)) {
        pacmxr_pause(0);
        tupacmixer_get_audio_type(mdata);
        tupacmixer_get_cover(&mdata->cover, mdata->rtvars_ptr);
        //SDL_UnlockAudioDevice(global_pacmxr_ctx.au_dev);
    } else {
        platform_dbg_log("failed to play file \n");
        char info[USERINFO_BUFFER_SIZE];
        snprintf(info, USERINFO_BUFFER_SIZE - 1,
                "[error]: failed to play file %s, skipping.", //NOTE: this will (almost) never show up since it just gets overwritten
                mdata->music_list.filenames_string_loclist[mdata->music_list.current_index]);
        set_userinfo(mdata->rtvars_ptr, info, USERINFO_TYPE_ERROR);
        //goto_next_file(mdata);
    }
}

PAC_INTERNAL void tupacmixer_stop_music(Music_Data *mdata)
{
    pacmxr_close_file();
}

void pac_sdlmixer_music_finished_callback(void)
{
}

PAC_INTERNAL char pac_init_tupacmixer(Music_Data *mdata) 
{
    Pacmxr_Init_Options init_opts = {};
    init_opts.sample_rate = PACMXR_RESAMPLE_SAMPLERATE;
    init_opts.no_init_sdl = 1;
    //init_opts.av_loglevel = AV_LOG_ERROR;
#if _2PACWAV_DEBUG
    init_opts.av_loglevel = AV_LOG_DEBUG;
#else
    init_opts.av_loglevel = AV_LOG_ERROR;
#endif
    if (!pacmxr_init(&init_opts)) {
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
    if (!pacmxr_context->au_dev) {
        platform_log("pacmxr_init failed to open audio device: %s\n",
                SDL_GetError());
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

PAC_INTERNAL char pac_init_sdl(Sdl_Apidata *sdldata) 
{
    char result = 0;
    if (!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO)) {
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
        if (sdldata->window_ptr) {
            sdldata->ogl_context = SDL_GL_CreateContext(sdldata->window_ptr);
            if (sdldata->ogl_context) { 
                result = 1; 
            } else { 
                fprintf(stderr, "SDL failed to create OpenGL context\n"); 
            }
        } else { 
            fprintf(stderr, "SDL failed to create window\n"); 
        }
    } else { 
        fprintf(stderr, "SDL failed to init\n"); 
    }
    return result;
}
