
/*
File: 2pacwav2_confparser.cpp
Date: Fri 06 Jun 2025 12:45:47 PM EEST

A very simple (non-recursive) descent parser.
Bootlegged off of the introspecter that the bossman himself Casey Muratori wrote on
episode 206 of Handmade Hero (https://www.youtube.com/watch?v=1IwYEJsvdcs)
This means that comments are C & C++ style and lines end on semicolons.
*/

#include "2pacwav2.h"

#define CONF_STARTUP_PATH_TOKEN     "startup_path"
#define CONF_FONTSIZE_TOKEN         "font_size"

typedef enum Token_Type
{
    TOKEN_IDENTIFIER,
    TOKEN_OPEN_PARENTHESIS,
    TOKEN_CLOSED_PARENTHESIS,
    TOKEN_EQUALSIGN,
    TOKEN_COLON,
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

PAC_INLINE char is_eol(char c)
{
    char ret = ((c == '\n') ||
                (c == '\r'));
    return ret;
}

PAC_INLINE char is_whitespace(char c)
{
    char ret = ((c == ' ') ||
                (c == '\t') ||
                (c == '\v') ||
                (c == '\f') ||
                is_eol(c));
    return ret;
}

PAC_INLINE char is_alpha(char c)
{
    char ret = (((c >= 'a') && (c <= 'z')) ||
                ((c >= 'A') && (c <= 'Z')));
	return ret;
}

PAC_INLINE char is_number(char c)
{
    char ret = ((c >= '0') && 
                (c <= '9'));
    return ret;
}

PAC_INLINE char token_equals(Token tok, char *match)
{
    char *at = match;
    for (int i = 0; i < tok.length; ++i, ++at) {
        if (!*at || (tok.text[i] != *at)) 
        { return 0; }
    }
    return (*at == 0);
}

PAC_INTERNAL void report_missing_semicolon(char *identifier)
{
    fprintf(stderr, 
            "2wfile syntax error: semicolon (;) required after definition of variable \"%s\"\n",
            identifier);
}

PAC_INTERNAL void report_duplicate(char *identifier)
{
    fprintf(stderr, 
            "2wfile warning: variable \"%s\" set more than once. using definition in first instance\n",
            identifier);
}

PAC_INTERNAL void eat_whitespace(Tokenizer *tokenizer)
{
    while (1) {
        if (is_whitespace(tokenizer->at[0])) {
            ++tokenizer->at;
        } else if ((tokenizer->at[0] == '/') &&
            (tokenizer->at[1] == '/')) {
            //c++ style comment
            tokenizer->at += 2;
            while (tokenizer->at[0] && 
                !is_eol(tokenizer->at[0])) 
            { ++tokenizer->at; }
        } else if ((tokenizer->at[0] == '/') &&
            /*c style comment*/
            (tokenizer->at[1] == '*')) {
            tokenizer->at += 2;
            while (tokenizer->at[0] && 
                !((tokenizer->at[0] == '*') &&
                (tokenizer->at[1] == '/'))) 
            { ++tokenizer->at; }
            if (tokenizer->at[0] == '*') 
            { tokenizer->at += 2; }
        } else {
            break;
        }
    }
}

PAC_INLINE char require_token(Tokenizer *tokenizer,
                            Token_Type req_tok)
{
    Token tok = get_token(tokenizer);
    char ret = (tok.type == req_tok);
    return ret;
}

PAC_INTERNAL Token get_token(Tokenizer *tokenizer)
{
    eat_whitespace(tokenizer);
    Token tok = {};
    tok.length = 1;
    tok.text = tokenizer->at;
    char c = tokenizer->at[0];
    ++tokenizer->at;

    switch (c) {
    case '\0': { tok.type = TOKEN_STREAM_END; } break;
    case '(': { tok.type = TOKEN_OPEN_PARENTHESIS; } break;
    case ')': { tok.type = TOKEN_CLOSED_PARENTHESIS; } break;
    case ';': { tok.type = TOKEN_SEMICOLON; } break;
    case ':': { tok.type = TOKEN_COLON; } break;
    case '*': { tok.type = TOKEN_ASTERISK; } break;
    case '[': { tok.type = TOKEN_OPEN_BRACKET; } break;
    case ']': { tok.type = TOKEN_CLOSED_BRACKET; } break;
    case '{': { tok.type = TOKEN_OPEN_BRACE; } break;
    case '}': { tok.type = TOKEN_CLOSED_BRACE; } break;
    case '=': { tok.type = TOKEN_EQUALSIGN; } break;

    case '"': {
        tok.type = TOKEN_STRING;
        tok.text = tokenizer->at;
        while (tokenizer->at[0] &&
            tokenizer->at[0] != '"') {
            if ((tokenizer->at[0] == '\\') &&
                (tokenizer->at[1])) 
            { ++tokenizer->at; }
            ++tokenizer->at;
        }
        tok.length = tokenizer->at - tok.text;
        if (tokenizer->at[0] == '"') 
        { ++tokenizer->at; }
    } break;

    default: {
        if (is_alpha(c)) {
            tok.type = TOKEN_IDENTIFIER;
            while (is_alpha(tokenizer->at[0]) ||
                (is_number(tokenizer->at[0]) ||
                tokenizer->at[0] == '_')) 
            { ++tokenizer->at; }
            tok.length = tokenizer->at - tok.text;
        } else if (is_number(c) ||
            ((c == '-') && (is_number(tokenizer->at[1])))) { 
            tok.type = TOKEN_NUMBER;
            tok.text = tokenizer->at - 1;
            while (is_number(tokenizer->at[0]) ||
                (tokenizer->at[0] == '.'))
            { ++tokenizer->at; }
            tok.length = tokenizer->at - tok.text;
        } else {
            tok.type = TOKEN_UNKNOWN;
        }
    } break;
    }

    return tok;
}

PAC_INTERNAL Token get_string_entry(Tokenizer *tokenizer,
                                char *dest,
                                int dest_size,
                                char *identifier)
{
    Token tok = {};
    if (require_token(tokenizer, TOKEN_EQUALSIGN)) {
        tok = get_token(tokenizer);
        if (tok.type == TOKEN_STRING) {
            if ((tok.length + 1) < (dest_size - 1)) {
                if (require_token(tokenizer, TOKEN_SEMICOLON)) {
                    snprintf(dest, tok.length + 1, "%s", tok.text);
                } else {
                    report_missing_semicolon(identifier);
                }
            } else {
                fprintf(stderr, 
                        "2wfile syntax error: invalid path in definition of variable \"%s\"\n",
                        identifier);
            }
        } else {
            fprintf(stderr, 
                    "2wfile syntax error: identifier \"%s\" must be followed by a string\n",
                    identifier);
        }
    } else {
        fprintf(stderr, "2wfile syntax error: missing = after identifier\n");
    }
    return tok;
}

PAC_INTERNAL float get_float_entry(Tokenizer *tokenizer, char *identifier)
{
    Token tok = {};
    float ret = 0.0f;
    if (require_token(tokenizer, TOKEN_EQUALSIGN)) {
        tok = get_token(tokenizer);
        if (tok.type == TOKEN_NUMBER) {
            char *endptr = tok.text + tok.length;
            errno = 0;
            ret = strtof(tok.text, &endptr);
            if ((0.0f == ret) && (errno == ERANGE)) {
                fprintf(stderr, "2wfile error: value used in definition of identifier \"%s\" is invalid\n",
                        identifier);
            } if (!require_token(tokenizer, TOKEN_SEMICOLON)) {
                ret = 0.0f;
                report_missing_semicolon(identifier);
            }
        } else {
            fprintf(stderr, "2wfile syntax error: expected number after identifier \"%s\"",
                    identifier);
        }
    }
    return ret;
}

PAC_INTERNAL void parse_and_apply_config(Runtime_Vars *rtvars, 
                                        char *confbuf, 
                                        int confbuf_bytes)
{
    char parsing = 1;
    Startup_Args *sargs = rtvars->sargs_ptr;
    Tokenizer tokenizer = {0};
    tokenizer.at = confbuf;
    char stringbuf[PATH_MAX];
    char startup_path_set = 0,
            fontsize_set = 0;

    while (parsing) {
        Token tok = get_token(&tokenizer);
        switch (tok.type) {
        case TOKEN_STREAM_END: { parsing = 0; } break;

        case TOKEN_UNKNOWN: {
            fprintf(stderr, 
                    "2wfile warning: unknown token encountered: %s\n",
                    tok.text);
        } break;

        //check and handle tokens here
        case TOKEN_IDENTIFIER: {
            if (token_equals(tok, CONF_STARTUP_PATH_TOKEN)) {
                if (!startup_path_set) {
                    stringbuf[0] = 0;
                    Token ret_tok = get_string_entry(&tokenizer,
                                        stringbuf,
                                        sizeof(stringbuf),
                                        CONF_STARTUP_PATH_TOKEN);
                    if (stringbuf[0] && ret_tok.length) {
                        platform_dbg_log("loading startup path %s\n", stringbuf);
                        add_to_music_list(stringbuf, rtvars->mdata_ptr, rtvars);
                        ++startup_path_set;
                    }
                } else if (1 == startup_path_set) {
                    report_duplicate(CONF_STARTUP_PATH_TOKEN);
                    ++startup_path_set;
                }
            } else if (token_equals(tok, CONF_FONTSIZE_TOKEN)) {
                if (!fontsize_set) {
                    float value = get_float_entry(&tokenizer, CONF_FONTSIZE_TOKEN);
                    if (value > 0.0f) {
                        sargs->font_size = value;
                        ++fontsize_set;
                    }
                } else if (1 == fontsize_set) {
                    report_duplicate(CONF_FONTSIZE_TOKEN);
                    ++fontsize_set;
                }
            } else {
                snprintf(stringbuf, tok.length + 1, "%s", tok.text);
                fprintf(stderr, "2wfile warning: unknown identifier: \"%s\". ignoring\n",
                        stringbuf);
            }
        } break;

        default: {
        } break;
        }
    }
}
