
/*
File: 2wmeta.c
Date: Fri 10 Jul 2026 12:15:25 AM EEST

Simple command line interface for the 2pacmixer metadata features
*/

#ifndef PACMXR_ONLY_INCLUDE_METADATA
    #define PACMXR_ONLY_INCLUDE_METADATA 1
#endif
#include "2pacmixer.h"

typedef struct File_List {
    char **filenames;
    int num_files;
} File_List;

typedef struct Tag_Listing_Data {
    int track;
    int year;
#define STRING_TAG_SIZE (1024)
    char title[STRING_TAG_SIZE];
    char artist[STRING_TAG_SIZE];
    char album[STRING_TAG_SIZE];
    char genre[STRING_TAG_SIZE];
} Tag_Listing_Data;

typedef struct Command_Options {
    int arg_count;
    char **args;
    char do_list;
    int edit_flags;
#define EDIT_TITLE_BIT  (1)
#define EDIT_ARTIST_BIT (1 << 1)
#define EDIT_ALBUM_BIT  (1 << 2)
#define EDIT_GENRE_BIT  (1 << 3)
#define EDIT_TRACK_BIT  (1 << 4)
#define EDIT_YEAR_BIT   (1 << 5)
    char *new_title;
    char *new_artist;
    char *new_album;
    char *new_genre;
    char *new_year;
    char *new_track;
} Command_Options;

typedef struct State {
    Pacmxr_Metadata pac_meta;
    Command_Options cmd_opts;
    File_List files;
} State;

static void help(void) {
    printf(
"2wmeta metadata editor\n"
"2wmeta [-l|-ar|-al|-ti|-ge|-ye|-tr] [files]\n"
"-h | --help : show this message\n"
"-l : list metadata in file(s)\n"
"-ti : specify song title\n"
"-ar : specify artist name\n"
"-al : specify album name\n"
"-ge : specify song genre\n"
"-ye : specify release year\n"
"-tr : specify track number\n"
        );
}

static void get_tags(Pacmxr_Metadata *pac_meta, Tag_Listing_Data *tagdata) {
    pacmxr_meta_get_title(pac_meta,
            tagdata->title,
            STRING_TAG_SIZE);
    pacmxr_meta_get_artist(pac_meta,
            tagdata->artist,
            STRING_TAG_SIZE);
    pacmxr_meta_get_album(pac_meta,
            tagdata->album,
            STRING_TAG_SIZE);
    pacmxr_meta_get_genre(pac_meta,
            tagdata->genre,
            STRING_TAG_SIZE);
    tagdata->year = pacmxr_meta_get_year(pac_meta);
    if (tagdata->year == -1) { tagdata->year = 0; }
    tagdata->track = pacmxr_meta_get_track(pac_meta);
    if (tagdata->track == -1) { tagdata->track = 0; }
}

static void list_files(State *state) {
    Tag_Listing_Data tagdata = {0};
    File_List *files = &state->files;

    for (int file_index = 0; file_index < files->num_files; ++file_index) {
        if (pacmxr_meta_open_file(&state->pac_meta, files->filenames[file_index])) {
            get_tags(&state->pac_meta, &tagdata);
            printf("%s:\ntitle: %s\nartist: %s\nalbum: %s\ngenre: %s\nyear: %d\ntrack: %d\n",
                    files->filenames[file_index],
                    tagdata.title,
                    tagdata.artist,
                    tagdata.album,
                    tagdata.genre,
                    tagdata.year,
                    tagdata.track);

            pacmxr_meta_close_file(&state->pac_meta);
        } else {
            fprintf(stderr, "%s: failed to open file: %s\n",
                    state->cmd_opts.args[0], state->files.filenames[file_index]);
        }
    }
}

