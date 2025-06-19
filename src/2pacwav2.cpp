
/*
File: 2pacwav2.cpp
Date: Thu 24 Apr 2025 04:24:08 PM EEST

TODO: figure out how to make the search dialog not block input
*/

#include <stdio.h>
#include <limits.h>
#include <stdint.h>

#include "SDL.h"
//#include "SDL_mixer.h"
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glext.h>

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
//#include "2pacwav2_tagging.cpp"
#include "2pacwav2_confparser.cpp"

#define PORT_THIS 0

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

PAC_INTERNAL void show_help()
{
    show_version();
    platform_log("usage: 2w [options] [files]\n"
                "-h | --help : print this message and exit\n"
                "-v | --version : print version and exit\n"
                "-noconf : do not look for a configuration file\n"
                "-fontsize <value> : set point size for font\n");
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
            show_help();
            break;
        } else if (!sargs->no_load_conf && !strcmp("-noconf", arg)) {
            sargs->no_load_conf = 1;
        } else if ((sargs->font_size == PAC_LATIN_FONTSIZE) &&
            !strcmp("-fontsize", arg)) {
            if (args[arg_index + 1]) {
                float value = strtof(args[arg_index + 1], 0);
                if (value != 0.0f) {
                    sargs->font_size = value;
                } else {
                    fprintf(stderr, "invalid argument given after -%s, ignoring.\n", arg);
                }
            } else {
                fprintf(stderr, "no argument given after -%s, ignoring.\n", arg);
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

PAC_INTERNAL void startup_load_conf(Runtime_Vars *rtvars, 
                                char *confbuf, 
                                int confbuf_bytes)
{
    char confpath[PATH_MAX];
    const int infosize = PATH_MAX + sizeof("loaded config: ");
    char infobuf[infosize];

#if _2PACWAV_LINUX
    snprintf(confpath, PATH_MAX - 1, "%s/%s", 
            rtvars->working_directory, PAC_CONFNAME_STRING);
#elif _2PACWAV_WIN32
    snprintf(confpath, PATH_MAX - 1, "%s\\%s", 
            rtvars->working_directory, PAC_CONFNAME_STRING);
#endif

    if (platform_file_exists(confpath)) {
        snprintf(rtvars->conf_directory, PATH_MAX, "%s", confpath);
        platform_read_file(confpath, confbuf, confbuf_bytes - 1);
        snprintf(infobuf, infosize, "loaded config: %s", confpath);
        set_userinfo(rtvars, infobuf, USERINFO_TYPE_NOTE);
        platform_dbg_log("%s\n", infobuf);
    } else {
#if _2PACWAV_LINUX
        char *username = getlogin();
        if (username) {
            snprintf(confpath, PATH_MAX,
                    "/home/%s/.config/2pacwav/%s",
                    username, PAC_CONFNAME_STRING);

            if (platform_file_exists(confpath)) {
                snprintf(rtvars->conf_directory, PATH_MAX, "%s", confpath);
                platform_read_file(confpath, confbuf, confbuf_bytes - 1);
                snprintf(infobuf, infosize, "loaded config: %s", confpath);
                set_userinfo(rtvars, infobuf, USERINFO_TYPE_NOTE);
            }
        }
#endif
    }
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

PAC_INTERNAL char pac_imgui_load_font(char *font_name,
                                    float font_size,
                                    Runtime_Vars *rtvars)
{
    char status = 0;
    char latin_path[PATH_MAX], cjk_path[PATH_MAX];
    snprintf(latin_path, PATH_MAX - 1, "%s/%s", 
            rtvars->resource_directory, font_name);
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

#if 1
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
#endif

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

    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(011, 0x33, 0x3A, 0xFF));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0x11, 0x33, 0x3A, 0xFF));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0x0, 0x0, 0x0, 0xFF));
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, IM_COL32(0x22, 0x22, 0x22, 0xFF));

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
    //astream->stream = pac_ctx->aqueue.read_ptr;
    astream->stream_size = pac_ctx->aqueue.sdl_stream_len;
    //printf("stream=%p, stream_len=%d\n", astream->stream, astream->stream_size);
    //astream->stream_size = stream_len;

    if ((rtvars->sflags.viewstate == CENTER_VIEW_STATE_CURRENT_INFO) &&
        (rtvars->sflags.visualizer_enabled))
    { do_visualizer(rtvars, sdldata); }
    SDL_GL_SwapWindow(sdldata->window_ptr);
}

