
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

typedef enum Token_Type {
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

typedef struct Token {
    Token_Type type;
    int length;
    char *text;
} Token;

typedef struct Tokenizer {
    char *at;
} Tokenizer;

PAC_INLINE char is_eol(char c);
PAC_INLINE char is_whitespace(char c);
PAC_INLINE char is_alpha(char c);
PAC_INLINE char is_number(char c);
PAC_INLINE char token_equals(Token tok, char *match);
PAC_INTERNAL void eat_whitespace(Tokenizer *tokenizer);
PAC_INLINE char require_token(Tokenizer *tokenizer, Token_Type req_tok);
PAC_INTERNAL Token get_token(Tokenizer *tokenizer);
PAC_INTERNAL Token get_string_entry(Tokenizer *tokenizer, char *dest, int dest_size, char *identifier);
PAC_INTERNAL float get_float_entry(Tokenizer *tokenizer, char *identifier);
PAC_INTERNAL void parse_and_apply_config(Runtime_Vars *rtvars, char *confbuf, int confbuf_bytes);

#define _2PACWAV2_PARSER_DOT_H 1
#endif //_2PACWAV2_PARSER_DOT_H
