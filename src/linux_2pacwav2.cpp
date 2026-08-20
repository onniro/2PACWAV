
/*
File: linux_2pacwav2.cpp
Date: Thu 24 Apr 2025 04:22:31 PM EEST

Linux platform-specific code for 2pacwav
*/

#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <stdarg.h>
#include <dirent.h>
#include <time.h>
#include <locale.h>
#include <complex.h>
#include <pthread.h>

#include <fontconfig/fontconfig.h>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"

#define GL_GLEXT_PROTOTYPES 1
#include "SDL.h"
//#include "SDL_mixer.h"
#include <GL/gl.h>
#include <GL/glu.h>

#define PACMXR_IMPLEMENTATION 1
#include "2pacmixer.h"

#include "ro_posix.h"
#include "2pacwav2.h"
#include "linux_2pacwav2.h"

#include "2pacwav2.cpp"

static char *platform_find_res_path(Runtime_Vars *rtvars, 
                                    char *out_res_path, 
                                    int bufsize)
{
    char *result = 0;
    char try_buf[PATH_MAX];
    strncpy(try_buf, rtvars->working_directory, PATH_MAX - 1);
    char *end_ptr = try_buf + strlen(try_buf);
    char *last_ptr;
    int max_tries = 5;
    for (int try_index = 0; try_index < max_tries; ++try_index)
    {
        strcat(end_ptr, "/../res/");
        if (platform_directory_exists(try_buf))
        {
            strncpy(out_res_path, try_buf, bufsize);
            result = out_res_path;
            break;
        }
        last_ptr = strstr(end_ptr, "/res/");
        *last_ptr = 0;
    }
    return result;
}

static void platform_get_font_path(Runtime_Vars *rtvars,
                                char *dest,
                                int dest_size)
{
    FcInit();
    FcConfig *fc_cfg = FcInitLoadConfigAndFonts();
    //FcPattern *pattern = FcNameParse((const FcChar8 *)"Liberation Mono:Regular");
    FcPattern *pattern = FcNameParse((const FcChar8 *)PAC_LATIN_FONT_STRING);
    FcObjectSet *obj_set = FcObjectSetBuild(FC_FILE, (void *)0);
    FcFontSet *font_set = FcFontList(fc_cfg, pattern, obj_set);
    FcChar8 *file;
    FcPattern *font;

    for (int i = 0; i < font_set->nfont; ++i)
    {
        font = font_set->fonts[i];
        FcPatternGetString(font, FC_FILE, 0, &file);
        if (!strcasestr((char *)file, "italic") &&
            !strcasestr((char *)file, "oblique"))
        {
            snprintf(dest, dest_size, "%s", (char *)file);
            platform_dbg_log("loading font %s\n", dest);
            break;
        }
    }

    FcFontSetDestroy(font_set);
    FcObjectSetDestroy(obj_set);
    FcPatternDestroy(pattern);
    FcConfigDestroy(fc_cfg);
    FcFini();
}

typedef struct Metadata_Getter_Args
{
    Runtime_Vars *rtvars;
    char *added_path;
} Metadata_Getter_Args;

static void platform_get_file_mod_dates(File_List *flist)
{
    struct stat statbuf;
    char buf[PATH_MAX];
    Audio_File *file;

    for (int file_index = 0;
        file_index < flist->entry_count;
        ++file_index)
    {
        file = &flist->file_strings[file_index];
        snprintf(buf, sizeof(buf) - 1, "%s/%s",
                file->containing_dir, file->filename);
        stat(buf, &statbuf);
        file->metadata.mod_time = statbuf.st_mtime;
    }
}

