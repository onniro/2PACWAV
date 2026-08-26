
/*
File: 2pacwav2_config.cpp
Date: Fri 06 Jun 2025 12:45:47 PM EEST

All of the configuration related things including the parser which
is a very simple (non-recursive) descent parser that was
bootlegged off of the introspecter that my GOAT Casey Muratori wrote on
episode 206 of Handmade Hero (https://www.youtube.com/watch?v=1IwYEJsvdcs)
This means that comments are C & C++ style and lines end on semicolons;
*/

#include <string.h>

#include "2pacwav2.h"

#include "2pacwav2_parser.h"

static void set_default_convars(Runtime_Vars *rtvars)
{
    Ui_Vars *uivars = &rtvars->uivars;
    uivars->vis_color[0] = 0.7f;
    uivars->vis_color[1] = 0.1f;
    uivars->vis_color[2] = 0.1f;
    uivars->vis_color[3] = 1.0f;

    uivars->text_color[0] = 0.9f; 
    uivars->text_color[1] = 0.9f;
    uivars->text_color[2] = 0.9f;
    uivars->text_color[3] = 1.0f;

    uivars->button_bg_color[0] = 0.1f; 
    uivars->button_bg_color[1] = 0.1f;
    uivars->button_bg_color[2] = 0.1f;
    uivars->button_bg_color[3] = 1.0f;

    rtvars->sargs_ptr->volume_step = PAC_DEFAULT_VOLUME_INCREMENT;
}

static void set_default_keybinds(Runtime_Vars *rtvars)
{
    Keybinds *keys = &rtvars->keybinds;

    keys->vol_up =              ImGuiMod_Ctrl|ImGuiKey_UpArrow;
    keys->vol_down =            ImGuiMod_Ctrl|ImGuiKey_DownArrow;
    keys->seek_forward =        ImGuiMod_Ctrl|ImGuiKey_LeftArrow;
    keys->seek_backward =       ImGuiMod_Ctrl|ImGuiKey_RightArrow;
    keys->seek_to_start =       ImGuiMod_Ctrl|ImGuiKey_Home;
    keys->cycle_sort =          ImGuiMod_Ctrl|ImGuiMod_Shift|ImGuiKey_S;
    keys->clear_list =          ImGuiMod_Ctrl|ImGuiMod_Shift|ImGuiKey_X;
    keys->toggle_list_vis =     ImGuiMod_Ctrl|ImGuiKey_L;
    keys->toggle_repeat =       ImGuiMod_Ctrl|ImGuiKey_R;
    keys->toggle_shuffle =      ImGuiMod_Ctrl|ImGuiKey_S;
    keys->reload_metadata =     ImGuiMod_Ctrl|ImGuiMod_Shift|ImGuiKey_M;
    keys->reload_config =       ImGuiMod_Ctrl|ImGuiMod_Shift|ImGuiKey_G;
    keys->goto_next =           ImGuiMod_Ctrl|ImGuiKey_N;
    keys->goto_prev =           ImGuiMod_Ctrl|ImGuiKey_P;
    keys->pause =               ImGuiKey_Space;
    keys->search =              ImGuiMod_Ctrl|ImGuiKey_F;
}

static size_t runtime_reload_conf(Runtime_Vars *rtvars, char *confbuf)
{
    char *confpath = (char *)rtvars->conf_directory;
    size_t bytes_read =  platform_read_file(confpath,
                            confbuf,
                            CONFBUFFER_SIZE - 1);
    confbuf[bytes_read] = 0;
    return bytes_read;
}