static void edit_files(State *state) {
    File_List *files = &state->files;
    Command_Options *cmd_opts = &state->cmd_opts;
    uint32_t edit_flags = cmd_opts->edit_flags;

    for (int file_index = 0; file_index < files->num_files; ++file_index) {
        if (pacmxr_meta_open_file(&state->pac_meta, files->filenames[file_index])) {
            if (cmd_opts->new_title) {
                pacmxr_meta_set_title(&state->pac_meta, cmd_opts->new_title);
            } if (cmd_opts->new_artist) {
                pacmxr_meta_set_artist(&state->pac_meta, cmd_opts->new_artist);
            } if (cmd_opts->new_album) {
                pacmxr_meta_set_album(&state->pac_meta, cmd_opts->new_album);
            } if (cmd_opts->new_genre) {
                pacmxr_meta_set_genre(&state->pac_meta, cmd_opts->new_genre);
            } if (cmd_opts->new_year) {
                errno = 0;
                int value = strtol(cmd_opts->new_year, 0, 10);
                if (!errno) {
                    pacmxr_meta_set_year(&state->pac_meta, value);
                } else {
                    fprintf(stderr, "%s: could not interpret value after -ye as an interger\n", cmd_opts->args[0]);
                }
            } if (cmd_opts->new_track) {
                errno = 0;
                int value = strtol(cmd_opts->new_track, 0, 10);
                if (!errno) {
                    pacmxr_meta_set_track(&state->pac_meta, value);
                } else {
                    fprintf(stderr, "%s: could not interpret value after -tr as an interger\n", cmd_opts->args[0]);
                }
            }

            pacmxr_meta_save(&state->pac_meta);
            pacmxr_meta_close_file(&state->pac_meta);
        } else {
            fprintf(stderr, "%s: failed to open file: %s\n",
                    state->cmd_opts.args[0], state->files.filenames[file_index]);
        }
    }
}

static void set_edit_option(State *state,
                        int *arg_index,
                        char *option,
                        uint32_t edit_flag,
                        char **ptr2set) {
    char **args = state->cmd_opts.args;
    if (args[*arg_index + 1]) {
        state->cmd_opts.edit_flags |= edit_flag;
        *ptr2set = args[*arg_index + 1];
        *arg_index += 1;
    } else {
        fprintf(stderr, "%s: expected argument after %s.\n", args[0], option);
    }
}

int main(int arg_count, char **args) {
    if (arg_count < 2) {
        help(); return 1;
    }

    State state = {0};
    state.cmd_opts.edit_flags = 0;
    state.cmd_opts.args = args;
    state.cmd_opts.arg_count = arg_count;

    //xaxaxaxa
    char **_file_ptr_array = (char **)alloca(arg_count*sizeof(char *));
    state.files.filenames = _file_ptr_array;
    Command_Options *cmd_opts = &state.cmd_opts;

    char *arg;
    for (int arg_index = 1; arg_index < arg_count; ++arg_index) {
        arg = args[arg_index];
        if (!strcmp(arg, "--")) {
            break;
        } else if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
            help();
            return 0;
        } else if (!strcmp(arg, "-l")) {
            cmd_opts->do_list = 1;
        } else if (!strcmp(arg, "-ti")) {
            set_edit_option(&state, &arg_index, arg, EDIT_TITLE_BIT, &state.cmd_opts.new_title);
        } else if (!strcmp(arg, "-ar")) {
            set_edit_option(&state, &arg_index, arg, EDIT_ARTIST_BIT, &state.cmd_opts.new_artist);
        } else if (!strcmp(arg, "-al")) {
            set_edit_option(&state, &arg_index, arg, EDIT_ALBUM_BIT, &state.cmd_opts.new_album);
        } else if (!strcmp(arg, "-ge")) {
            set_edit_option(&state, &arg_index, arg, EDIT_GENRE_BIT, &state.cmd_opts.new_genre);
        } else if (!strcmp(arg, "-ye")) {
            set_edit_option(&state, &arg_index, arg, EDIT_YEAR_BIT, &state.cmd_opts.new_year);
        } else if (!strcmp(arg, "-tr")) {
            set_edit_option(&state, &arg_index, arg, EDIT_TRACK_BIT, &state.cmd_opts.new_track);
        } else {
            state.files.filenames[state.files.num_files] = arg;
            ++state.files.num_files;
        }
    }

    if (state.cmd_opts.edit_flags) {
        if (state.files.num_files > 0)  {
            edit_files(&state);
        } else {
            fprintf(stderr, "%s: specified editing options but no files were supplied.\n", state.cmd_opts.args[0]);
        }
    }

    if (state.cmd_opts.do_list) {
        if (state.files.num_files > 0) {
            list_files(&state);
        } else {
            fprintf(stderr, "%s: used -l but no files were supplied.\n", state.cmd_opts.args[0]);
        }
    }

    return 0;
}