static void *metadata_bulk_getter_thread_entry(void *args_voidptr)
{
    Metadata_Getter_Args *args = (Metadata_Getter_Args *)args_voidptr;
    Runtime_Vars *rtvars = args->rtvars;
    rtvars->sflags.metadata_getter_thread_lock = true;
    Music_Data *mdata = rtvars->mdata_ptr;
    File_List *fl = &mdata->music_list;
    Pacmxr_Metadata pm;
    Audio_File *file;
    char tempbuf[METADATA_STRING_SIZE];
    char filename[PATH_MAX];

    Frametime_Vars time;
    time.start = ro_posix_get_timestamp();
    int files_processed = 0;
    uint16_t dir_hash = hash_fnv1a16((uint8_t *)args->added_path, strlen(args->added_path));

    //TODO: this shit isnt platform dependent so just move this somewhere else at some point
    for (int file_index = 0;
        file_index < fl->entry_count;
        ++file_index)
    {
        file = &fl->file_strings[file_index];
        //this doesnt actually change the speed in any way because opening files
        //is mega slow but comparing the hash feels more correct 
        //if (!strcmp(file->containing_dir, args->added_path))
        if (file->containing_dir_hash == dir_hash)
        {
            snprintf(filename, PATH_MAX, "%s/%s", args->added_path, file->filename);

            if (pacmxr_meta_open_file(&pm, filename))
            {
                tempbuf[0] = 0; pacmxr_meta_get_title(&pm, tempbuf, METADATA_STRING_SIZE - 1);
                metadata_string_push(tempbuf,
                        offsetof(List_File_Metadata, title),
                        fl->titles_buf,
                        file_index,
                        mdata);
                tempbuf[0] = 0; pacmxr_meta_get_artist(&pm, tempbuf, METADATA_STRING_SIZE - 1);
                metadata_string_push(tempbuf,
                        offsetof(List_File_Metadata, artist),
                        fl->artist_names_buf,
                        file_index,
                        mdata);
                tempbuf[0] = 0; pacmxr_meta_get_album(&pm, tempbuf, METADATA_STRING_SIZE - 1);
                metadata_string_push(tempbuf,
                        offsetof(List_File_Metadata, album),
                        fl->album_names_buf,
                        file_index,
                        mdata);

                pacmxr_meta_close_file(&pm);
                ++files_processed;
            }
            else
            {
                fprintf(stderr, "failed to open file %s\n", file->filename);
            }
        }
    }

    time.end = ro_posix_get_timestamp();
    time.delta = time.end - time.start;
    platform_dbg_log("[platform_get_metadata_bulk] retrieved metadata for %d files in %.3f ms\n",
            files_processed, ((float)time.delta/1000.0f));

    rtvars->sflags.metadata_getter_thread_lock = false;
    return EXIT_SUCCESS;
}

static void platform_get_metadata_bulk(Runtime_Vars *rtvars, char *added_path)
{
    pthread_t thread_handle;
    static Metadata_Getter_Args args = {};
    args = (Metadata_Getter_Args){ rtvars, added_path };

    platform_dbg_log("attempting to get metadata from directory %s\n", added_path);
    if (pthread_create(&thread_handle, 0,
        metadata_bulk_getter_thread_entry,
        (void *)&args))
    {
        fprintf(stderr, "ERROR: metadata getter thread failed to start.\n");
        return;
    }
    pthread_detach(thread_handle);
}

//this isnt needed anymore thank da lawd
#if 0
static void platform_sort_mlist_mod_date_janky(File_List *file_list,
                                            char ascending,
                                            Runtime_Vars* rtvars)
{
    //TODO: get rid of this retarded shell command shit and write a function
    //instead that just puts identical data in the buffer
    //shouldnt be that hard but im lazy
    if (!file_list->entry_count) { return; }
    int out_file_desc;
    pid_t proc_id;
    const int command_cap = PATH_MAX*3;
    char command[command_cap] = "ls -At1p --zero";
    if (ascending) { strcat(command, " -r"); }
    int command_length = strlen(command);
    for (int dir_index = 0;
         dir_index < file_list->dirs_added;
         ++dir_index)
    {
        char *dirname = file_list->dirnames_string_loclist[dir_index];
        command_length += snprintf(command + command_length,
                                command_cap - command_length,
                                " \"%s\"", dirname);
    }
    //exclude directories
    //(for some reason directories are excluded already without this in most cases?)
    command_length += snprintf(command + command_length,
                            command_cap - command_length,
                            " | grep -vz /");

    int status = ro_posix_run_command(command, &out_file_desc, &proc_id, 1);
    char *scratch_buf = (char *)rtvars->bufgroup_ptr->scratch_space;
    size_t scratch_bytes = rtvars->bufgroup_ptr->scratch_bytes;
    size_t bytes_read = 0, bytes_read_total = 0;
    if (status)
    {
        ro_posix_get_command_output(out_file_desc, scratch_buf, scratch_bytes, 1);
    }
    else
    {
        platform_dbg_log("failed to read shell command output while rebuilding file list\n"
                        "(ro_posix_get_stdout status: %d, file descriptor: %d)\n",
                        status, out_file_desc);
    }

    PAC_ASSERT(FILENAMES_BUFFER_SIZE >= bytes_read_total);
    char *filenames_buf = file_list->filenames_buf;
    //ls -1 --zero outputs the same format of data so we can just do this lol
    memcpy(filenames_buf, scratch_buf, FILENAMES_BUFFER_SIZE);
    Audio_File *string_loclist = file_list->file_strings;
    char *begin_ptr = filenames_buf;
    string_loclist[0].filename = begin_ptr;
    for (int file_index = 1;
        file_index < file_list->entry_count;
        ++file_index)
    {
        while (1)
        {
            if (!begin_ptr[0] && begin_ptr[1])
            {
                ++begin_ptr;
                string_loclist[file_index].filename = begin_ptr;
                break;
            }
            ++begin_ptr;
        }
    }
    set_match_flags((char *)rtvars->bufgroup_ptr->inbuf_search, rtvars->mdata_ptr);
}
#endif

