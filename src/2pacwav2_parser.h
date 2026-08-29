
/*
File: 2pacwav2_parser.h
Date: Sat 24 Jan 2026 03:41:31 PM EET
*/

#ifndef _2PACWAV2_PARSER_DOT_H

#define CONF_STARTUP_PATH               "startup_path"
#define CONF_FONTSIZE                   "font_size"
#define CONF_FONT_PATH                  "font_path"
#define CONF_VISUALIZER_STATUS          "visualizer"
#define CONF_VISUALIZER_COLOR           "visualizer_color"
#define CONF_STARTUP_VOLUME             "volume"
#define CONF_VOLUME_STEP                "volume_step"
#define CONF_UI_TEXT_COLOR              "text_color"
#define CONF_UI_BUTTON_BG_COLOR         "button_bg_color"

#define CONF_KEYNAME_CTRL               "ctrl"
#define CONF_KEYNAME_SHIFT              "shift"
#define CONF_KEYNAME_ALT                "alt"
#define CONF_KEYNAME_SPACE              "space"
#define CONF_KEYNAME_TAB                "tab"

typedef enum Token_Type
{
    TOKEN_IDENTIFIER,
    TOKEN_OPEN_PARENTHESIS,
    TOKEN_CLOSED_PARENTHESIS,
    TOKEN_EQUALSIGN,
    TOKEN_COLON,
    TOKEN_COMMA,
    TOKEN_SEMICOLON,
    TOKEN_STRING,
    TOKEN_NUMBER,
    TOKEN_ASTERISK,
    TOKEN_OPEN_BRACKET,
    TOKEN_CLOSED_BRACKET,
    TOKEN_OPEN_BRACE,
    TOKEN_CLOSED_BRACE,
    TOKEN_UNKNOWN,
    TOKEN_STREAM_END,
} Token_Type;

typedef struct Token
{
    Token_Type type;
    int length;
    char *text;
} Token;

typedef struct Tokenizer
{
    char *at;
} Tokenizer;

typedef struct Config_Vars_State
{
    char fontsize_set;
    char fontpath_set;
    char vis_status_set;
    char volume_step_set;
    char volume_set;
    char vis_color_set;
    char text_ui_color_set;
    char button_bg_color_set;
    char key_vol_up_set;
} Config_Vars_State;

static inline char is_eol(char c);
static inline char is_whitespace(char c);
static inline char is_alpha(char c);
static inline char is_number(char c);
static inline char token_equals(Token tok, char *match);
static void eat_whitespace(Tokenizer *tokenizer);
static inline char require_token(Tokenizer *tokenizer, Token_Type req_tok);
static Token get_token(Tokenizer *tokenizer);
static Token get_string_entry(Tokenizer *tokenizer, char *dest, int dest_size, char *identifier);
static float get_float_entry(Tokenizer *tokenizer, char *identifier);
static void parse_and_apply_config(Runtime_Vars *rtvars, char *confbuf, int confbuf_bytes);


static inline char is_eol(char c)
{
    char ret = ((c == '\n') ||
                (c == '\r'));
    return ret;
}

static inline char is_whitespace(char c)
{
    char ret = ((c == ' ') ||
                (c == '\t') ||
                (c == '\v') ||
                (c == '\f') ||
                is_eol(c));
    return ret;
}

static inline char is_alpha(char c)
{
    char ret = (((c >= 'a') && (c <= 'z')) ||
                ((c >= 'A') && (c <= 'Z')));
	return ret;
}

static inline char is_number(char c)
{
    char ret = ((c >= '0') && 
                (c <= '9'));
    return ret;
}

static inline char token_equals(Token tok, char *match)
{
    char *at = match;
    for (int i = 0; i < tok.length; ++i, ++at)
    {
        if (!*at || (tok.text[i] != *at)) 
        { return 0; }
    }
    return (*at == 0);
}

#define _2PACWAV2_PARSER_DOT_H 1
#endif //_2PACWAV2_PARSER_DOT_H