PAC_INTERNAL void update_audio_time(Music_Data *mdata)
{
#if PORT_THIS
    mdata->current_duration = Mix_MusicDuration();
    mdata->current_position = Mix_GetMusicPosition(mdata->sdlmixer_music);
    if ((mdata->current_position + 0.1) >= mdata->current_duration) 
    { goto_next_file(mdata); }
#else
    mdata->current_duration = pacmxr_stream_duration();
    mdata->current_position = pacmxr_seconds_played();
    Pacmxr_Context *pac_ctx = pacmxr_get_context();
    if ((!pac_ctx->paused && pacmxr_file_is_open()) && 
        ((mdata->current_position + 0.1) >= mdata->current_duration))
    { goto_next_file(mdata); }
#endif
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
            "[vol:%d/128][pos:%06.1f/%06.1f][enctype:%s]"
            "\n[path:%s]", 
            mdata->sample_rate, mdata->pcm_bits, mdata->channels,
            mdata->volume, mdata->current_position, mdata->current_duration, mdata->music_type_buf,
            mdata->current_filename);
}

#if PORT_THIS
PAC_INTERNAL void sdlmixer_get_music_type(Music_Data *mdata)
{
    Mix_MusicType music_type = Mix_GetMusicType(mdata->sdlmixer_music);
    switch (music_type) {
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
#else
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
#endif

PAC_INTERNAL void load_file_from_path(char *path, Music_Data *mdata)
{
    if (platform_file_exists(path)) {
#if PORT_THIS
        sdlmixer_start_music(mdata, path); 
#else
        tupacmixer_start_music(mdata, path);
#endif
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
            --temp_in; ++chars;
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
#if PORT_THIS
    double result = 0;
    if (mdata->sdlmixer_music && 
        mdata->current_position && 
        mdata->current_duration) {
        result = (((double)(mdata->seek_value)) / 
                PAC_SEEK_VALUE_MAX) *
                mdata->current_duration; 
    }
    return result;
#else
    float result = 0;
    if (pacmxr_file_is_open() &&
        mdata->current_position &&
        mdata->current_duration) {
        result = (((double)(mdata->seek_value)) /
                PAC_SEEK_VALUE_MAX) *
                mdata->current_duration;
    }
    return result;
#endif
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

PAC_INTERNAL void add_to_music_list(char *path, Music_Data *mdata, Runtime_Vars *rtvars)
{
    int old_file_count = mdata->music_list.entry_count, 
            new_file_count, 
            added_files;

    if (platform_file_exists(path)) {
        add_single_file_to_music_list(path, mdata); 
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
    if (mdata->music_list.entry_count < 1) { return; }

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
    if (!mdata->music_list.entry_count) { return; }
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

PAC_INTERNAL void format_taginfo(Music_Data *mdata, char *begin, char separate)
{
//don't actually port this, i've realized that this is dumb
#if PORT_THIS
    char **title_ptr = &mdata->current_metadata.title_begin_in_buf;
    char **artist_ptr = &mdata->current_metadata.artist_begin_in_buf;
    char **album_ptr = &mdata->current_metadata.album_begin_in_buf;

    *title_ptr = begin;
    **title_ptr = 0; ++*title_ptr;
    Audio_Metadata_Group *meta = &mdata->current_metadata;
    File_List *mlist = &mdata->music_list;
    if (separate) {
        if (strlen(mdata->current_metadata.tag_title)) {
            snprintf(*title_ptr, 1023, "%s\n%s\n%s",
                    meta->tag_title,
                    meta->tag_artist,
                    meta->tag_album);
        } else {
            snprintf(*title_ptr, 1023, "%s",
                    mlist->filenames_string_loclist[mlist->current_index]);
        }
        *artist_ptr = strchr(*title_ptr, '\n');
        **artist_ptr = 0; ++*artist_ptr;
        *album_ptr = strchr(*artist_ptr, '\n');
        **album_ptr = 0; ++*album_ptr;
    } else {
        if (strlen(mdata->current_metadata.tag_title)) {
            snprintf(*title_ptr, 1023, "%s  -  %s  -  %s",
                    meta->tag_title,
                    meta->tag_artist,
                    meta->tag_album);
        } else {
            snprintf(*title_ptr, 1023, "%s",
                    mlist->filenames_string_loclist[mlist->current_index]);
        }
    }
#endif
}

PAC_INTERNAL void menu_do_current_file_info(Runtime_Vars *rtvars, 
                                        Music_Data *mdata, 
                                        General_Buffer_Group *bufgroup)
{
    update_info_buffer(mdata, bufgroup);
    char *infobuf = (char *)bufgroup->music_info_buffer;
    char *path_begin = strchr(infobuf, '\n');
    *path_begin = 0; ++path_begin;
    Sdl_Apidata *sdldata = rtvars->sdldata_ptr;

#if PORT_THIS
    char **title_ptr = &mdata->current_metadata.title_begin_in_buf;
    char **artist_ptr = &mdata->current_metadata.artist_begin_in_buf;
    char **album_ptr = &mdata->current_metadata.album_begin_in_buf;

    if (sdlmixer_get_taginfo(mdata)) {
        char *meta_begin = path_begin + strlen(path_begin);
        gormat_taginfo(mdata, meta_begin, 0); 
    }
#else
    tupacmixer_get_taginfo(mdata);
#endif

    ImGui::Text("%s\n%s", (char *)bufgroup->music_info_buffer, path_begin);
    
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

    //ImGui::Separator();
    ImGui::SetCursorPosX(50);
    if (filename[0]) {
        ImGui::Text("editing metadata for:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::CalcTextSize(filename).x + 10.0f);
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
        if (ImGui::Button("save metadata")) {
#if PORT_THIS
            Tag_Ref tr;
            if (tag_open_file(meta->editor_current, &tr)) {
                if (tag_set_all(&tr, meta)) {
                    set_userinfo(rtvars, 
                            "updated metadata.", 
                            USERINFO_TYPE_NOTE);
                } else {
                    set_userinfo(rtvars, 
                            "[error]: failed to update metadata.", 
                            USERINFO_TYPE_ERROR);
                }
            }
#else
            if (meta->meta_struct.in_avf_ctx) {
                pacmxr_meta_set_title(&meta->meta_struct, meta->inbuf_title);
                pacmxr_meta_set_artist(&meta->meta_struct, meta->inbuf_artist);
                pacmxr_meta_set_album(&meta->meta_struct, meta->inbuf_album);

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
#endif
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
#if PORT_THIS
        Tag_Ref tr;
        if (tag_open_file(meta->editor_current, &tr)) {
            tag_get_title(&tr, meta->inbuf_title, META_EDITOR_BUFSIZE - 1);
            tag_get_artist(&tr, meta->inbuf_artist, META_EDITOR_BUFSIZE - 1);
            tag_get_album(&tr, meta->inbuf_album, META_EDITOR_BUFSIZE - 1);
        }
#else
        if (meta->meta_struct.in_avf_ctx) {
            pacmxr_meta_close_file(&meta->meta_struct); 
        }
        
        meta->inbuf_title[0] = 0;
        meta->inbuf_artist[0] = 0;
        meta->inbuf_album[0] = 0;
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
        } else {
            set_userinfo(rtvars,
                    "[error]: couldn't get metadata information",
                    USERINFO_TYPE_ERROR);
        }
#endif
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

    for (int loop_index = 0; loop_index < (int)mlist->entry_count; ++loop_index) {
        if (!mlist->match_flags[loop_index]) {
            filename = mlist->filenames_string_loclist[loop_index];
            //2 or more buttons with the same name wont work properly
            snprintf(btntext, NAME_MAX - 1, "%s##%d", filename, loop_index);
            if (ImGui::Button(btntext, ImVec2(ImGui::GetColumnWidth(-1), 0))) { 
                file_list_play_file(filename, loop_index, mdata); 
            } if (ImGui::BeginPopupContextItem(btntext)) {
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
    //ImGui::EndChild();
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
    //if(rtvars->kbd_state[SDL_SCANCODE_LCTRL])
    if (!ImGui::GetIO().WantTextInput) {
        if (pac_btn_press(SDL_SCANCODE_0, &sflags->zero_wasdown, rtvars->kbd_state)) { 
            if ((mdata->volume + PAC_DEFAULT_VOLUME_INCREMENT) < SDL_MIX_MAXVOLUME) { 
                mdata->volume += PAC_DEFAULT_VOLUME_INCREMENT; 
            } else {
                mdata->volume = SDL_MIX_MAXVOLUME;
            }
#if PORT_THIS
            Mix_VolumeMusic(mdata->volume);
#else
            pacmxr_set_volume(mdata->volume);
#endif
        } if (pac_btn_press(SDL_SCANCODE_9, &sflags->nine_wasdown, rtvars->kbd_state)) { 
            if (((int)mdata->volume - PAC_DEFAULT_VOLUME_INCREMENT) > 0) {
                mdata->volume -= PAC_DEFAULT_VOLUME_INCREMENT; 
            } else { 
                mdata->volume = 0;
            }
#if PORT_THIS
            Mix_VolumeMusic(mdata->volume);
#else
            pacmxr_set_volume(mdata->volume);
#endif
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
#if PORT_THIS
                Mix_SetMusicPosition(new_pos); 
#else
                pacmxr_seek(pacmxr_seconds_to_seek_value(new_pos));
#endif
            } else { 
                goto_next_file(mdata); 
            }
        } else if (pac_btn_press(SDL_SCANCODE_LEFT, &sflags->left_wasdown, kbd)) {
            new_pos = mdata->current_position - (double)mdata->seek_increment;
            //new_pos = mdata->seek_value - 0.05f;
            if (new_pos > 0.0) { 
#if PORT_THIS
                Mix_SetMusicPosition(new_pos); 
#else
                pacmxr_seek(pacmxr_seconds_to_seek_value(new_pos));
#endif
            } else { 
#if PORT_THIS
                Mix_SetMusicPosition(0.0); 
#else
                pacmxr_seek(0.0f);
#endif
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
        ImGuiSliderFlags_NoInput)) {
#if PORT_THIS
        Mix_SetMusicPosition(new_seek);
#else
        pacmxr_seek(mdata->seek_value/(float)PAC_SEEK_VALUE_MAX);
#endif
    }
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
        printf("dir %s exists. it has %d things\n", tempbuf, alist->entry_count);
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

        printf("whole:%s pattern: %s idx=%d fname=%s templ=%d patl=%d\n", 
                current, 
                pattern, 
                suggest_index, 
                alist->filenames_string_loclist[suggest_index],
                temp_len, 
                pattern_len); 

        //lmao
        if (suggest_index != -1) {
            strncpy(tempbuf, current, PATH_MAX - 1);
            strncat(tempbuf, alist->filenames_string_loclist[suggest_index], PATH_MAX - 1);
            strncpy(current, tempbuf, PATH_MAX - 1);
            printf("tempbuf: %s\n", tempbuf);
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
        if (ImGui::BeginMenu("file")) {
            if (ImGui::MenuItem("add", "ctrl-o")) {
                printf("this does nothing on this build of the program. sorry about that\n");
            }
            ImGui::EndMenu();
        } if (ImGui::BeginMenu("view")) {
            if (ImGui::MenuItem(sort_text, "ctrl-shift-s")) {
                clear_was_pressed = 1;
            } if (ImGui::MenuItem("clear", "ctrl-shift-x")) {
                clear_was_pressed = 1;
            } if (ImGui::MenuItem(ls_toggle_text, "ctrl-l")) {
                lstoggle_was_pressed = 1;
            } if (ImGui::MenuItem("search", "ctrl-f")) {
                sflags->searchwindow_open = 1;
            } if (ImGui::MenuItem(vis_toggle_text, "")) {
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
#if PORT_THIS
                if (mdata->sdlmixer_music) {
                    if (!mdata->paused) {
                        mdata->paused = 1;
                        Mix_PauseMusic(); 
                    } else {
                        mdata->paused = 0;  
                        Mix_ResumeMusic(); 
                    }
                }
#else
                if (pacmxr_file_is_open()) {
                    pacmxr_toggle_playback();
                    //mdata->paused = !mdata->paused;
                    //pacmxr_pause(mdata->paused);
                }
#endif
            } if (ImGui::MenuItem("halt")) {
#if PORT_THIS
                sdlmixer_stop_music(mdata); 
#else
                tupacmixer_stop_music(mdata);
#endif
                mdata->current_filename[0] = 0x0;
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
        }

        set_userinfo_color(rtvars);
        ImGui::SetCursorPosX(rtvars->sdldata_ptr->win_width - 
                (ImGui::CalcTextSize((char *)bufgroup->userinfo_buffer).x) - 5);
        ImGui::Text((char *)bufgroup->userinfo_buffer);
        ImGui::PopStyleColor();
        ImGui::EndMenuBar();
    } 

    if (sort_was_pressed) {
        if (!sflags->sort_reversed) {
            sort_file_list_alpha(&mdata->music_list, 0); 
            strcpy(sort_text, "sort (z-a)");
            sflags->sort_reversed = 1;
        } else {
            sort_file_list_alpha(&mdata->music_list, 1); 
            strcpy(sort_text, "sort (a-z)");
            sflags->sort_reversed = 0;
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

#if PORT_THIS
    if (mdata->sdlmixer_music && 
        (Mix_PlayingMusic() || 
        Mix_PausedMusic()))
    { update_audio_time(mdata); }
#else
    if (pacmxr_file_is_open())
    { update_audio_time(mdata); }
#endif

    pac_begin_frame(rtvars, sdldata);
    menu_do_menubar(rtvars, mdata);

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

    if (ImGui::Button((char *)_stop_btn_glyph, ImVec2(btn_side_len, btn_side_len))) {
#if PORT_THIS
        sdlmixer_stop_music(mdata); 
#else
        tupacmixer_stop_music(mdata);
#endif
        mdata->current_filename[0] = 0x0;
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
#if PORT_THIS
        if (mdata->sdlmixer_music) {
            if (!mdata->paused) { 
                mdata->paused = 1; 
                Mix_PauseMusic(); 
            } else { 
                mdata->paused = 0; 
                Mix_ResumeMusic(); 
            }
        }
#else
        if (pacmxr_file_is_open())
        { pacmxr_toggle_playback(); }
#endif
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

#if PORT_THIS
PAC_INTERNAL char sdlmixer_get_taginfo(Music_Data *mdata)
{
    if (!mdata->sdlmixer_music) { return 0; }
    Audio_Metadata_Group *amg = &mdata->current_metadata;
    amg->tag_title =    Mix_GetMusicTitleTag(mdata->sdlmixer_music);
    amg->tag_artist =   Mix_GetMusicArtistTag(mdata->sdlmixer_music);
    amg->tag_album =    Mix_GetMusicAlbumTag(mdata->sdlmixer_music);
    return 1;
}
#else
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
#endif

#if PORT_THIS
PAC_INTERNAL void sdlmixer_start_music(Music_Data *mdata, char *music_path) 
{
    if (mdata->sdlmixer_music && 
        (Mix_PlayingMusic() || 
        Mix_PausedMusic())) 
    { sdlmixer_stop_music(mdata); }

    mdata->sdlmixer_music = Mix_LoadMUS(music_path);

    if (mdata->sdlmixer_music) { 
        mdata->paused = 0;
        sdlmixer_get_music_type(mdata);
        Mix_FadeInMusic(mdata->sdlmixer_music, 0, 0); 
    } else { 
        platform_dbg_log("failed to load music. desc: %s\n", SDL_GetError()); 
        char info[USERINFO_BUFFER_SIZE];
        snprintf(info, USERINFO_BUFFER_SIZE - 1,
                "[error]: failed to play file %s, skipping.", //NOTE: this will (almost) never show up since it just gets overwritten
                mdata->music_list.filenames_string_loclist[mdata->music_list.current_index]);
        set_userinfo(mdata->rtvars_ptr, info, USERINFO_TYPE_ERROR);
        goto_next_file(mdata);
    }
}
#else
PAC_INTERNAL void tupacmixer_start_music(Music_Data *mdata, char *music_path) 
{
    pacmxr_close_file();
    if (pacmxr_open_file(music_path)) {
        pacmxr_pause(0);
        tupacmixer_get_audio_type(mdata);
        //SDL_UnlockAudioDevice(global_pacmxr_ctx.au_dev);
    } else {
        platform_dbg_log("failed to load music\n");
        //goto_next_file(mdata);
    }
}
#endif

#if PORT_THIS
PAC_INTERNAL void sdlmixer_stop_music(Music_Data *mdata)
{
    if (mdata->sdlmixer_music) {
        Mix_HaltMusic();
        Mix_FreeMusic(mdata->sdlmixer_music);
        mdata->sdlmixer_music = 0;
        strcpy(mdata->music_type_buf, "NONE");
    }
}
#else
PAC_INTERNAL void tupacmixer_stop_music(Music_Data *mdata)
{
    pacmxr_close_file();
}
#endif

void pac_sdlmixer_music_finished_callback(void)
{
}

void pac_sdlmixer_postmix_callback(void *udata, uint8_t *stream, int stream_len)
{
    Runtime_Vars *rtvars = (Runtime_Vars *)udata;
    if (rtvars) {
        Audio_Stream *astream = &rtvars->mdata_ptr->astream;
        astream->stream = stream;
        astream->stream_size = stream_len;
    }
}

#if PORT_THIS
PAC_INTERNAL char pac_init_sdlmixer(Music_Data *mdata) 
{
    //int audio_devs = SDL_GetNumAudioDevices(0);
    const char *dev2open = SDL_GetAudioDeviceName(0, 0);
    PAC_ASSERT(dev2open);

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
    mdata->volume = MIX_MAX_VOLUME/2;
    mdata->seek_increment = PAC_DEFAULT_SEEK_INCREMENT;

    if (Mix_OpenAudioDevice(mdata->sample_rate, 
            mdata->pcm_bits, 
            mdata->channels, 
            mdata->chunk_size,
            dev2open,
            flags)) { 
        fprintf(stderr, 
                "failed to open audio device. desc: %s\n", 
                SDL_GetError()); 
    } else {
        result = 1; 
        Mix_VolumeMusic(mdata->volume);
        Mix_SetMusicCMD(SDL_getenv("MUSIC_CMD"));
        Mix_HookMusicFinished(pac_sdlmixer_music_finished_callback);
        Mix_QuerySpec(&mdata->sample_rate, &mdata->pcm_bits, &mdata->channels);
        mdata->pcm_bits &= 0xFF;
        //Mix_SetPostMix(pac_sdlmixer_postmix_callback, (void *)mdata->rtvars_ptr);
    }
    return result;
}
#else
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
#endif

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