#if 0
static int platform_list_files_simple(char *path, 
                                    File_List *out_flist, 
                                    char sort)
{
    int result = 0;
    dirent *dir_entry;
    DIR *dir_struct = opendir(path);
    if (dir_struct)
    {
        int filename_len;
        char *write_ptr;
        //char **loclist = out_flist->filenames_string_loclist;
        Audio_File *loclist = out_flist->file_strings;
        while (1)
        {
            dir_entry = readdir(dir_struct);
            if (!dir_entry) { break; }

            if (!(!strcmp(".", dir_entry->d_name) || 
                !strcmp("..", dir_entry->d_name)))
            {
                write_ptr = loclist[out_flist->entry_count].filename;
                filename_len = strlen(dir_entry->d_name);
                strncpy(write_ptr, dir_entry->d_name, NAME_MAX - 1);
                write_ptr[filename_len + 1] = 0x0;
                loclist[out_flist->entry_count + 1].filename = write_ptr + filename_len + 1;
                ++out_flist->entry_count;
            }
        }

        if (sort) 
        { sort_file_list_alpha(out_flist, 0); }
        result = 1;
        closedir(dir_struct);
    } else { 
        platform_dbg_log("directory listing failed. reason: failed to initialize directory struct\n"); 
    }
    return result;
}
#endif

static int platform_list_files_mlist(char *path, File_List *out_flist)
{
    int result = 0;
    dirent *dir_entry;
    DIR *dir_struct = opendir(path);
    if (dir_struct)
    {
        file_list_push_dirname(path, out_flist);
        uint16_t dir_hash = hash_fnv1a16((uint8_t *)path, strlen(path));

        int filename_len;
        char *write_ptr;
        //char **loclist = out_flist->filenames_string_loclist;
        Audio_File *loclist = out_flist->file_strings, *file;
        while (1)
        {
            dir_entry = readdir(dir_struct);
            if (!dir_entry) { break; }

            if (dir_entry->d_type == DT_REG)
            {
                file = &loclist[out_flist->entry_count];
                write_ptr = file->filename;
                file->containing_dir = out_flist->dirnames_string_loclist[out_flist->dirs_added - 1];
                file->containing_dir_hash = dir_hash;
                filename_len = strlen(dir_entry->d_name);
                strncpy(write_ptr, dir_entry->d_name, NAME_MAX - 1);
                write_ptr[filename_len + 1] = 0x0;
                loclist[out_flist->entry_count + 1].filename = write_ptr + filename_len + 1;
                ++out_flist->entry_count;
            }
        }
        result = 1;
        closedir(dir_struct);
    }
    else
    {
        platform_dbg_log("directory listing failed. reason: failed to initialize directory struct\n"); 
    }

    return result;
}