static size_t startup_load_conf(Runtime_Vars *rtvars, 
                                char *confbuf, 
                                int confbuf_bytes)
{
    char confpath[PATH_MAX]; confpath[0] = 0;
    const int infosize = PATH_MAX + sizeof("loaded config: ");
    char infobuf[infosize];
    size_t bytes_read = 0;

#if _2PACWAV_LINUX
    if (rtvars->sargs_ptr->conf_path)
    {
        snprintf(confpath, PATH_MAX, "%s", rtvars->sargs_ptr->conf_path);
    }
    else
    {
        snprintf(confpath, PATH_MAX, "%s/%s", 
                rtvars->working_directory, PAC_CONFNAME_STRING);
    }
#elif _2PACWAV_WIN32
    snprintf(confpath, PATH_MAX - 1, "%s\\%s", 
            rtvars->working_directory, PAC_CONFNAME_STRING);
#endif

    if (platform_file_exists(confpath))
    {
        snprintf(rtvars->conf_directory, PATH_MAX, "%s", confpath);
        bytes_read = platform_read_file(confpath, confbuf, confbuf_bytes - 1);
        confbuf[bytes_read] = 0;
        snprintf(infobuf, infosize, "loaded config: %s", confpath);
        set_userinfo(rtvars, infobuf, USERINFO_TYPE_NOTE);
    }
    else
    {
#if _2PACWAV_LINUX
        char *username = getlogin();
        if (username)
        {
            snprintf(confpath, PATH_MAX,
                    "/home/%s/.config/2pacwav/%s",
                    username, PAC_CONFNAME_STRING);

            if (platform_file_exists(confpath))
            {
                snprintf(rtvars->conf_directory, PATH_MAX, "%s", confpath);
                bytes_read = platform_read_file(confpath, confbuf, confbuf_bytes - 1);
                confbuf[bytes_read] = 0;
                snprintf(infobuf, infosize, "loaded config: %s", confpath);
                set_userinfo(rtvars, infobuf, USERINFO_TYPE_NOTE);
            }
        }
#endif
    }

    if (confpath[0])
    {
        platform_dbg_log("config path: %s\n", confpath);
    }

    return bytes_read;
}

static inline void report_missing_semicolon(char *identifier)
{
    fprintf(stderr, 
            "2wconf syntax error: semicolon (;) required after definition of variable %s\n",
            identifier);
}

static inline void report_missing_equalsign(char *identifier)
{
    fprintf(stderr, "2wconf syntax error: missing equal sign (=) after identifier %s\n",
            identifier);
}

static inline void report_duplicate(char *identifier)
{
    fprintf(stderr, 
            "2wconf warning: variable %s set more than once. ignoring all but the first value.\n",
            identifier);
}

static inline void eat_whitespace(Tokenizer *tokenizer)
{
    while (1)
    {
        if (is_whitespace(tokenizer->at[0]))
        {
            ++tokenizer->at;
        }
        else if ((tokenizer->at[0] == '/') &&
            (tokenizer->at[1] == '/'))
        {
            //c++ style comment
            tokenizer->at += 2;
            while (tokenizer->at[0] && 
                !is_eol(tokenizer->at[0])) 
            { ++tokenizer->at; }
        }
        else if ((tokenizer->at[0] == '/') &&
            /*c style comment*/
            (tokenizer->at[1] == '*'))
        {
            tokenizer->at += 2;
            while (tokenizer->at[0] && 
                !((tokenizer->at[0] == '*') &&
                (tokenizer->at[1] == '/'))) 
            { ++tokenizer->at; }
            if (tokenizer->at[0] == '*') 
            { tokenizer->at += 2; }
        }
        else
        {
            break;
        }
    }
}

static inline char require_token(Tokenizer *tokenizer, Token_Type req_tok)
{
    Token tok = get_token(tokenizer);
    char ret = (tok.type == req_tok);
    return ret;
}

static Token get_token(Tokenizer *tokenizer)
{
    eat_whitespace(tokenizer);
    Token tok = {};
    tok.length = 1;
    tok.text = tokenizer->at;
    char c = tokenizer->at[0];
    ++tokenizer->at;

    switch (c)
    {
    case '\0': { tok.type = TOKEN_STREAM_END; } break;
    case '(': { tok.type = TOKEN_OPEN_PARENTHESIS; } break;
    case ')': { tok.type = TOKEN_CLOSED_PARENTHESIS; } break;
    case ';': { tok.type = TOKEN_SEMICOLON; } break;
    case ':': { tok.type = TOKEN_COLON; } break;
    case ',': { tok.type = TOKEN_COMMA; } break;
    case '*': { tok.type = TOKEN_ASTERISK; } break;
    case '[': { tok.type = TOKEN_OPEN_BRACKET; } break;
    case ']': { tok.type = TOKEN_CLOSED_BRACKET; } break;
    case '{': { tok.type = TOKEN_OPEN_BRACE; } break;
    case '}': { tok.type = TOKEN_CLOSED_BRACE; } break;
    case '=': { tok.type = TOKEN_EQUALSIGN; } break;

    case '"':
    {
        tok.type = TOKEN_STRING;
        tok.text = tokenizer->at;
        while (tokenizer->at[0] &&
            tokenizer->at[0] != '"')
        {
            if ((tokenizer->at[0] == '\\') &&
                (tokenizer->at[1])) 
            { ++tokenizer->at; }
            ++tokenizer->at;
        }
        tok.length = tokenizer->at - tok.text;
        if (tokenizer->at[0] == '"') 
        { ++tokenizer->at; }
    } break;

    default:
    {
        if (is_alpha(c))
        {
            tok.type = TOKEN_IDENTIFIER;
            while (is_alpha(tokenizer->at[0]) ||
                (is_number(tokenizer->at[0]) ||
                tokenizer->at[0] == '_')) 
            { ++tokenizer->at; }
            tok.length = tokenizer->at - tok.text;
        }
        else if (is_number(c) ||
            ((c == '-') && (is_number(tokenizer->at[1]))))
        {
            tok.type = TOKEN_NUMBER;
            tok.text = tokenizer->at - 1;
            while (is_number(tokenizer->at[0]) ||
                (tokenizer->at[0] == '.'))
            { ++tokenizer->at; }
            tok.length = tokenizer->at - tok.text;
        }
        else
        {
            tok.type = TOKEN_UNKNOWN;
        }
    } break;
    }

    return tok;
}

