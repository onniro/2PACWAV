
/*
File: 2pacwav2_playlist.cpp
Date: Sat 24 Jan 2026 03:34:19 PM EET

//example playlist

//full paths to files
"~/music/zyzz/tevvez_legend.mp3";
"~/music/nightcore/im_sippin_crush_nightcore_hyperborea_edit.opus";

//path to directory followed by ; (entire directory gets added)
"~/music/drain_gang/";

//if multiple files are in the same directory, you can specify the directory
//just once like this
"~/music/tupac/"
{
    "tupac_all_eyez_on_me.mp3";
    "tupac_california_love.mp3";
    "tupac_changes.mp3";
}

note: although I have confirmed that this code can handle a simple and minimal test
case such as the commented stuff above, this code still probably has crazy bugs!!!
*/

#include "2pacwav2.h"
#include "2pacwav2_parser.h"

//why is this here
#define LAZY_ERROR() fprintf(stderr, "ERROR @ %s:%d\n", __FILE__, __LINE__)

#define PLAYLIST_EXTENSION ".2w_playlist"

static uint64_t load_playlist(char *playlist_path,
                            char *dest,
                            uint32_t dest_bytes,
                            Runtime_Vars *rtvars)
{
    uint64_t playlist_size = platform_read_file(playlist_path, dest, dest_bytes);
    return playlist_size;
}

static void process_playlist(Runtime_Vars *rtvars, char *playlist_path)
{
    char *playlist_buffer = (char *)rtvars->bufgroup_ptr->scratch_space;
    uint32_t buf_bytes = rtvars->bufgroup_ptr->scratch_bytes;
    uint64_t playlist_bytes = load_playlist(playlist_path,
                                    playlist_buffer,
                                    buf_bytes,
                                    rtvars);
    if (!playlist_bytes)
    { return; }

    char parsing = 1;
    char stringbuf[PATH_MAX];
    char dirname[PATH_MAX];
    char filename[NAME_MAX];
    Tokenizer tokenizer = {0};
    tokenizer.at = playlist_buffer;
    while (parsing)
    {
        Token token = get_token(&tokenizer);
        switch (token.type)
        {
        case TOKEN_STREAM_END: { parsing = 0; } break;

        case TOKEN_UNKNOWN:
        {
            fprintf(stderr, 
                    "playlist warning: unknown token encountered: %s\n",
                    token.text);
            parsing = 0;
        } break;

        case TOKEN_STRING:
        {
            get_string(&token, stringbuf, sizeof(stringbuf));
            if (platform_file_exists(stringbuf))
            {
                add_to_music_list(stringbuf, rtvars->mdata_ptr, rtvars);
                if (!require_token(&tokenizer, TOKEN_SEMICOLON))
                {
                    fprintf(stderr, "error: lines must end on a semicolon\n");
                    break;
                }
            }
            else if (platform_directory_exists(stringbuf))
            {
                Token after_string = get_token(&tokenizer);
                if (after_string.type == TOKEN_SEMICOLON)
                {
                    platform_dbg_log("loading path %s\n", stringbuf);
                    add_to_music_list(stringbuf, rtvars->mdata_ptr, rtvars);
                }
                else if (after_string.type == TOKEN_OPEN_BRACE)
                {
                    snprintf(dirname, PATH_MAX, "%s", stringbuf);
                    while (1)
                    {
                        Token in_block = get_token(&tokenizer);
                        if (in_block.type == TOKEN_STRING)
                        {
                            get_string(&in_block, filename, NAME_MAX);
                            snprintf(stringbuf, PATH_MAX, "%s/%s", dirname, filename);
                            add_to_music_list(stringbuf, rtvars->mdata_ptr, rtvars);
                        }
                        else if (in_block.type == TOKEN_SEMICOLON)
                        {
                            continue;
                        }
                        else
                        {
                            if (in_block.type != TOKEN_CLOSED_BRACE)
                            {
                                fprintf(stderr, "playlist warning: weird token encountered in playlist block\n");
                            }
                            break;
                        }
                    }
                }
            }
            else
            {
                fprintf(stderr, "%s: no such file or directory\n", stringbuf);
            }
        } break;

        default: { parsing = 0; } break;
        }
    }
}
