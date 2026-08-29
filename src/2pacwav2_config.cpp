
/*
File: 2pacwav2_config.cpp
Date: Fri 06 Jun 2025 12:45:47 PM EEST

All of the configuration related things including the parser which
is a very simple (non-recursive) descent parser that was
bootlegged off of the introspecter that my GOAT Casey Muratori wrote on
episode 206 of Handmade Hero (https://www.youtube.com/watch?v=1IwYEJsvdcs)
This means that comments are C & C++ style and lines end on semicolons;
//TODO: remove this semicolon shit
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
    keys->go_next =             ImGuiMod_Ctrl|ImGuiKey_N;
    keys->go_prev =             ImGuiMod_Ctrl|ImGuiKey_P;
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

static void parse_and_apply_keybind(char *stringbuf,
                                int *keybind2modify,
                                char *keybind_name,
                                Runtime_Vars *rtvars)
{
    int len = strlen(stringbuf);
    char *hyphen = 0, *read_ptr = stringbuf;
    int keyname_len;
    int keybind_backup = *keybind2modify;
    *keybind2modify = 0;
    int non_modifiers_encountered = 0;

    char *end_ptr = stringbuf + len;
    char parsing = true;
    while (parsing && (read_ptr < end_ptr))
    {
        len = (end_ptr - read_ptr);
        hyphen = (char *)memchr(read_ptr, '-', len);
        keyname_len = len;
        if (hyphen) { keyname_len = hyphen - read_ptr; }
        if (1 == keyname_len)
        {
            ++non_modifiers_encountered;
            char keyname = tolower(*read_ptr);

            if ((keyname >= '0') && (keyname <= '9'))
            { *keybind2modify |= ((keyname - '0') + ImGuiKey_0); }
            else if ((keyname <= 'z') && (keyname >= 'a'))
            { *keybind2modify |= ((keyname - 'a') + ImGuiKey_A); }
            else
            {
                fprintf(stderr, "2wconf error: only keys 0-9 and a-z are bindable as non-modifier keys.\n"
                        "(in definition of variable %s)\n", keybind_name);
                --non_modifiers_encountered;
                parsing = false;
            }

            if (!hyphen) { parsing = false; }
            else { read_ptr += 2; }
        }
        else
        {
            if (!strncmp(read_ptr, CONF_KEYNAME_CTRL, keyname_len))
            { *keybind2modify |= ImGuiMod_Ctrl; }
            else if (!strncmp(read_ptr, CONF_KEYNAME_SHIFT, keyname_len))
            { *keybind2modify |= ImGuiMod_Shift; }
            else if (!strncmp(read_ptr, CONF_KEYNAME_ALT, keyname_len))
            { *keybind2modify |= ImGuiMod_Alt; }
            else if (!strncmp(read_ptr, CONF_KEYNAME_SPACE, keyname_len))
            { *keybind2modify |= ImGuiKey_Space; ++non_modifiers_encountered; }
            else if (!strncmp(read_ptr, CONF_KEYNAME_TAB, keyname_len))
            { *keybind2modify |= ImGuiKey_Tab; ++non_modifiers_encountered; }
            else
            {
                fprintf(stderr, "2wconf error: unrecognized key name '%.*s' in definition of variable %s.\n",
                        keyname_len, read_ptr, keybind_name);
            }

            if (!hyphen) { parsing = false; }
            read_ptr += keyname_len + 1;
        }
    }

    if (!non_modifiers_encountered)
    {
        fprintf(stderr, "2wconf error: keybinds must have at least 1 non-modifier key. ignoring bind.\n"
                        "(in definition of variable %s)\n", keybind_name);
        *keybind2modify = keybind_backup;
    }
    else if (*keybind2modify < ImGuiKey_NamedKey_BEGIN)
    {
        fprintf(stderr, "2wconf error: attempted to assign variable %s an invalid value. reverting it to default.\n",
                keybind_name);
        platform_dbg_log("(value in question: %d)\n", *keybind2modify);
        *keybind2modify = keybind_backup;
    }
}

static bool config_handle_keybinds(Tokenizer *tokenizer,
                                Token tok,
                                Config_Vars_State *cvs,
                                char *stringbuf, int stringbuf_size,
                                Runtime_Vars *rtvars)
{
    //NOTE: i know this is pretty janky but these must always be in identical
    //order to their corresponding bitfields in the Keybinds struct!!!11
    //otherwise the above function will be subject to modifying the wrong keybinds
    //(see call to parse_and_apply_keybind below to find out why)
    static char *key_config_vars[] =
    {
        "key_vol_up",
        "key_vol_down",
        "key_seek_forward",
        "key_seek_backward",
        "key_seek_to_start",
        "key_cycle_sort",
        "key_clear_list",
        "key_toggle_list_vis",
        "key_toggle_repeat",
        "key_toggle_shuffle",
        "key_reload_metadata",
        "key_reload_config",
        "key_go_next",
        "key_go_prev",
        "key_pause",
        "key_search",
    };
    constexpr int num_vars = sizeof(key_config_vars)/sizeof(key_config_vars[0]);
    static char keybinds_set[num_vars] = {};

    Keybinds *keys = &rtvars->keybinds;
    Ui_Vars *uivars = &rtvars->uivars;
    State_Flags *sflags = &rtvars->sflags;
    Startup_Args *sargs = rtvars->sargs_ptr;
    bool result = false;

    for (int keybind_index = 0;
        keybind_index < num_vars;
        ++keybind_index)
    {
        if (token_equals(tok, key_config_vars[keybind_index]))
        {
            if (!keybinds_set[keybind_index])
            {
                result = true;
                stringbuf[0] = 0;
                Token ret_tok = get_string_entry(tokenizer,
                                    stringbuf,
                                    stringbuf_size - 1,
                                    key_config_vars[keybind_index]);

                if (stringbuf[0] && ret_tok.length)
                {
                    parse_and_apply_keybind(stringbuf,
                            (int *)((uintptr_t)keys + (sizeof(int)*keybind_index)),
                            key_config_vars[keybind_index],
                            rtvars);
                }
            }
            else if (1 == keybinds_set[keybind_index])
            {
                report_duplicate(key_config_vars[keybind_index]);
                ++keybinds_set[keybind_index];
            }
            break;
        }
    }

    return result;
}

static bool config_handle_general_vars(Tokenizer *tokenizer,
                                    Token tok,
                                    Config_Vars_State *cvs,
                                    char *stringbuf, int stringbuf_size,
                                    Runtime_Vars *rtvars)
{
    Ui_Vars *uivars = &rtvars->uivars;
    State_Flags *sflags = &rtvars->sflags;
    Startup_Args *sargs = rtvars->sargs_ptr;
    bool result = false;

    if (token_equals(tok, CONF_FONTSIZE))
    {
        result = true;
        //TODO: maybe just make this possible to do at runtime
        if (!sflags->startup_parse_done)
        {
            if (!cvs->fontsize_set)
            {
                float value = get_float_entry(tokenizer, CONF_FONTSIZE);
                if (value > 0.0f)
                {
                    sargs->font_size = value;
                    ++cvs->fontsize_set;
                }
            }
            else if (1 == cvs->fontsize_set)
            {
                report_duplicate(CONF_FONTSIZE);
                ++cvs->fontsize_set;
            }
        }
    }
    else if (token_equals(tok, CONF_VISUALIZER_STATUS))
    {
        result = true;
        if (!cvs->vis_status_set)
        {
            float value = get_float_entry(tokenizer, CONF_VISUALIZER_STATUS);
            if (value == 0.0f)
            {
                sflags->visualizer_enabled = 0;
                strcpy(rtvars->bufgroup_ptr->vis_toggle_text, "enable visualizer");
            }
            ++cvs->vis_status_set;
        }
        else if (1 == cvs->vis_status_set)
        {
            report_duplicate(CONF_VISUALIZER_STATUS);
            ++cvs->vis_status_set;
        }
    }
    else if (token_equals(tok, CONF_STARTUP_VOLUME))
    {
        result = true;
        if (!cvs->volume_set)
        {
            float value = get_float_entry(tokenizer, CONF_STARTUP_VOLUME);
            value = pacmxr_clamp_float(value, 0.0f, SDL_MIX_MAXVOLUME);
            rtvars->mdata_ptr->volume = value;
            pacmxr_set_volume(value);
            ++cvs->volume_set;
        }
        else if (1 == cvs->volume_set)
        {
            report_duplicate(CONF_STARTUP_VOLUME);
            ++cvs->volume_set;
        }
    }
    else if (token_equals(tok, CONF_VOLUME_STEP))
    {
        result = true;
        if (!cvs->volume_step_set)
        {
            int value = (int)get_float_entry(tokenizer, CONF_VOLUME_STEP);
            sargs->volume_step = pacmxr_clamp_int(value, 0, SDL_MIX_MAXVOLUME);
            ++cvs->volume_step_set;
        }
        else if (1 == cvs->volume_step_set)
        {
            report_duplicate(CONF_VOLUME_STEP);
            ++cvs->volume_step_set;
        }
    }
    else if (token_equals(tok, CONF_FONT_PATH))
    {
        result = true;
        if (!sflags->startup_parse_done)
        {
            if (!cvs->fontpath_set)
            {
                stringbuf[0] = 0;
                char *buf = (char *)rtvars->bufgroup_ptr->fontpath_ptr;
                Token ret_tok = get_string_entry(tokenizer,
                                    stringbuf,
                                    stringbuf_size - 1,
                                    CONF_STARTUP_PATH);

                if (stringbuf[0] && ret_tok.length &&
                    (ret_tok.length + 1 < PATH_MAX))
                {
                    if (platform_file_exists(stringbuf))
                    {
                        snprintf(buf, PATH_MAX, "%s", stringbuf);
                        ++cvs->fontpath_set;
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
            else if (1 == cvs->fontpath_set)
            {
                report_duplicate(CONF_FONT_PATH);
                ++cvs->fontpath_set;
            }
        }
    }
    else if (token_equals(tok, CONF_VISUALIZER_COLOR))
    {
        result = true;
        if (!cvs->vis_color_set)
        {
            if (get_num_array_entry(tokenizer,
                        uivars->vis_color, 4,
                        CONF_VISUALIZER_COLOR))
            {
                apply_num_array_entry(uivars->vis_color, 4, CONF_VISUALIZER_COLOR);
                ++cvs->vis_color_set;
            }
            else
            {
                fprintf(stderr, "2wconf error: could not set visualizer color.\n");
            }
        }
        else if (1 == cvs->vis_color_set)
        {
            report_duplicate(CONF_VISUALIZER_COLOR);
            ++cvs->vis_color_set;
        }
    }
    else if (token_equals(tok, CONF_UI_TEXT_COLOR))
    {
        result = true;
        if (!cvs->text_ui_color_set)
        {
            if (get_num_array_entry(tokenizer,
                        uivars->text_color, 4,
                        CONF_UI_TEXT_COLOR))
            {
                apply_num_array_entry(uivars->text_color, 4, CONF_UI_TEXT_COLOR);
                float *x = uivars->text_color;
                ++cvs->text_ui_color_set;
            }
            else
            {
                fprintf(stderr, "2wconf error: could not set text color.\n");
            }
        }
        else if (1 == cvs->text_ui_color_set)
        {
            report_duplicate(CONF_UI_TEXT_COLOR);
            ++cvs->text_ui_color_set;
        }
    }
    else if (token_equals(tok, CONF_UI_BUTTON_BG_COLOR))
    {
        result = true;
        if (!cvs->button_bg_color_set)
        {
            if (get_num_array_entry(tokenizer,
                        uivars->button_bg_color, 4,
                        CONF_UI_BUTTON_BG_COLOR))
            {
                apply_num_array_entry(uivars->button_bg_color, 4, CONF_UI_BUTTON_BG_COLOR);
                ++cvs->button_bg_color_set;
            }
            else
            {
                fprintf(stderr, "2wconf error: could not set button background color.\n");
            }
        }
        else if (1 == cvs->button_bg_color_set)
        {
            report_duplicate(CONF_UI_BUTTON_BG_COLOR);
            ++cvs->button_bg_color_set;
        }
    }
     
    return result;
}

static void parse_and_apply_config(Runtime_Vars *rtvars, 
                                char *confbuf, 
                                int confbuf_bytes)
{
    Startup_Args *sargs = rtvars->sargs_ptr;
    Tokenizer tokenizer = {0};
    tokenizer.at = confbuf;
    char stringbuf[PATH_MAX];
    //we do this because metadata is retrieved asynchronously and the shit
    //in "stringbuf" will get changed at the same time otherwise
    static char startup_path_buffer[PATH_MAX];
    Config_Vars_State cvs = {};
    Ui_Vars *uivars = &rtvars->uivars;
    State_Flags *sflags = &rtvars->sflags;

    if (sflags->startup_parse_done)
    { runtime_reload_conf(rtvars, confbuf); }

    char parsing = 1;
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
            else
            {
                if (!config_handle_general_vars(&tokenizer,
                                tok,
                                &cvs,
                                stringbuf,
                                sizeof(stringbuf) - 1,
                                rtvars))
                {
                    if (!config_handle_keybinds(&tokenizer,
                                tok,
                                &cvs,
                                stringbuf,
                                sizeof(stringbuf) - 1,
                                rtvars))
                    {
                        fprintf(stderr, "2wconf warning: unknown identifier or variable '%.*s.', ignoring.\n",
                                tok.length, tok.text);
                    }
                }
            }
        } break;

        default:
        {
        } break;
        }
    }

    if (!cvs.fontpath_set && !sflags->startup_parse_done)
    {
        platform_get_font_path(rtvars,
                (char *)rtvars->bufgroup_ptr->fontpath_ptr,
                PATH_MAX);
    }

    sflags->startup_parse_done = true;
}