static Token get_string_entry(Tokenizer *tokenizer,
                            char *dest,
                            int dest_size,
                            char *identifier)
{
    Token tok = {};
    if (require_token(tokenizer, TOKEN_EQUALSIGN))
    {
        tok = get_token(tokenizer);
        if (tok.type == TOKEN_STRING)
        {
            if ((tok.length + 1) < (dest_size - 1))
            {
                if (require_token(tokenizer, TOKEN_SEMICOLON))
                {
                    snprintf(dest, tok.length + 1, "%s", tok.text);
                }
                else
                {
                    report_missing_semicolon(identifier);
                }
            }
            else
            {
                fprintf(stderr, 
                        "2wconf syntax error: invalid path in definition of variable %s\n",
                        identifier);
            }
        }
        else
        {
            fprintf(stderr, 
                    "2wconf syntax error: identifier %s must be followed by a string\n",
                    identifier);
        }
    }
    else
    {
        report_missing_equalsign(identifier);
    }
    return tok;
}

static void get_string(Token *token, char *dest, int dest_size)
{
    if ((token->length + 1) < dest_size)
    {
        snprintf(dest, token->length + 1, "%s", token->text);
    }
}

static float get_float_entry(Tokenizer *tokenizer, char *identifier)
{
    Token tok = {};
    float ret = 0.0f;
    if (require_token(tokenizer, TOKEN_EQUALSIGN))
    {
        tok = get_token(tokenizer);
        if (tok.type == TOKEN_NUMBER)
        {
            char *endptr = tok.text + tok.length;
            errno = 0;
            ret = strtof(tok.text, &endptr);
            if ((0.0f == ret) && (errno == ERANGE))
            {
                fprintf(stderr, "2wconf error: could not set variable %s to specified value because it might be invalid.\n",
                        identifier);
            }
            if (!require_token(tokenizer, TOKEN_SEMICOLON))
            {
                ret = 0.0f;
                report_missing_semicolon(identifier);
            }
        }
        else
        {
            fprintf(stderr, "2wconf syntax error: expected number after identifier %s.",
                    identifier);
        }
    }
    else
    {
        report_missing_equalsign(identifier);
    }
    return ret;
}

static int get_num_array_entry(Tokenizer *tokenizer,
                                    float *array,
                                    int array_count,
                                    char *identifier)
{
    int ret = 0;
    if (require_token(tokenizer, TOKEN_EQUALSIGN))
    {
        if (require_token(tokenizer, TOKEN_OPEN_BRACE))
        {
            float *temp_values = (float *)alloca(array_count*sizeof(float));
            //memset((void *)temp_values, 0, array_count*sizeof(float));
            Token tok = {};

            int i;
            for (i = 0; i < array_count; ++i)
            {
                tok = get_token(tokenizer);
                if (TOKEN_NUMBER == tok.type)
                {
                    char *endptr = tok.text + tok.length;
                    temp_values[i] = strtof(tok.text, &endptr);

                    if ((i != array_count - 1) &&
                        !require_token(tokenizer, TOKEN_COMMA))
                    {
                        fprintf(stderr, "2wconf syntax error: missing comma after array member in definition of variable %s.\n"
                                "This variable expects a %d-component array of numerical values.\n",
                                identifier, array_count);
                        break;
                    }
                }
                else
                {
                    fprintf(stderr, "2wconf syntax error: invalid value (or no value) in definition of variable %s.\n"
                            "This variable expects a %d-component array of numerical values.\n",
                            identifier, array_count);
                    break;
                }
            }

            if (i == array_count)
            {
                memcpy(array, temp_values, sizeof(float)*array_count);
                ret = 1;
            }
        }
        else
        {
            fprintf(stderr, "2wconf syntax error: missing open brace in definition of variable %s.\n",
                    identifier);
        }
    }
    else
    {
        report_missing_equalsign(identifier);
    }

    return ret;
}