static void startup_alloc_buffers(Ro_Heap_Buffer *heapbuf,
                                General_Buffer_Group *bufgroup)
{
#define MEM_INIT_ASSERT(main_buffer, buf2init, size)                                \
    buf2init = ro_buffer_alloc_region(main_buffer, size);                           \
    if (!buf2init)                                                                  \
    {                                                                               \
        platform_dbg_log("failed to init buffer %s\n(unallocated=%u)exiting.\n",    \
                #buf2init, ro_buffer_unallocated_bytes(heapbuf));                   \
        PAC_ASSERT(0 && "not enough memory!");                                      \
    }

    memset(heapbuf->memory, 0, PAC_MAIN_STORAGE_SIZE);
    MEM_INIT_ASSERT(heapbuf, bufgroup->userinfo_buffer,                 USERINFO_BUFFER_SIZE);
    MEM_INIT_ASSERT(heapbuf, bufgroup->music_info_buffer,               DEBUG_BUFFER_SIZE);
    MEM_INIT_ASSERT(heapbuf, bufgroup->conf_file_buffer,                CONFBUFFER_SIZE);
    MEM_INIT_ASSERT(heapbuf, bufgroup->music_current_filename,          PATH_MAX);
    MEM_INIT_ASSERT(heapbuf, bufgroup->inbuf_filename,                  PATH_MAX);
    MEM_INIT_ASSERT(heapbuf, bufgroup->inbuf_search,                    PATH_MAX);
#if PAC_SPECTRUM_ENABLED
    MEM_INIT_ASSERT(heapbuf, bufgroup->fft_complex32_buffer,            FFT_COMPLEX32_BUFFER_SIZE*2);
#endif
    MEM_INIT_ASSERT(heapbuf, bufgroup->flist_matching_indices,          MATCH_FLAGS_BUFFER_SIZE);
    MEM_INIT_ASSERT(heapbuf, bufgroup->flist_filenames_string_loclist,  FILENAMEBUF_LOCATION_LIST_SIZE);
    MEM_INIT_ASSERT(heapbuf, bufgroup->flist_dirnames_string_loclist,   DIRNAMEBUF_LOCATION_LIST_SIZE);
    MEM_INIT_ASSERT(heapbuf, bufgroup->autocomp_string_loclist,         FILENAMEBUF_LOCATION_LIST_SIZE);
    MEM_INIT_ASSERT(heapbuf, bufgroup->flist_filenames_buf,             FILENAMES_BUFFER_SIZE);
    MEM_INIT_ASSERT(heapbuf, bufgroup->flist_dirnames_buf,              DIRNAMES_BUFFER_SIZE);
    MEM_INIT_ASSERT(heapbuf, bufgroup->prev_files_buf,                  PREV_FILES_BUFFER_SIZE);
    MEM_INIT_ASSERT(heapbuf, bufgroup->titles_buf,                      METADATA_STRINGS_BUFSIZE);
    MEM_INIT_ASSERT(heapbuf, bufgroup->artist_names_buf,                METADATA_STRINGS_BUFSIZE);
    MEM_INIT_ASSERT(heapbuf, bufgroup->album_names_buf,                 METADATA_STRINGS_BUFSIZE);
    //MEM_INIT_ASSERT(heapbuf, bufgroup->autocomp_buffer,                 FILENAMES_BUFFER_SIZE);

    bufgroup->scratch_bytes = ro_buffer_unallocated_bytes(heapbuf);
    if (bufgroup->scratch_bytes)
    {
        MEM_INIT_ASSERT(heapbuf, 
                bufgroup->scratch_space, 
                bufgroup->scratch_bytes); 
    }
    platform_dbg_log("scratch: %d bytes\n", bufgroup->scratch_bytes);
}

static uint64_t platform_get_timestamp(void)
{
    return ro_posix_get_timestamp();
}

static char platform_file_exists(char *path)
{
    return ro_posix_file_exists(path);
}

static char platform_directory_exists(char *path)
{
    return ro_posix_directory_exists(path);
}

static char platform_path_exists(char *path)
{
    return ro_posix_path_exists(path);
}

static uint64_t platform_read_file(char *file_path, 
                                char *dest, 
                                uint64_t dest_bytes)
{
    return ro_posix_read_file(file_path, dest, dest_bytes);
}

static int platform_write_file(char *file_path, 
                            void *in_buffer, 
                            uint64_t buffer_size)
{
    return ro_posix_write_file(file_path, in_buffer, buffer_size);
}


static void platform_log(char *fmt_string, ...)
{
    char buf[4096];
    va_list args;
    va_start(args, fmt_string);
    vsnprintf(buf, 4095, fmt_string, args);
    fprintf(stdout, "%s", buf);
    va_end(args);
}

static void platform_dbg_log(char *fmt_string, ...)
{
#if _2PACWAV_DEBUG || (defined(_2PACWAV_ENABLE_LOG) && _2PACWAV_ENABLE_LOG)
    char buf[4096];
    va_list args;
    va_start(args, fmt_string);
    vsnprintf(buf, 4095, fmt_string, args);
    fprintf(stderr, "%s", buf);
    va_end(args);
#endif
}

static void platform_dbg_dump_file(char *containing_dir,
                                void *buffer,
                                size_t buffer_size)
{
    char filename[PATH_MAX];
    snprintf(filename, PATH_MAX, "%s/%ld.2w_dump", containing_dir, time(0));
    platform_write_file(filename, buffer, buffer_size);
}

static void platform_get_working_directory(char *buf, int buf_size)
{
    ro_posix_get_working_directory(buf, buf_size);
    int len = strlen(buf);
    while (buf[len - 1] == '/') 
    { buf[--len] = 0; }
}

int main(int arg_count, char **args)
{
    Runtime_Vars rtvars = {};
    rtvars.sflags.visualizer_enabled = 1;
    General_Buffer_Group bufgroup = {};
    if (ro_posix_make_heap_buffer(&rtvars.main_storage, 
            PAC_MAIN_STORAGE_SIZE))
    {
        startup_alloc_buffers(&rtvars.main_storage, &bufgroup); 
    }
    else
    { 
        fprintf(stderr, "failed to get memory\n"); 
        return -1; 
    }

    Startup_Args sargs = {};
    sargs.bufgroup_ptr = &bufgroup;
    sargs.paths.buffer = (char *)bufgroup.scratch_space;
    sargs.paths.buffer[0] = 0;
    sargs.paths.ptrs[0] = sargs.paths.buffer;
    sargs.font_size = PAC_LATIN_FONTSIZE;
    rtvars.sargs_ptr = &sargs;
    if (pac_do_command_args(arg_count, args, &sargs, &bufgroup)) 
    { return EXIT_SUCCESS; }

    Sdl_Apidata sdldata = {};
    Music_Data mdata = {};
    strcpy(mdata.music_type_buf, "NONE");

    setlocale(LC_ALL, "en_US.UTF-8");
    srand48(time(0));

    rtvars.sdldata_ptr = &sdldata;
    rtvars.bufgroup_ptr = &bufgroup;
    sdldata.mdata_ptr = &mdata;

    if (!pac_init_sdl(&sdldata))
    {
        fprintf(stderr, "%s: failed to initialize SDL library.\n", args[0]);
        return EXIT_FAILURE;
    }
    if (!pac_init_tupacmixer(&mdata))
    {
        fprintf(stderr, "%s: failed to initialize audio mixer.\n", args[0]);
        return EXIT_FAILURE;
    }
    rtvars.pacmxr_ctx = pacmxr_get_context();
    mdata.current_filename = (char *)bufgroup.music_current_filename;
    mdata.rtvars_ptr = &rtvars;

    platform_get_working_directory(rtvars.working_directory, PATH_MAX);
    //platform_get_font_path(&rtvars, (char *)bufgroup.scratch_space, PATH_MAX);
    platform_find_res_path(&rtvars, rtvars.resource_directory, PATH_MAX - 1);

    mdata.music_list.filenames_buf = (char *)bufgroup.flist_filenames_buf;
    mdata.music_list.file_strings = (Audio_File *)bufgroup.flist_filenames_string_loclist;
    mdata.music_list.file_strings[0].filename = (char *)mdata.music_list.filenames_buf;
    mdata.music_list.dirnames_buf = (char *)bufgroup.flist_dirnames_buf;
    mdata.music_list.dirnames_string_loclist = (char **)bufgroup.flist_dirnames_string_loclist;
    mdata.music_list.dirnames_string_loclist[0] = (char *)mdata.music_list.dirnames_buf;
    mdata.music_list.matching_indices = (uint16_t *)bufgroup.flist_matching_indices;

    mdata.music_list.artist_names_buf = (char *)bufgroup.artist_names_buf;
    mdata.music_list.file_strings[0].metadata.artist = (char *)bufgroup.artist_names_buf;
    mdata.music_list.titles_buf = (char *)bufgroup.titles_buf;
    mdata.music_list.file_strings[0].metadata.title = (char *)bufgroup.titles_buf;
    mdata.music_list.album_names_buf = (char *)bufgroup.album_names_buf;
    mdata.music_list.file_strings[0].metadata.album = (char *)bufgroup.album_names_buf;

#if PAC_SPECTRUM_ENABLED
    mdata.astream.complex32_buffer_in = (Complex32 *)bufgroup.fft_complex32_buffer;
    mdata.astream.complex32_buffer_out = mdata.astream.complex32_buffer_in + (FFT_COMPLEX32_BUFFER_SIZE/2);
#endif
    
    mdata.music_list.prev_files.buffer = (char *)bufgroup.prev_files_buf;
    init_prev_file_list(&mdata.music_list.prev_files);

    rtvars.keep_running = 1;

    Frametime_Vars frametime;
    rtvars.frametime_info_ptr = &frametime;
    rtvars.mdata_ptr = &mdata;

    bufgroup.fontpath_ptr = (char *)bufgroup.scratch_space + (bufgroup.scratch_bytes - (PATH_MAX + 1));
    set_default_convars(&rtvars);
    if (!sargs.no_load_conf)
    {
        size_t conf_len = startup_load_conf(&rtvars,
                            (char *)bufgroup.conf_file_buffer, 
                            CONFBUFFER_SIZE);
        if (conf_len && (conf_len < CONFBUFFER_SIZE))
        {
            parse_and_apply_config(&rtvars,
                    (char *)bufgroup.conf_file_buffer,
                    CONFBUFFER_SIZE);
        }
        else
        {
            platform_get_font_path(&rtvars, bufgroup.fontpath_ptr, PATH_MAX);
        }
    }
    else
    {
        platform_get_font_path(&rtvars, bufgroup.fontpath_ptr, PATH_MAX);
    }

    if (sargs.volume != -1)
    {
        mdata.volume = sargs.volume;
        pacmxr_set_volume(mdata.volume);
        mdata.volume = pacmxr_get_volume();
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    //ImGuiIO &io = ImGui::GetIO();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::GetIO().ConfigFlags |= ImGuiWindowFlags_NoSavedSettings;
    ImGui::GetIO().IniFilename = 0;
    ImGui::StyleColorsDark();
    ImGui_ImplOpenGL3_Init("#version 130");
    ImGui_ImplSDL2_InitForOpenGL(sdldata.window_ptr, sdldata.ogl_context);

    if (!pac_imgui_load_font(bufgroup.fontpath_ptr,
            sargs.font_size,
            &rtvars))
    {
        platform_log("[error]: loading fonts failed\n");
        return -1;
    }

    useconds_t us2sleep;

#if _2PACWAV_DEBUG
    {
        int gl_maj, gl_min;
        SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &gl_maj);
        SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &gl_min);            
        platform_dbg_log("OpenGL vendor: %s\n"
                "OpenGL version: %s\n"
                "renderer: %s\n"
                "GLSL version: %s\n"
                "SDL_GL context version: %d.%d\n",
                glGetString(GL_VENDOR),
                glGetString(GL_VERSION),
                glGetString(GL_RENDERER),
                glGetString(GL_SHADING_LANGUAGE_VERSION),
                gl_maj, gl_min);
    }
#endif

    if (sargs.paths.count)
    {
        startup_add_paths(&sargs, &rtvars, &mdata);
        memset(bufgroup.scratch_space, 0, (sargs.paths.count + 2)*PATH_MAX);
    }
    
    //rtvars.autocomp_list.filenames_buf = (char *)bufgroup.autocomp_buffer;
    //rtvars.autocomp_list.filenames_string_loclist = (char **)bufgroup.autocomp_string_loclist;
    //rtvars.autocomp_list.filenames_string_loclist[0] = rtvars.autocomp_list.filenames_buf;

    while (rtvars.keep_running)
    {
        frametime.start = ro_posix_get_timestamp();
        pac_main_loop(&rtvars, &sdldata, &bufgroup, &mdata);
        frametime.end = ro_posix_get_timestamp();
        frametime.delta = frametime.end - frametime.start;

        if ((useconds_t)frametime.delta < MAX_FRAMETIME_MICROSEC)
        {
            us2sleep = (MAX_FRAMETIME_MICROSEC - (useconds_t)frametime.delta);
            ro_posix_sleep_usec(us2sleep);
        }
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    pacmxr_deinit();
    SDL_GL_DeleteContext(sdldata.ogl_context);
    SDL_DestroyWindow(sdldata.window_ptr);
    SDL_Quit();

    return EXIT_SUCCESS;
}