static void apply_num_array_entry(float *array, int array_length, char *variable)
{
    char warning = 0;
    float value;
    for (int index = 0; index < array_length; ++index)
    {
        value = array[index];
        if (((value < 0.0f) || (value > 1.0f)) && !warning)
        {
            array[index] = pacmxr_clamp_float(value, 0.0f, 1.0f);
            fprintf(stderr,
                    "2wconf warning: values in definition of variable %s should be between 0.0 and 1.0.\n"
                    "For instance: value %g clamped to %g.\n",
                    variable, value, array[index]);
            ++warning;
        }
    }
}

static void parse_and_apply_config(Runtime_Vars *rtvars, 
                                char *confbuf, 
                                int confbuf_bytes)
{
    char parsing = 1;
    Startup_Args *sargs = rtvars->sargs_ptr;
    Tokenizer tokenizer = {0};
    tokenizer.at = confbuf;
    char stringbuf[PATH_MAX];
    //we do this because metadata is retrieved asynchronously and the shit
    //in "stringbuf" will get changed at the same time
    static char startup_path_buffer[PATH_MAX];
    char fontsize_set = 0,
            fontpath_set = 0,
            vis_status_set = 0,
            volume_step_set = 0,
            volume_set = 0,
            vis_color_set = 0,
            text_ui_color_set = 0,
            button_bg_color_set = 0;
    Ui_Vars *uivars = &rtvars->uivars;
    State_Flags *sflags = &rtvars->sflags;

    if (sflags->startup_parse_done)
    { runtime_reload_conf(rtvars, confbuf); }

    while (parsing)
    {
        Token tok = get_token(&tokenizer);
        switch (tok.type)
        {
        case TOKEN_STREAM_END: { parsing = 0; } break;

        case TOKEN_UNKNOWN:
        {
            fprintf(stderr, 
                    "2wconf warning: unknown token encountered: %s\n",
                    tok.text);
        } break;

        //check and handle tokens here
        case TOKEN_IDENTIFIER:
        {
            if (token_equals(tok, CONF_STARTUP_PATH))
            {
                if (!sflags->startup_parse_done)
                {
                    startup_path_buffer[0] = 0;
                    Token ret_tok = get_string_entry(&tokenizer,
                                        startup_path_buffer,
                                        sizeof(startup_path_buffer),
                                        CONF_STARTUP_PATH);
                    if (startup_path_buffer[0] && ret_tok.length &&
                        !rtvars->sargs_ptr->no_load_startup_paths)
                    {
                        platform_dbg_log("loading startup path %s\n", stringbuf);
                        add_to_music_list(startup_path_buffer, rtvars->mdata_ptr, rtvars);
                    }
                }
            }
            else if (token_equals(tok, CONF_FONTSIZE))
            {
                //TODO: maybe just make this possible to do at runtime
                if (!sflags->startup_parse_done)
                {
                    if (!fontsize_set)
                    {
                        float value = get_float_entry(&tokenizer, CONF_FONTSIZE);
                        if (value > 0.0f)
                        {
                            sargs->font_size = value;
                            ++fontsize_set;
                        }
                    }
                    else if (1 == fontsize_set)
                    {
                        report_duplicate(CONF_FONTSIZE);
                        ++fontsize_set;
                    }
                }
            }
            else if (token_equals(tok, CONF_VISUALIZER_STATUS))
            {
                if (!vis_status_set)
                {
                    float value = get_float_entry(&tokenizer, CONF_VISUALIZER_STATUS);
                    if (value == 0.0f)
                    {
                        sflags->visualizer_enabled = 0;
                        strcpy(rtvars->bufgroup_ptr->vis_toggle_text, "enable visualizer");
                    }
                    ++vis_status_set;
                }
                else if (1 == vis_status_set)
                {
                    report_duplicate(CONF_VISUALIZER_STATUS);
                    ++vis_status_set;
                }
            }
            else if (token_equals(tok, CONF_STARTUP_VOLUME))
            {
                if (!volume_set)
                {
                    float value = get_float_entry(&tokenizer, CONF_STARTUP_VOLUME);
                    value = pacmxr_clamp_float(value, 0.0f, SDL_MIX_MAXVOLUME);
                    rtvars->mdata_ptr->volume = value;
                    pacmxr_set_volume(value);
                    ++volume_set;
                }
                else if (1 == volume_set)
                {
                    report_duplicate(CONF_STARTUP_VOLUME);
                    ++volume_set;
                }
            }
            else if (token_equals(tok, CONF_VOLUME_STEP))
            {
                if (!volume_step_set)
                {
                    int value = (int)get_float_entry(&tokenizer, CONF_VOLUME_STEP);
                    sargs->volume_step = pacmxr_clamp_int(value, 0, SDL_MIX_MAXVOLUME);
                    ++volume_step_set;
                }
                else if (1 == volume_step_set)
                {
                    report_duplicate(CONF_VOLUME_STEP);
                    ++volume_step_set;
                }
            }
            else if (token_equals(tok, CONF_FONT_PATH))
            {
                if (!sflags->startup_parse_done)
                {
                    if (!fontpath_set)
                    {
                        stringbuf[0] = 0;
                        char *buf = (char *)rtvars->bufgroup_ptr->fontpath_ptr;
                        Token ret_tok = get_string_entry(&tokenizer,
                                            stringbuf,
                                            sizeof(stringbuf),
                                            CONF_STARTUP_PATH);

                        if (stringbuf[0] && ret_tok.length &&
                            (ret_tok.length + 1 < PATH_MAX))
                        {
                            if (platform_file_exists(stringbuf))
                            {
                                snprintf(buf, PATH_MAX, "%s", stringbuf);
                                ++fontpath_set;
                            }
                            else
                            {
                                fprintf(stderr, "2wconf error: specified font path doesn't exist on the system.\n");
                            }
                        }
                        else
                        {
                            fprintf(stderr, "2wconf error: failed setting font path.\n");
                        }
                    }
                    else if (1 == fontpath_set)
                    {
                        report_duplicate(CONF_FONT_PATH);
                        ++fontpath_set;
                    }
                }
            }
            else if (token_equals(tok, CONF_VISUALIZER_COLOR))
            {
                if (!vis_color_set)
                {
                    if (get_num_array_entry(&tokenizer,
                                uivars->vis_color, 4,
                                CONF_VISUALIZER_COLOR))
                    {
                        apply_num_array_entry(uivars->vis_color, 4, CONF_VISUALIZER_COLOR);
                        ++vis_color_set;
                    }
                    else
                    {
                        fprintf(stderr, "2wconf error: could not set visualizer color.\n");
                    }
                }
                else if (1 == vis_color_set)
                {
                    report_duplicate(CONF_VISUALIZER_COLOR);
                    ++vis_color_set;
                }
            }
            else if (token_equals(tok, CONF_UI_TEXT_COLOR))
            {
                if (!text_ui_color_set)
                {
                    if (get_num_array_entry(&tokenizer,
                                uivars->text_color, 4,
                                CONF_UI_TEXT_COLOR))
                    {
                        apply_num_array_entry(uivars->text_color, 4, CONF_UI_TEXT_COLOR);
                        float *x = uivars->text_color;
                        ++text_ui_color_set;
                    }
                    else
                    {
                        fprintf(stderr, "2wconf error: could not set text color.\n");
                    }
                }
                else if (1 == text_ui_color_set)
                {
                    report_duplicate(CONF_UI_TEXT_COLOR);
                    ++text_ui_color_set;
                }
            }
            else if (token_equals(tok, CONF_UI_BUTTON_BG_COLOR))
            {
                if (!button_bg_color_set)
                {
                    if (get_num_array_entry(&tokenizer,
                                uivars->button_bg_color, 4,
                                CONF_UI_BUTTON_BG_COLOR))
                    {
                        apply_num_array_entry(uivars->button_bg_color, 4, CONF_UI_BUTTON_BG_COLOR);
                        ++button_bg_color_set;
                    }
                    else
                    {
                        fprintf(stderr, "2wconf error: could not set button background color.\n");
                    }
                }
                else if (1 == button_bg_color_set)
                {
                    report_duplicate(CONF_UI_BUTTON_BG_COLOR);
                    ++button_bg_color_set;
                }
            }
            else
            {
                snprintf(stringbuf, tok.length + 1, "%s", tok.text);
                fprintf(stderr, "2wconf warning: unknown identifier or variable %s. ignoring.\n",
                        stringbuf);
            }
        } break;

        default:
        {
        } break;
        }
    }

    if (!fontpath_set && !sflags->startup_parse_done)
    {
        platform_get_font_path(rtvars,
                (char *)rtvars->bufgroup_ptr->fontpath_ptr,
                PATH_MAX);
    }

    sflags->startup_parse_done = true;
}
