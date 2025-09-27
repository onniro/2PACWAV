
/*
File: 2pacmixer.h
Date: Thu 22 May 2025 07:36:19 PM EEST

2pacmixer is a wannabe single-header library that is designed to be a replacement
for SDL mixer in the 2pacwav music player. The motivation for replacing SDL mixer
was the desire to be able to support more codecs and containers by utilizing
the FFmpeg libraries such as avcodec and avformat.
This code has (mostly) been developed in isolation of the rest of 2pacwav and is being
used in the present format in order to ease potential reuse of this code in the future.

required link options:
-lSDL2 -lavformat -lavcodec -lavutil -lswresample -lpthread
*/

#ifndef _2PACMIXER_DOT_H

#ifdef __cplusplus
extern "C" {
#endif

/*
If there are include errors, a better idea than defining PACMXR_DONT_INCLUDE_3RD_PARTY
to 1 and including this stuff elsewhere is to add the containing folders of these headers 
if they aren't found automatically with the -I option in your compiler.
*/
#if !PACMXR_DONT_INCLUDE_3RD_PARTY
#include <SDL2/SDL.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libavutil/dict.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>
#include <limits.h>
#include <errno.h>

#if defined(unix) || defined(__unix) || defined(__unix__)
    #define PACMXR_UNIX 1
#elif defined(_WIN32)
    #define PACMXR_WIN32 1 //unused
#endif

#if PACMXR_UNIX
#include <sys/mman.h>
#include <sys/select.h>
#endif

/* 
MACROS 
*/

#if defined(PACMXR_IMPLEMENTATION) && PACMXR_IMPLEMENTATION
    #define PACMXR_HEADER_ONLY 1
    #ifndef PACMXR_DEF
        #define PACMXR_DEF static
    #endif
    #ifndef PACMXR_INLINE
        #define PACMXR_INLINE static inline
    #endif
#else
    #define PACMXR_HEADER_ONLY 0
    #ifndef PACMXR_INLINE
        #define PACMXR_DEF extern
    #endif
    #ifndef PACMXR_INLINE
        #define PACMXR_INLINE extern inline
    #endif
#endif

#ifndef PACMXR_ZERO_INIT
    #if defined(__cplusplus) && (__cplusplus >= 201103L)
        #define PACMXR_ZERO_INIT
    #else
        #define PACMXR_ZERO_INIT 0
    #endif
#endif

#define PACMXR_GLOBALVAR static

#define PACMXR_SDL_AUDIO_BUFFER_SIZE            (2048)
#define PACMXR_OUTPUT_AUDIO_SAMPLE_FMT          AV_SAMPLE_FMT_S16
//IMPORTANT: this (below) is the size of ONE buffer and there are 2
#define PACMXR_AUDIOQUEUE_BUFFER_SIZE           (50*(1024*1024))
#define PACMXR_BYTES_PER_SAMPLE                 (sizeof(int16_t))
#define PACMCX_CHAN_COUNT                       (2)
#define PACMXR_RESAMPLE_SAMPLERATE              (48000)
//the word padding might be a little inaccurate for what this is actually for
//(see big comment in pacmxr__ffmpeg_resample_and_queue 4 more detailz)
#define PACMXR_BUF_END_PADDING                  (4096)
#define PACMXR_AUDIOQUEUE_MAX_SECS              ((PACMXR_AUDIOQUEUE_BUFFER_SIZE - \
                                                    PACMXR_BUF_END_PADDING) / \
                                                    (PACMXR_RESAMPLE_SAMPLERATE * \
                                                    PACMXR_CHAN_COUNT * \
                                                    PACMXR_BYTES_PER_SAMPLE))
#define PACMXR_DECODE_BUFFER_SIZE               (8*1024)
#define PACMXR_MIN_DECODE_BUFFER_SIZE           (8*1024)
#define PACMXR_CHAN_COUNT                       (2)

#define PACMXR_INITFLAGS_DEFAULT                (0)
#define PACMXR_INITFLAGS_NO_SDL                 (1)

#define PACMXR_NOP_MACRO(...)
#define PACMXR_ALIGN_UP(val, multiple) ((((val) + ((multiple) - 1))/(multiple))*(multiple))

/*
STRUCTS AND TYPEDEFS
*/

typedef int16_t Pcm16_Sample;
typedef int32_t Stereo16_Frame;

typedef struct Audio_Queue {
    uint8_t *front_buffer; //will point to buffer1 or buffer2 depending on which one is currently being read from
    uint8_t *buffer1;
    uint8_t *buffer2;
    uint8_t *write_ptr; //the pointer to which the decoder is writing new data
    uint8_t *read_ptr; //the pointer from which the sdl callback is reading from
    uint8_t *sdl_stream;
    size_t sdl_stream_len;
    size_t bytes_left; //how many bytes are left in the *front* buffer
    size_t frontbuffer_bytes; //how many bytes total are in the front buffer altogether
    size_t backbuffer_bytes;
    size_t bytes_allocated; //how much memory each buffer has (equal to PACMXR_AUDIOQUEUE_BUFFER_SIZE)
    size_t total_decode_bytes; //how many bytes you will end up with if you resample the entire stream
    int num_chunks; //how many times bigger the whole decoded size is than how much data can fit into the queue (rounded up)
    int chunk_index; //which chunk is currently in front_buffer (starts from 0)
    int volume; //only member in this struct that you can write to worry-free as long as the value is between 0 and 128
    //int64_t last_timestamp; //unused. last reading of AVFrame.best_effort_timestamp before the queue was filled up
    char buf_was_swapped; //gets set when front buffer runs out to signal us to prepare the next chunk
} Audio_Queue;

typedef struct File_Context {
    AVCodecContext *avc_ctx;
    AVFormatContext *avf_ctx;
    AVStream *astream;
    struct SwrContext *swr_ctx;
    AVChannelLayout out_chan_layout;
    AVCodec *codec;
    AVPacket *packet;
    AVFrame *frame;
    int stream_index;
    char is_open;
} File_Context;

typedef struct Pacmxr_Context {
    char paused;
    File_Context fctx;
    int decode_buffer_bytes;
    Audio_Queue aqueue;
    int out_buffer_size;
    SDL_AudioDeviceID au_dev;
    pthread_t decode_thread_handle;
    char thread_keep_running;
    uint8_t decode_buffer[PACMXR_DECODE_BUFFER_SIZE];
} Pacmxr_Context;

typedef struct Pacmxr_Init_Options {
    int sample_rate; //ignored as of june 4 2025
    /*
    Set below (no_init_sdl) to nonzero if pacmxr_init should not call out to SDL in any way.
    This is probably necessary if this header is being used in an application that already
    uses SDL and there is other code that does the initializing and there is a need for setting
    up SDL differently than this header normally would.
    NOTE: If this is set that means that there has to be other code that opens an audio device and sets
    global_pacmxr_ctx.au_dev to the resulting device id. 
    See definition of pacmxr__init_sdl to see a kind of reference implementation for this
    (pacmxr_default_init_options sets this to 0)
    */
    char no_init_sdl;
    uint32_t av_loglevel; //ffmpeg is pretty verbose normally, so by default this makes it only report errors 
} Pacmxr_Init_Options;

/*
FORWARD DECLARATIONS
*/

/*
All function names are prepended with pacmxr_, as C does not have namespaces.
Some functions have an extra _ (e.g. pacmxr__swap_buffers). Such functions are not
designed to be called from outside this file, so it should only be done if the caller
really knows what he's doing.
Some functions lack documentation because I think they are self-explanatory and
some other functions lack it because it hasn't been added yet or it actually
wasn't as self-explanatory as I might have initially thought.
*/

/*
pacmxr_init:
Should be called once before calling any other function in this header
except for pacmxr_default_init_options and the metadata functions.
This function will set up an audio device with SDL and set the callback to pacmxr__sdl_audio_callback. 
It will also allocate an amount of memory equal to PACMXR_AUDIOQUEUE_BUFFER_SIZE*2
using mmap on Unix systems (and nothing on Windows since it isn't supported).
Lastly it will also start a thread whose entry point is pacmxr__decode_check_thread_entry.
ARG 1 - opts:
Pointer to a Pacmxr_Init_Options struct with the desired options filled out.
See definition of Pacmxr_Init_Options or just call pacmxr_default_init_options
if you don't care.
RETURN VALUE:
Zero on failure, nonzero on success.

pacmxr_default_init_options:
Returns a Pacmxr_Init_Options struct that has reasonable options filled out.
See definition of this function to see what the default options are.

pacmxr_deinit:
Should be called when you no longer need 2pacmixer.
Frees memory, closes the SDL audio device and cleans up the FFmpeg related contexts and whatnot.
*/
PACMXR_DEF int pacmxr_init(Pacmxr_Init_Options *opts);
PACMXR_INLINE Pacmxr_Init_Options pacmxr_default_init_options(void);
PACMXR_DEF void pacmxr_deinit(void);

/*
pacmxr_open_file:
Call this to open an audio file with 2pacmixer.
Opens a file with avformat_open_input and begins decoding and resampling it immediately.
ARG 1 - filename:
Pointer to a null-terminated string that contains the path to the file to be opened.
RETURN VALUE:
Zero on failure, nonzero on success.

pacmxr_close_file:
Call this to close a file that was previously opened with pacmxr_open_file.
This is important before opening another file.
*/
PACMXR_DEF int pacmxr_open_file(char *filename);
PACMXR_DEF void pacmxr_close_file(void);

/*
pacmxr_get_context:
Get a pointer to the global Pacmxr_Context variable called global_pacmxr_ctx.
Equal to &global_pacmxr_ctx. Just exists for consistency

pacmxr_get_audioq:
Get a pointer to the Audio_Queue structure within the global Pacmxr_Context.
Equal to &global_pacmxr_ctx.aqueue.
*/
PACMXR_INLINE Pacmxr_Context *pacmxr_get_context(void);
PACMXR_INLINE Audio_Queue *pacmxr_get_audioq(void);

/*
pacmxr_pause & pacmxr_toggle_playback:
Hopefully self-explanatory
(pause argument in pacmxr_pause just means 1 = pause, 0 = unpause)
*/
PACMXR_INLINE void pacmxr_pause(char pause);
PACMXR_INLINE void pacmxr_toggle_playback(void);

/*
Some random utility functions
*/
PACMXR_INLINE int pacmxr_align_up(size_t val, size_t multiple);
PACMXR_INLINE int pacmxr_clamp_int(int val, int min, int max);
PACMXR_INLINE float pacmxr_clamp_float(float val, float min, float max);
PACMXR_INLINE float pacmxr_ceilf(float val);
PACMXR_INLINE float pacmxr_floorf(float val);

/*
pacmxr_set_volume:
Set output volume for the SDL audio device.
Equivalent to writing to global_pacmxr_ctx.aqueue.volume
ARG 1 - volume:
The volume to set. Clamped to 0-128 (SDL_MIX_MAXVOLUME)

pacmxr_get_volume:
Get current output volume of the SDL audio device.
Equivalent to reading from global_pacmxr_ctx.aqueue.volume.

pacmxr_file_is_open:
Returns nonzero if an audio file is currently open and zero otherwise.
Equal to reading from global_pacmxr_ctx.fctx.is_open
*/
PACMXR_INLINE void pacmxr_set_volume(int volume);
PACMXR_INLINE int pacmxr_get_volume(void);
PACMXR_INLINE char pacmxr_file_is_open(void);
PACMXR_INLINE char pacmxr_is_paused(void);

/*
pacmxr_seek:
Seek/jump to a desired percentage in the currently opened stream.
ARG 1 - percent:
A value between 0.0f and 1.0f that specifies how far into the stream
2pacmixer should seek to, where 0.0f is the beginning of the stream and
1.0f is the end. The value gets clamped in case it doesn't fall within
the desired range.
*/
PACMXR_DEF void pacmxr_seek(float percent);

/*
pacmxr_stream_duration:
Returns the length of the currently opened audio stream in seconds.

pacmxr_seconds_played:
Returns the amount of seconds from the beginning of the stream to the currently playing frame.

pacmxr_entire_stream_seconds_left:
Returns the amount of seconds from the end of the stream to the currently playing frame.
Equal to pacmxr_stream_duration() - pacmxr_seconds_played()

pacmxr_estimate_decode_size:
Returns the number of bytes you will end up with if you resample the entire stream
(that is currently opened) with the current audio output and resampling properties.

pacmxr_seconds_to_bytes:
Returns the number of bytes that are to be present in an audio stream with a length
specified by the seconds argument, given that the stream contains data that was resampled
with the present properties.
ARG 1 - seconds:
The number of seconds you want to convert to bytes.

pacmxr_seconds_to_seek_value:
Converts a number of seconds to the kind of value expected by pacmxr_seek.
Return value is to equal seconds/pacmxr_stream_duration() unless seconds is negative 
or greater than pacmxr_stream_duration(), in which case the return value will be
clamped accordingly.
*/
PACMXR_INLINE float pacmxr_stream_duration(void);
PACMXR_INLINE float pacmxr_seconds_played(void);
PACMXR_INLINE float pacmxr_entire_stream_seconds_left(void);
PACMXR_INLINE size_t pacmxr_estimate_decode_size(void);
PACMXR_INLINE size_t pacmxr_seconds_to_bytes(float seconds);
PACMXR_INLINE float pacmxr_seconds_to_seek_value(float seconds);

/*
This set of functions is unlikely to be very useful for anything except 
other functions in the header so they aren't documented.
They are also very simple and small so its probably obvious
*/
PACMXR_INLINE float pacmxr_queue_seconds_left(void);
PACMXR_INLINE int pacmxr_chunk_sec_offset(int index);

/*
As was said above, these functions with the extra underscores aren't designed to be called
by the code that uses this header, so they aren't documented.
*/
PACMXR_DEF int pacmxr__init_sdl(Pacmxr_Init_Options *opts);
PACMXR_DEF void *pacmxr__decode_check_thread_entry(void *udata);
PACMXR_DEF void pacmxr__queue_decoded_chunk(Audio_Queue *aq, uint8_t *srcbuf, int num_bytes);
PACMXR_DEF int pacmxr__ffmpeg_resample_and_queue(int64_t second_offset);
PACMXR_DEF void pacmxr__decode_more(void);
PACMXR_DEF void pacmxr__sdl_audio_callback(void *userdata, uint8_t *stream, int len);
PACMXR_DEF void pacmxr__reset_queue(Audio_Queue *aq, char zero);
PACMXR_INLINE void pacmxr__swap_buffers(void);

/*
METADATA
*/

typedef struct Pacmxr_Metadata {
    uint32_t stream_index;
    AVFormatContext *in_avf_ctx;
    AVFormatContext *out_avf_ctx;
    AVDictionary *out_metadata;
} Pacmxr_Metadata;

/*
pacmxr_meta_open_file:
Open a file solely for the purpose of manipulating metadata tags.
For opening a file for playback, see description of pacmxr_open_file.
ARG 1 - pac_meta:
Pointer to a Pacmxr_Metadata struct that will get filled with data
so that you can pass this struct to other metadata related functions.
ARG 2 - filename:
Pointer to a null-terminated string with the name of the file you want to open

pacmxr_meta_close_file:
Deinitialize a previously initialized Pacmxr_Metadata struct.
ARG 1 - pac_meta:
Pointer to a Pacmxr_Metadata struct that was previously passed to pacmxr_meta_open_file
*/
PACMXR_DEF int pacmxr_meta_open_file(Pacmxr_Metadata *pac_meta, char *filename);
PACMXR_DEF void pacmxr_meta_close_file(Pacmxr_Metadata *pac_meta);

/*
pacmxr_meta_title
pacmxr_meta_artist
pacmxr_meta_album

Get metadata information from a file.
These functions do the same thing, the only thing that differs is the data they query. 
Hence, the same documentation for the arguments applies for all of these functions.
ARG 1 - pac_meta:
Pointer to a Pacmxr_Metadata struct that was previously initialized
with pacmxr_meta_open_file. If this argument is null, this function
will output the corresponding data of the file that was last opened with pacmxr_open_file
(note that pacmxr_meta_open_file and pacmxr_open_file are totally different functions)
ARG 2 - dest:
Pointer to a buffer that will receive the data.
ARG 3 - dest_size:
The number of bytes that can safely be written to dest.
RETURN VALUE:
Zero if the queried value doesn't exist and nonzero otherwise
*/
PACMXR_DEF int pacmxr_meta_get_title(Pacmxr_Metadata *pac_meta, char *dest, int dest_size);
PACMXR_DEF int pacmxr_meta_get_artist(Pacmxr_Metadata *pac_meta, char *dest, int dest_size);
PACMXR_DEF int pacmxr_meta_get_album(Pacmxr_Metadata *pac_meta, char *dest, int dest_size);
PACMXR_DEF int pacmxr_meta_get_genre(Pacmxr_Metadata *pac_meta, char *dest, int dest_size);

/*
pacmxr_meta_get_track
pacmxr_meta_get_year

Get metadata information from a file
ARG 1 - pac_meta:
Pointer to a Pacmxr_Metadata struct that was previously initialized
with pacmxr_meta_open_file. If this argument is null, this function
will output the corresponding data of the file that was last opened with pacmxr_open_file
RETURN VALUE:
The integer value being queried if it exists in the file and -1 otherwise
*/
PACMXR_DEF int pacmxr_meta_get_track(Pacmxr_Metadata *pac_meta);
PACMXR_DEF int pacmxr_meta_get_year(Pacmxr_Metadata *pac_meta);

/*
pacmxr_meta_get_cover

Get the cover art image data from a file.
ARG 1 - pac_meta:
Pointer to a Pacmxr_Metadata struct that was previously initialized
with pacmxr_meta_open_file. If this argument is null, this function
will output the corresponding data of the file that was last opened with pacmxr_open_file
ARG 2 - out_size:
A pointer to an integer variable that will receive the number of bytes in the cover art.
RETURN VALUE:
If any cover art is present in the opened file, this will return a pointer to the beginning
of a buffer that contains the image data. Otherwise, the returned pointer will be null.
*/
PACMXR_DEF uint8_t *pacmxr_meta_get_cover(Pacmxr_Metadata *pac_meta, int *out_size);

/*
pacmxr_meta_set_title
pacmxr_meta_set_artist
pacmxr_meta_set_album
pacmxr_meta_set_genre

Set new metadata information for the currently open file.
These functions merely prepare the new metadata
and information is only saved upon calling pacmxr_meta_save.
The only difference between these functions is the data they modify
and the arguments have the same purpose.
ARG 1 - pac_meta:
Pointer to a previously initialized Pacmxr_Metadata struct. Cannot be null.
ARG 2 - value:
Pointer to a null-terminated string to which you want to set the metadata.
*/
PACMXR_DEF void pacmxr_meta_set_title(Pacmxr_Metadata *pac_meta, char *value);
PACMXR_DEF void pacmxr_meta_set_artist(Pacmxr_Metadata *pac_meta, char *value);
PACMXR_DEF void pacmxr_meta_set_album(Pacmxr_Metadata *pac_meta, char *value);
PACMXR_DEF void pacmxr_meta_set_genre(Pacmxr_Metadata *pac_meta, char *value);

/*
pacmxr_meta_set_year 
pacmxr_meta_set_track

ARG 1 - pac_meta:
Pointer to a previously initialized Pacmxr_Metadata struct. Cannot be null.
ARG 2 - value:
The integer value to which you want to set the new metadata.
*/
PACMXR_DEF void pacmxr_meta_set_year(Pacmxr_Metadata *pac_meta, int value);
PACMXR_DEF void pacmxr_meta_set_track(Pacmxr_Metadata *pac_meta, int value);

/*
pacmxr_meta_save:
Write the metadata information in its current state to the currently open file,
whether or not any data has been modified.
More technically speaking, this file is actually written twice because I don't really know
how (else) to do in-place metadata editing at least with libavformat.
The file is first written to /tmp/ and then copied via pacmxr_copy_file to where your actual
file is, overwriting the original file. Lastly, the temporary file is deleted.
ARG 1 - pac_meta:
Pointer to previously initialized Pacmxr_Metadata struct.
*/
PACMXR_DEF int pacmxr_meta_save(Pacmxr_Metadata *pac_meta);

/*
pacmxr_copy_file:
Utility function. Read a file from the path specified by src_path and then
write it to the path specified by dst_path.
RETURN VALUE:
Zero on failure, nonzero on success.
*/
PACMXR_DEF int pacmxr_copy_file(char *dst_path, char *src_path);

PACMXR_GLOBALVAR Pacmxr_Context global_pacmxr_ctx;

#if PACMXR_HEADER_ONLY

PACMXR_INLINE Pacmxr_Context *pacmxr_get_context(void)
{
    return &global_pacmxr_ctx;
}

PACMXR_INLINE Audio_Queue *pacmxr_get_audioq(void) 
{
    return &global_pacmxr_ctx.aqueue;
}

PACMXR_INLINE int pacmxr_align_up(size_t val, size_t multiple)
{
    int ret = ((val + multiple - 1)/val)*multiple;
    return ret;
}

PACMXR_INLINE int pacmxr_clamp_int(int val, int min, int max)
{
    int ret = val;
    if (val < min) {
        ret = min; 
    } else if (val > max) {
        ret = max;
    }
    return ret;
}

PACMXR_INLINE float pacmxr_clamp_float(float val, float min, float max)
{
    float ret = val;
    if (val < min) {
        ret = min; 
    } else if (val > max) {
        ret = max;
    }
    return ret;
}

PACMXR_INLINE float pacmxr_floorf(float val)
{
    return (float)((int)val);
}

PACMXR_INLINE float pacmxr_ceilf(float val)
{
    float ret = 0;
    if ((val - (int)val) > 0) {
        ret = (float)((int)val + 1); 
    } else {
        ret = (float)((int)val);
    }
    return ret;
}

PACMXR_INLINE void pacmxr_toggle_playback(void)
{
    Pacmxr_Context *ctx = &global_pacmxr_ctx;
    ctx->paused = !ctx->paused;
    SDL_PauseAudioDevice(ctx->au_dev, ctx->paused);
}

PACMXR_INLINE void pacmxr_set_volume(int volume) 
{
    Pacmxr_Context *ctx = &global_pacmxr_ctx;
    volume = pacmxr_clamp_int(volume, 0, SDL_MIX_MAXVOLUME);
    ctx->aqueue.volume = volume;
}

PACMXR_INLINE int pacmxr_get_volume(void) 
{
    return global_pacmxr_ctx.aqueue.volume;
}

PACMXR_INLINE char pacmxr_file_is_open(void)
{
    return global_pacmxr_ctx.fctx.is_open;
}

PACMXR_INLINE char pacmxr_is_paused(void)
{
    return global_pacmxr_ctx.paused;
}

//#define PACMXR_UNIX 1
#if defined(PACMXR_UNIX) && PACMXR_UNIX
PACMXR_INLINE void pacmxr_sleep_ms(int millisec) 
{
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = millisec*1000;
    select(0, 0, 0, 0, &tv);
}

PACMXR_INLINE void pacmxr_sleep_us(int microsec)
{
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = microsec;
    select(0, 0, 0, 0, &tv);
}
#endif

PACMXR_DEF void *pacmxr__decode_check_thread_entry(void *udata) 
{
#define PACMXR_THREAD_SLEEP_MS (33)
    Pacmxr_Context *ctx = (Pacmxr_Context *)udata;
    volatile Audio_Queue *aq = &ctx->aqueue;
    volatile char *keep_running = &ctx->thread_keep_running;
    volatile char *paused = &ctx->paused;

    while (*keep_running) {
        //if (!*paused && pacmxr_file_is_open())
        if (!*paused) {
#if 0
            if (!aq->bytes_left && aq->backbuffer_bytes) {
                pacmxr__swap_buffers();
                aq->bytes_left = aq->backbuffer_bytes;
                aq->frontbuffer_bytes = aq->backbuffer_bytes;
                aq->backbuffer_bytes = 0;
                aq->read_ptr = aq->front_buffer;
                ++aq->chunk_index;
            } 
#endif
            if (aq->buf_was_swapped) {
                aq->buf_was_swapped = 0;
                //if (aq->chunk_index != (aq->num_chunks - 1))
                { pacmxr__decode_more(); }
            }
        }
        pacmxr_sleep_ms(PACMXR_THREAD_SLEEP_MS);
    }

    return 0;
}

PACMXR_INLINE Pacmxr_Init_Options pacmxr_default_init_options(void)
{
    Pacmxr_Init_Options ret = {PACMXR_ZERO_INIT};
    ret.sample_rate = PACMXR_RESAMPLE_SAMPLERATE;
    ret.no_init_sdl = 0;
    ret.av_loglevel = AV_LOG_ERROR;
    return ret;
}

PACMXR_DEF int pacmxr__init_sdl(Pacmxr_Init_Options *opts)
{
    Pacmxr_Context *ctx = &global_pacmxr_ctx;
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "pacmxr_init could not initialize SDL: %s\n", SDL_GetError());
        return 0;
    }

    SDL_AudioSpec wanted_spec, obtained_spec;
    SDL_zero(wanted_spec);
    wanted_spec.freq = PACMXR_RESAMPLE_SAMPLERATE;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.channels = 2;
    wanted_spec.samples = PACMXR_SDL_AUDIO_BUFFER_SIZE;
    wanted_spec.callback = pacmxr__sdl_audio_callback;
    wanted_spec.userdata = (void *)ctx;

    ctx->au_dev = SDL_OpenAudioDevice(0, 0, &wanted_spec, &obtained_spec, 0);
    if (!ctx->au_dev) {
        fprintf(stderr, "pacmxr_init failed to open audio device: %s\n", SDL_GetError());
        return 0;
    }
    SDL_PauseAudioDevice(ctx->au_dev, 1);
    SDL_LockAudioDevice(ctx->au_dev);
    return 1;
}

PACMXR_DEF int pacmxr_init(Pacmxr_Init_Options *opts)
{
    Pacmxr_Context *ctx = &global_pacmxr_ctx;
    memset((void *)ctx, 0, sizeof(Pacmxr_Context));
    Audio_Queue *aq = &ctx->aqueue;

    int filedesc = open("/dev/zero", O_RDWR);
    if (filedesc < 0) {
        fprintf(stderr, "pacmxr_init failed to get memory for 2pacmixer.\n");
        perror("open");
        return 0;
    }
    aq->buffer1 = (uint8_t *)mmap(0, PACMXR_AUDIOQUEUE_BUFFER_SIZE*2,
                    PROT_READ|PROT_WRITE,
                    MAP_PRIVATE, filedesc, 0);
    if (ctx->aqueue.buffer1 == MAP_FAILED) {
        fprintf(stderr, "pacmxr_init failed to get memory for 2pacmixer.\n");
        perror("mmap");
        return 0;
    }

    aq->front_buffer = aq->buffer2;
    aq->bytes_allocated = PACMXR_AUDIOQUEUE_BUFFER_SIZE;
    aq->buffer2 = aq->buffer1 + aq->bytes_allocated;
    aq->backbuffer_bytes = 0;
    aq->write_ptr = aq->buffer1;
    aq->read_ptr = aq->buffer1;
    aq->volume = SDL_MIX_MAXVOLUME/2;
    aq->bytes_left = 0;
    ctx->fctx.is_open = 0;

    if (!opts->no_init_sdl)
    { pacmxr__init_sdl(opts); }

    av_log_set_level(opts->av_loglevel);

    ctx->thread_keep_running = 1;
    ctx->paused = 1;
    if (pthread_create(&ctx->decode_thread_handle, 0,
            pacmxr__decode_check_thread_entry,
            (void *)ctx)) {
        fprintf(stderr, "pacmxr_init failed to start decode thread\n");
        return 0;
    }
    pthread_detach(ctx->decode_thread_handle);
    return 1;
}

PACMXR_DEF void pacmxr_deinit(void)
{
    Pacmxr_Context *ctx = &global_pacmxr_ctx;
    ctx->thread_keep_running = 0;
    Audio_Queue *aq = &ctx->aqueue;
    //SDL_PauseAudioDevice(ctx->au_dev, 0);
    SDL_UnlockAudioDevice(ctx->au_dev);
    pacmxr_set_volume(0);
    pacmxr_pause(0);
    SDL_UnlockAudioDevice(ctx->au_dev);
    SDL_Delay(100);
    SDL_CloseAudioDevice(ctx->au_dev);
    SDL_Quit();
    pacmxr_close_file();
    //it seems to be that unlocking, unpausing and unlocking again
    //is a fool proof method to un deadlock the sdl audio shit every time
    //100p success rate tricknology
#if 1
    munmap(aq->buffer1, aq->bytes_allocated);
#else
    free(aq->buffer);
#endif
}

PACMXR_DEF void pacmxr_pause(char pause)
{
    Pacmxr_Context *ctx = &global_pacmxr_ctx;
    ctx->paused = pause;
    SDL_PauseAudioDevice(ctx->au_dev, pause);
}

PACMXR_INLINE float pacmxr_stream_duration(void)
{
    Pacmxr_Context *ctx = &global_pacmxr_ctx;
    File_Context *fctx = &ctx->fctx;
    float ret = -1.0f;
    if (fctx && fctx->avf_ctx) 
    { ret = (float)fctx->avf_ctx->duration/(float)AV_TIME_BASE; }
    return ret;
}

PACMXR_INLINE size_t pacmxr_unqueued_bytes(void)
{
    Pacmxr_Context *ctx = &global_pacmxr_ctx;
    Audio_Queue *aq = &ctx->aqueue;
    //size_t unqueued_bytes = aq->total_decode_bytes - aq->backbuffer_bytes;
    size_t unqueued_bytes = aq->total_decode_bytes - aq->bytes_left;
    return unqueued_bytes;
}

PACMXR_INLINE float pacmxr_seconds_played(void)
{
    Pacmxr_Context *ctx = &global_pacmxr_ctx;
    Audio_Queue *aq = &ctx->aqueue;
    float prev_chunk_secs = PACMXR_AUDIOQUEUE_MAX_SECS*aq->chunk_index;
    float this_chunk_left = pacmxr_queue_seconds_left();
    float this_chunk_secs;
    if (aq->chunk_index != (aq->num_chunks - 1)) {
        this_chunk_secs = PACMXR_AUDIOQUEUE_MAX_SECS - this_chunk_left;
    } else {
        this_chunk_secs = (pacmxr_stream_duration() -
                        prev_chunk_secs) -
                        this_chunk_left;
    }
    float secs_played = prev_chunk_secs + this_chunk_secs;

    return secs_played;
}

PACMXR_INLINE float pacmxr_entire_stream_seconds_left(void)
{
    float seconds_left = pacmxr_stream_duration() -
                        pacmxr_seconds_played();
    return seconds_left;
}

//this will be wrong for the last chunk but its kinad useless anyway
PACMXR_INLINE float pacmxr_queue_seconds_total(void)
{
    return PACMXR_AUDIOQUEUE_MAX_SECS;
}

PACMXR_INLINE float pacmxr_queue_seconds_left(void)
{
    Pacmxr_Context *ctx = &global_pacmxr_ctx;
    float bytes_left = (float)ctx->aqueue.bytes_left;
    //int num_chans = ctx->fctx.avc_ctx->ch_layout.nb_channels;
    float num_chans = PACMXR_CHAN_COUNT;
    float sample_rate = PACMXR_RESAMPLE_SAMPLERATE;
    float bytes_per_sample = sizeof(Pcm16_Sample);
    float seconds_left = bytes_left/(sample_rate*num_chans*bytes_per_sample);
    return seconds_left;
}

PACMXR_DEF void pacmxr__queue_decoded_chunk(Audio_Queue *aq,
                                        uint8_t *srcbuf,
                                        int num_bytes)
{
    memcpy(aq->write_ptr, srcbuf, num_bytes);
    aq->write_ptr += num_bytes;
    //aq->bytes_left += num_bytes;
    aq->backbuffer_bytes += num_bytes;
}

PACMXR_DEF int pacmxr__ffmpeg_resample_and_queue(int64_t second_offset)
{
    Pacmxr_Context *ctx = &global_pacmxr_ctx;
    uint8_t *decode_buffer = ctx->decode_buffer;
    AVCodecContext *avc_ctx = ctx->fctx.avc_ctx;
    AVFormatContext *avf_ctx = ctx->fctx.avf_ctx;
    AVPacket *packet = ctx->fctx.packet;
    //AVCodec *codec = ctx->fctx.codec;
    AVFrame *frame = ctx->fctx.frame;
    struct SwrContext *swr_ctx = ctx->fctx.swr_ctx;
    //SDL_AudioDeviceID au_dev = ctx->au_dev;
    Audio_Queue *aq = &ctx->aqueue;
    int dst_nb_samples, buffer_size, samples_converted;
    size_t decode_size;
    File_Context *fctx = &ctx->fctx;

    if (second_offset >= 0) {
        double tbase_f64 = av_q2d(avf_ctx->streams[fctx->stream_index]->time_base);
        int64_t tstamp = (int64_t)(second_offset/tbase_f64);
#if PACMXR_DEBUG
        fprintf(stderr, "[pacmxr__ffmpeg_resample_and_queue]:\n"
                        "seeking to %ld (timebase: %g)\n",
                        tstamp, tbase_f64);
#endif
        if (av_seek_frame(avf_ctx, 
            ctx->fctx.stream_index, 
            tstamp, AVSEEK_FLAG_FRAME) < 0) {
            fprintf(stderr, "seeking to specified frame failed\n");
        }
        avcodec_flush_buffers(ctx->fctx.avc_ctx);
    }

    /*
    this padding shit is so that we defer processing the next packet until the next
    time we fill the back buffer if 100% of the frames in the packet 
    won't fit completely. otherwise all the frames that wont fit get skipped
    and the decoder just starts from the following packet the next time causing
    a (sometimes) noticable skip in the playback cuz of all the shit that got skipped.
    TODO: PACMXR_BUF_END_PADDING is set to a value i just thought seemed safe, 
    figure out what this actually should be for higher efficiency/correctness
    */

    int pack_count = 0, frame_count = 0;
    while (1) {
        if ((aq->backbuffer_bytes + PACMXR_BUF_END_PADDING) > aq->bytes_allocated) {
            av_packet_unref(packet);
            break;
        } if (av_read_frame(avf_ctx, packet) < 0) {
            av_packet_unref(packet);
            break;
        }

        if (packet->stream_index == ctx->fctx.stream_index) {
            if (avcodec_send_packet(avc_ctx, packet) < 0) {
                fprintf(stderr, "error sending packet\n");
                return -2;
            }

            while (avcodec_receive_frame(avc_ctx, frame) >= 0) {
                dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx, avc_ctx->sample_rate) +
                                        frame->nb_samples,
                                        avc_ctx->sample_rate,
                                        avc_ctx->sample_rate,
                                        AV_ROUND_UP);

                buffer_size = av_samples_get_buffer_size(0,
                                    avc_ctx->ch_layout.nb_channels,
                                    dst_nb_samples,
                                    PACMXR_OUTPUT_AUDIO_SAMPLE_FMT, 1);
                (void)buffer_size; //fixes warnings

                samples_converted = swr_convert(swr_ctx,
                                        &decode_buffer,
                                        dst_nb_samples,
                                        (const uint8_t **)frame->data, 
                                        frame->nb_samples);
                ctx->out_buffer_size = av_samples_get_buffer_size(0,
                                            avc_ctx->ch_layout.nb_channels, 
                                            samples_converted,
                                            PACMXR_OUTPUT_AUDIO_SAMPLE_FMT, 1);
                decode_size = aq->backbuffer_bytes + ctx->out_buffer_size;

                if (decode_size < aq->bytes_allocated) {
                    pacmxr__queue_decoded_chunk(&ctx->aqueue,
                            decode_buffer,
                            ctx->out_buffer_size);
                } else {
                    //aq->last_timestamp = frame->pts;
                    break;
                }
                ++frame_count;
            }
        }
        av_packet_unref(packet);
        ++pack_count;
    }

#if PACMXR_DEBUG
    fprintf(stderr, "[pacmxr__ffmpeg_resample_and_queue]\n"
                    "packets: %d, frames: %d\n", pack_count, frame_count);
#endif

    return 0;
}

#if 0
#define PACMXR_NEED_MORE_DATA_SEC_THRESH 5
//this stuff isn't used anymore
PACMXR_INLINE size_t pacmxr__need_more_data(void)
{
    Pacmxr_Context *ctx = &global_pacmxr_ctx;
    size_t ret = 0;
    Audio_Queue *aq = &ctx->aqueue;
    int time_left = pacmxr_queue_seconds_left();
    //printf("time_left:%d, bytes_left:%d\n", time_left, aq->bytes_left);
    //this could just be a boolean but i cant be fucked changing the decls
    if ((aq->total_decode_bytes > aq->bytes_allocated) &&
         (time_left < PACMXR_NEED_MORE_DATA_SEC_THRESH) &&
         (!aq->backbuffer_bytes))
    { ret = aq->bytes_allocated; }
    return ret;
}
#endif

PACMXR_INLINE size_t pacmxr_seconds_to_bytes(float seconds)
{
    if (seconds < 0.0f)
    { return 0; }
    size_t ret = PACMXR_RESAMPLE_SAMPLERATE *
                PACMXR_CHAN_COUNT *
                PACMXR_BYTES_PER_SAMPLE *
                seconds;
    return ret;
}

PACMXR_INLINE float pacmxr_seconds_to_seek_value(float seconds)
{
    Pacmxr_Context *ctx = &global_pacmxr_ctx;
    float ret = 0.0f;
    float stream_len = pacmxr_stream_duration();
    if (stream_len != -1.0f) {
        if (seconds > stream_len)
        { ret = 1.0f; }
        else if (seconds > 0.0f)
        { ret = seconds/stream_len; }
    }
    return ret;
}

PACMXR_DEF void pacmxr_seek(float percent)
{
    if (!pacmxr_file_is_open())
    { return; }
    Pacmxr_Context *ctx = &global_pacmxr_ctx;
    percent = pacmxr_clamp_float(percent, 0.0f, 1.0f);

    Audio_Queue *aq = &ctx->aqueue;
    //SDL_PauseAudioDevice(ctx->au_dev, 1);
    SDL_LockAudioDevice(ctx->au_dev);

    int whole_duration = pacmxr_stream_duration();
    float target_time = whole_duration*percent;
    int target_chunk = pacmxr_floorf(target_time/PACMXR_AUDIOQUEUE_MAX_SECS);
    //printf("target time: %g (falls in chunk %d)\n", 
    //        target_time, target_chunk);
    //TODO: clean this shit up a little

    if (target_chunk != aq->chunk_index) {
        int sec_offset = pacmxr_chunk_sec_offset(target_chunk);
        //ctx->paused = 1;
        pacmxr__reset_queue(aq, 1);

        if (pacmxr__ffmpeg_resample_and_queue(sec_offset)) {
            fprintf(stderr, "2pacmixer failed to seek outside of chunk\n");
        }

        aq->chunk_index = target_chunk;
        pacmxr__swap_buffers();
        aq->bytes_left = aq->backbuffer_bytes;
        aq->frontbuffer_bytes = aq->backbuffer_bytes;
        aq->backbuffer_bytes = 0;
        aq->read_ptr = aq->front_buffer;

        float chunk_start = pacmxr_chunk_sec_offset(target_chunk);
        float secs2skip = target_time - chunk_start;
        //printf("seeking by %g seconds from %g (%g)\n",
        //        secs2skip, chunk_start, chunk_start + secs2skip);

        assert(target_time >= chunk_start);

        int frames2skip = pacmxr_seconds_to_bytes(secs2skip)/sizeof(Stereo16_Frame);
        Stereo16_Frame *buffer = (Stereo16_Frame *)aq->front_buffer; 
        Stereo16_Frame *target_frame = &buffer[frames2skip];
        uintptr_t buf_end = (uintptr_t)aq->front_buffer + aq->frontbuffer_bytes;
        size_t new_bytes_left = buf_end - (uintptr_t)target_frame;
        aq->read_ptr = (uint8_t *)target_frame;
        aq->bytes_left = new_bytes_left;

        //aq->buf_was_swapped = 1;
        //ctx->paused = 0;
    } else {
        float chunk_start = pacmxr_chunk_sec_offset(aq->chunk_index);
        float secs2skip = target_time - chunk_start;
        int frames2skip = pacmxr_seconds_to_bytes(secs2skip)/sizeof(Stereo16_Frame);
        Stereo16_Frame *buffer = (Stereo16_Frame *)aq->front_buffer; 

        assert((size_t)frames2skip < (aq->frontbuffer_bytes/sizeof(Stereo16_Frame)));

        Stereo16_Frame *target_frame = &buffer[frames2skip];
        uintptr_t buf_end = (uintptr_t)aq->front_buffer + aq->frontbuffer_bytes;
        size_t new_bytes_left = buf_end - (uintptr_t)target_frame;

        aq->read_ptr = (uint8_t *)target_frame;
        aq->bytes_left = new_bytes_left;
    }

    //SDL_PauseAudioDevice(ctx->au_dev, 0);
    SDL_UnlockAudioDevice(ctx->au_dev);
}

PACMXR_INLINE int pacmxr_chunk_sec_offset(int index)
{
    //Pacmxr_Context *ctx = &global_pacmxr_ctx;
    int64_t ret = index*PACMXR_AUDIOQUEUE_MAX_SECS;
    return ret;
}

PACMXR_DEF void pacmxr__decode_more(void)
{
    Pacmxr_Context *ctx = &global_pacmxr_ctx;
    Audio_Queue *aq = &ctx->aqueue;

    if (aq->front_buffer == aq->buffer1) {
        aq->write_ptr = aq->buffer2;
    } else {
        aq->write_ptr = aq->buffer1;
    }
    ctx->aqueue.backbuffer_bytes = 0;

    if (pacmxr__ffmpeg_resample_and_queue(-1)) {
        fprintf(stderr, "failed to decode block\n");
        //do other shit
    }
}

PACMXR_INLINE void pacmxr__swap_buffers(void)
{
    Pacmxr_Context *ctx = &global_pacmxr_ctx;
    Audio_Queue *aq = &ctx->aqueue;
    if (aq->front_buffer == aq->buffer1) {
        aq->front_buffer = aq->buffer2;
    } else {
        aq->front_buffer = aq->buffer1;
    }
    aq->buf_was_swapped = 1;
}

PACMXR_DEF void pacmxr__sdl_audio_callback(void *userdata, uint8_t *stream, int len)
{
    Pacmxr_Context *ctx = (Pacmxr_Context *)userdata;
    Audio_Queue *aq = &ctx->aqueue;
    aq->sdl_stream = stream;
    aq->sdl_stream_len = len;
    //if (!pacmxr_file_is_open())
    //{ return; }
    if (!aq->bytes_left) {
        if (!aq->backbuffer_bytes) 
        { return; }
#if 1
        pacmxr__swap_buffers();
        aq->bytes_left = aq->backbuffer_bytes;
        aq->frontbuffer_bytes = aq->backbuffer_bytes;
        aq->backbuffer_bytes = 0;
        aq->read_ptr = aq->front_buffer;
        ++aq->chunk_index;
#endif
    }

    if ((size_t)len > aq->bytes_left) 
    { len = aq->bytes_left; }
    SDL_memset(stream, 0, len);
    SDL_MixAudioFormat(stream, 
            aq->read_ptr, 
            AUDIO_S16SYS,
            len, 
            aq->volume);
    aq->read_ptr += len;
    aq->bytes_left -= len;
}

PACMXR_DEF int pacmxr_open_file(char *filename)
{
    Pacmxr_Context *ctx = &global_pacmxr_ctx;
    const int fail = 0, success = 1; 
    File_Context *fctx = &ctx->fctx;

    if (avformat_open_input(&fctx->avf_ctx, filename, 0, 0) < 0) {
        fprintf(stderr, "avformat could not open input file %s\n", filename);
        return fail;
    }

    if (avformat_find_stream_info(fctx->avf_ctx, 0) < 0) {
        fprintf(stderr, "avformat could not find stream information\n");
        return fail;
    }

    fctx->stream_index = -1;
    for (uint32_t i = 0; i < fctx->avf_ctx->nb_streams; i++) {
        fctx->astream = fctx->avf_ctx->streams[i];
        if (fctx->astream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            fctx->stream_index = i;
            break;
        }
    }
    fctx->astream = fctx->avf_ctx->streams[fctx->stream_index];

    if (fctx->stream_index < 0) {
        fprintf(stderr, "could not find an audio stream in the file\n");
        return fail;
    }

    AVCodecParameters *codecpar = fctx->avf_ctx->streams[fctx->stream_index]->codecpar;
    fctx->codec = (AVCodec *)avcodec_find_decoder(codecpar->codec_id);
    if (!fctx->codec) {
        fprintf(stderr, "avcodec failed to find audio codec\n");
        return fail;
    }

    fctx->avc_ctx = avcodec_alloc_context3(fctx->codec);
    if (!fctx->avc_ctx) {
        fprintf(stderr, "avcodec failed to allocate the codec context\n");
        return fail;
    }

    if (avcodec_parameters_to_context(fctx->avc_ctx, codecpar) < 0) {
        fprintf(stderr, "avccodec failed to copy codec parameters to codec context\n");
        return fail;
    }

    if (avcodec_open2(fctx->avc_ctx, fctx->codec, 0) < 0) {
        fprintf(stderr, "avcodec failed to open codec\n");
        return fail;
    }

    if (!fctx->avc_ctx->ch_layout.u.mask) {
        av_channel_layout_default(&fctx->avc_ctx->ch_layout, 
                fctx->avc_ctx->ch_layout.nb_channels);
    }

    av_channel_layout_default(&fctx->out_chan_layout, 2);
    int swr_status = swr_alloc_set_opts2(&fctx->swr_ctx,
            &fctx->out_chan_layout,
            PACMXR_OUTPUT_AUDIO_SAMPLE_FMT,
            PACMXR_RESAMPLE_SAMPLERATE,
            &fctx->avc_ctx->ch_layout,
            fctx->avc_ctx->sample_fmt,
            fctx->avc_ctx->sample_rate,
            0, 0);
    if (swr_status) {
        fprintf(stderr, "swresample failed to set options for swresample\n");
        return fail;
    }

    if (!fctx->swr_ctx || swr_init(fctx->swr_ctx) < 0) {
        fprintf(stderr, "swresample failed to initialize the resampling context\n");
        return fail;
    }

    fctx->packet = av_packet_alloc();
    if (!fctx->packet) {
        fprintf(stderr, "avcodec failed to allocate packet\n");
        return fail;
    }
    fctx->frame = av_frame_alloc();
    if (!fctx->frame) {
        fprintf(stderr, "avcodec failed to allocate frame\n");
        return fail;
    }

    Audio_Queue *aq = &ctx->aqueue;
    aq->total_decode_bytes = pacmxr_estimate_decode_size();
    //aq->num_chunks = ceil(aq->total_decode_bytes/PACMXR_AUDIOQUEUE_BUFFER_SIZE);
    aq->num_chunks = pacmxr_ceilf(pacmxr_stream_duration()/PACMXR_AUDIOQUEUE_MAX_SECS);
    aq->chunk_index = -1;

#if 0
    aq->backbuffer_bytes = 0;
    aq->bytes_left = 0;
    aq->read_ptr = aq->buffer1;
    aq->frontbuffer_bytes = 0;
    aq->write_ptr = aq->buffer1;
    aq->front_buffer = aq->buffer2;
#endif

    if (pacmxr__ffmpeg_resample_and_queue(-1) < 0) {
        fprintf(stderr, "failed to resample audio\n");
        avcodec_send_packet(ctx->fctx.avc_ctx, 0);
        return fail;
    }

#if PACMXR_DEBUG
    fprintf(stderr, "[pacmxr_open_file] (file: %s)\n"
                    "bytes_left=%zu\n"
                    "back_bytes=%zu, front_bytes=%zu total=%zu\n"
                    "num_chunks=%d\n",
                    filename,
                    aq->bytes_left,
                    aq->backbuffer_bytes, aq->frontbuffer_bytes, aq->total_decode_bytes,
                    aq->num_chunks);
#endif
    ctx->paused = 0;
    ctx->fctx.is_open = 1;

    SDL_UnlockAudioDevice(ctx->au_dev);
    pacmxr_pause(0);
    SDL_UnlockAudioDevice(ctx->au_dev);
    /*
    HACK: this (below) delay seems to prevent a bug in 2pacwav where only the first time this function
    gets called after pacmxr_init, the file is actually opened properly and everything works but something
    in 2pacwav decides to immediately call this function again with the next file, thus skipping
    the file the user wanted to play. this is probably due to inconsistencies in state after
    calls to pacmxr_init and pacmxr_close_file, so that should be looked into
    */
    static char first = 1;
    if (first) {
        SDL_Delay(100);
        first = 0;
    }

    return success;
}

PACMXR_INLINE size_t pacmxr_estimate_decode_size(void)
{
    float seconds = pacmxr_stream_duration();
    size_t estimate = pacmxr_ceilf(seconds) *
                        PACMXR_RESAMPLE_SAMPLERATE *
                        PACMXR_CHAN_COUNT *
                        PACMXR_BYTES_PER_SAMPLE;
    return estimate;
}

PACMXR_DEF void pacmxr_close_file(void)
{
    //NOTE: idk if this breaks shit but it seems 2 fix an infinite loop in 2pacwav
    if (!global_pacmxr_ctx.fctx.is_open)
    { return; }
    Pacmxr_Context *ctx = &global_pacmxr_ctx;
    SDL_LockAudioDevice(ctx->au_dev);
    //SDL_PauseAudioDevice(ctx->au_dev, 1);
    ctx->paused = 1;
    File_Context *fctx = &ctx->fctx;

    if (fctx->frame)
    { av_frame_free(&fctx->frame); }
    if (fctx->packet)
    { av_packet_free(&fctx->packet); }
    if (fctx->swr_ctx)
    { swr_free(&fctx->swr_ctx); }
    if (fctx->avc_ctx) {
        avcodec_send_packet(fctx->avc_ctx, 0);
        avcodec_flush_buffers(fctx->avc_ctx);
        avcodec_free_context(&fctx->avc_ctx); 
    }
    if (fctx->avf_ctx) { avformat_close_input(&fctx->avf_ctx); }
    pacmxr__reset_queue(&ctx->aqueue, 1);
    ctx->fctx.is_open = 0;
    //SDL_PauseAudioDevice(ctx->au_dev, 0);
    //SDL_UnlockAudioDevice(ctx->au_dev);
}

PACMXR_DEF void pacmxr__reset_queue(Audio_Queue *aq, char zero)
{
    aq->write_ptr = aq->buffer1;
    aq->read_ptr = aq->buffer1;
    aq->bytes_left = 0;
    aq->front_buffer = aq->buffer2;
    aq->chunk_index = -1;
    if (zero) { 
        memset(aq->buffer1, 0, PACMXR_AUDIOQUEUE_BUFFER_SIZE*2);
    }
    aq->backbuffer_bytes = 0;
    aq->frontbuffer_bytes = 0;
}

//metadata things

PACMXR_DEF int pacmxr_meta_open_file(Pacmxr_Metadata *pac_meta, char *filename)
{
    pac_meta->in_avf_ctx = 0;
    pac_meta->out_avf_ctx = 0;
    pac_meta->out_metadata = 0;
    pac_meta->stream_index = 0;

    if (avformat_open_input(&pac_meta->in_avf_ctx, filename, 0, 0) < 0) 
    { return 0; }
    AVFormatContext *in_ctx = pac_meta->in_avf_ctx;

    for (uint32_t i = 0; i < in_ctx->nb_streams; i++) {
        if (in_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            pac_meta->stream_index = i;
            break;
        }
    }

    if (av_dict_count(pac_meta->in_avf_ctx->metadata)) {
        av_dict_copy(&pac_meta->out_metadata, pac_meta->in_avf_ctx->metadata, 0);
    } else {
        av_dict_copy(&pac_meta->out_metadata, in_ctx->streams[pac_meta->stream_index]->metadata, 0);
    }

    return 1;
}

PACMXR_DEF void pacmxr_meta_close_file(Pacmxr_Metadata *pac_meta)
{
    if (!(pac_meta && pac_meta->in_avf_ctx)) { return; }
    avformat_close_input(&pac_meta->in_avf_ctx);
    if (pac_meta->out_avf_ctx) {
        avformat_free_context(pac_meta->out_avf_ctx);
    } if (pac_meta->out_metadata) {
        av_dict_free(&pac_meta->out_metadata);
    }
}

PACMXR_DEF int pacmxr_meta_get_title(Pacmxr_Metadata *pac_meta, 
                                    char *dest,
                                    int dest_size)
{
    int ret = 0;
    Pacmxr_Context *ctx = &global_pacmxr_ctx;
    AVFormatContext *avf_ctx;
    uint32_t stream_index;
    if (!pac_meta) {
        avf_ctx = ctx->fctx.avf_ctx;
        stream_index = ctx->fctx.stream_index;
    } else {
        avf_ctx = pac_meta->in_avf_ctx;
        stream_index = pac_meta->stream_index;
    }

    if (!(avf_ctx && dest && dest_size)) 
    { return ret; }
    AVDictionaryEntry *tag = 0;
    tag = av_dict_get(avf_ctx->metadata,
                "title",
                tag,
                AV_DICT_IGNORE_SUFFIX);
    //prefer tags in avf_ctx->metadata ("container-level")
    //attempt to fall back to tags in the stream being used ("stream-level")
    if (tag) {
        ret = snprintf(dest, dest_size, "%s", tag->value);
    } else {
        tag = av_dict_get(avf_ctx->streams[stream_index]->metadata,
                    "title",
                    tag,
                    AV_DICT_IGNORE_SUFFIX);
        if (tag) {
            ret = snprintf(dest, dest_size, "%s", tag->value);
        }
    }

    return ret;
}

PACMXR_DEF int pacmxr_meta_get_artist(Pacmxr_Metadata *pac_meta, 
                                    char *dest,
                                    int dest_size)
{
    int ret = 0;
    Pacmxr_Context *ctx = &global_pacmxr_ctx;
    AVFormatContext *avf_ctx;
    uint32_t stream_index;
    if (!pac_meta) {
        avf_ctx = ctx->fctx.avf_ctx;
        stream_index = ctx->fctx.stream_index;
    } else {
        avf_ctx = pac_meta->in_avf_ctx;
        stream_index = pac_meta->stream_index;
    }

    if (!(avf_ctx && dest && dest_size)) 
    { return ret; }
    AVDictionaryEntry *tag = 0;
    tag = av_dict_get(avf_ctx->metadata,
                "artist", 
                tag,
                AV_DICT_IGNORE_SUFFIX);
    if (tag) {
        ret = snprintf(dest, dest_size, "%s", tag->value);
    } else {
        tag = av_dict_get(avf_ctx->streams[stream_index]->metadata,
                    "artist",
                    tag,
                    AV_DICT_IGNORE_SUFFIX);
        if (tag) {
            ret = snprintf(dest, dest_size, "%s", tag->value);
        }
    }

    return ret;
}

PACMXR_DEF int pacmxr_meta_get_album(Pacmxr_Metadata *pac_meta, 
                                    char *dest,
                                    int dest_size)
{
    int ret = 0;
    Pacmxr_Context *ctx = &global_pacmxr_ctx;
    AVFormatContext *avf_ctx;
    uint32_t stream_index;
    if (!pac_meta) {
        avf_ctx = ctx->fctx.avf_ctx;
        stream_index = ctx->fctx.stream_index;
    } else {
        avf_ctx = pac_meta->in_avf_ctx;
        stream_index = pac_meta->stream_index;
    }

    if (!(avf_ctx && dest && dest_size)) 
    { return ret; }
    AVDictionaryEntry *tag = 0;
    tag = av_dict_get(avf_ctx->metadata,
                "album",
                tag,
                AV_DICT_IGNORE_SUFFIX);
    if (tag) {
        ret = snprintf(dest, dest_size, "%s", tag->value);
    } else {
        tag = av_dict_get(avf_ctx->streams[stream_index]->metadata,
                    "album",
                    tag,
                    AV_DICT_IGNORE_SUFFIX);
        if (tag) {
            ret = snprintf(dest, dest_size, "%s", tag->value);
        }
    }

    return ret;
}

PACMXR_DEF int pacmxr_meta_get_genre(Pacmxr_Metadata *pac_meta, 
                                    char *dest,
                                    int dest_size)
{
    int ret = 0;
    Pacmxr_Context *ctx = &global_pacmxr_ctx;
    AVFormatContext *avf_ctx;
    uint32_t stream_index;
    if (!pac_meta) {
        avf_ctx = ctx->fctx.avf_ctx;
        stream_index = ctx->fctx.stream_index;
    } else {
        avf_ctx = pac_meta->in_avf_ctx;
        stream_index = pac_meta->stream_index;
    }

    if (!(avf_ctx && dest && dest_size)) 
    { return ret; }
    AVDictionaryEntry *tag = 0;
    tag = av_dict_get(avf_ctx->metadata,
                "genre",
                tag,
                AV_DICT_IGNORE_SUFFIX);
    if (tag) {
        ret = snprintf(dest, dest_size, "%s", tag->value);
    } else {
        tag = av_dict_get(avf_ctx->streams[stream_index]->metadata,
                    "genre",
                    tag,
                    AV_DICT_IGNORE_SUFFIX);
        if (tag) {
            ret = snprintf(dest, dest_size, "%s", tag->value);
        }
    }

    return ret;
}

PACMXR_DEF int pacmxr_meta_get_year(Pacmxr_Metadata *pac_meta)
{
    int ret = -1;
    Pacmxr_Context *ctx = &global_pacmxr_ctx;
    AVFormatContext *avf_ctx;
    uint32_t stream_index;
    if (!pac_meta) {
        avf_ctx = ctx->fctx.avf_ctx;
        stream_index = ctx->fctx.stream_index;
    } else {
        avf_ctx = pac_meta->in_avf_ctx;
        stream_index = pac_meta->stream_index;
    }

    if (!avf_ctx) 
    { return ret; }
    AVDictionaryEntry *tag = 0;
    tag = av_dict_get(avf_ctx->metadata,
                "date",
                tag,
                AV_DICT_IGNORE_SUFFIX);
    if (tag) {
        errno = 0;
        int value = strtol(tag->value, 0, 10);
        if (errno != ERANGE)
        { ret = value; }
    } else {
        tag = av_dict_get(avf_ctx->streams[stream_index]->metadata,
                    "date",
                    tag,
                    AV_DICT_IGNORE_SUFFIX);
        if (tag) {
            errno = 0;
            int value = strtol(tag->value, 0, 10);
            if (errno != ERANGE)
            { ret = value; }
        }
    }

    return ret;
}

PACMXR_DEF int pacmxr_meta_get_track(Pacmxr_Metadata *pac_meta)
{
    int ret = -1;
    Pacmxr_Context *ctx = &global_pacmxr_ctx;
    AVFormatContext *avf_ctx;
    uint32_t stream_index;
    if (!pac_meta) {
        avf_ctx = ctx->fctx.avf_ctx;
        stream_index = ctx->fctx.stream_index;
    } else {
        avf_ctx = pac_meta->in_avf_ctx;
        stream_index = pac_meta->stream_index;
    }

    if (!avf_ctx) 
    { return ret; }
    AVDictionaryEntry *tag = 0;
    tag = av_dict_get(avf_ctx->metadata,
                "track",
                tag,
                AV_DICT_IGNORE_SUFFIX);
    if (tag) {
        errno = 0;
        int value = strtol(tag->value, 0, 10);
        if (errno != ERANGE)
        { ret = value; }
    } else {
        tag = av_dict_get(avf_ctx->streams[stream_index]->metadata,
                    "track",
                    tag,
                    AV_DICT_IGNORE_SUFFIX);
        if (tag) {
            errno = 0;
            int value = strtol(tag->value, 0, 10);
            if (errno != ERANGE)
            { ret = value; }
        }
    }
    return ret;
}

PACMXR_DEF uint8_t *pacmxr_meta_get_cover(Pacmxr_Metadata *pac_meta, int *out_size)
{
    uint8_t *ret = 0;
    Pacmxr_Context *ctx = &global_pacmxr_ctx;
    AVFormatContext *avf_ctx;
    if (!pac_meta) {
        avf_ctx = ctx->fctx.avf_ctx;
    } else {
        avf_ctx = pac_meta->in_avf_ctx;
    }
    if (!avf_ctx)
    { return ret; }

    AVPacket *pkt;
    for (uint32_t stream_i = 0; stream_i < avf_ctx->nb_streams; stream_i++) {
        if (avf_ctx->streams[stream_i]->disposition & AV_DISPOSITION_ATTACHED_PIC) {
            pkt = &avf_ctx->streams[stream_i]->attached_pic;
            ret = pkt->data;
            if (out_size)
            { *out_size = pkt->size; }
            break;
        }
    }

    return ret;
}

PACMXR_DEF void pacmxr_meta_set_title(Pacmxr_Metadata *pac_meta, char *value)
{
    av_dict_set(&pac_meta->out_metadata, "title", value, 0);
}

PACMXR_DEF void pacmxr_meta_set_artist(Pacmxr_Metadata *pac_meta, char *value)
{
    av_dict_set(&pac_meta->out_metadata, "artist", value, 0);
}

PACMXR_DEF void pacmxr_meta_set_album(Pacmxr_Metadata *pac_meta, char *value)
{
    av_dict_set(&pac_meta->out_metadata, "album", value, 0);
}

PACMXR_DEF void pacmxr_meta_set_genre(Pacmxr_Metadata *pac_meta, char *value)
{
    av_dict_set(&pac_meta->out_metadata, "genre", value, 0);
}

PACMXR_DEF void pacmxr_meta_set_year(Pacmxr_Metadata *pac_meta, int value)
{
    av_dict_set_int(&pac_meta->out_metadata, "date", value, 0);
}

PACMXR_DEF void pacmxr_meta_set_track(Pacmxr_Metadata *pac_meta, int value)
{
    av_dict_set_int(&pac_meta->out_metadata, "track", value, 0);
}

#ifndef PATH_MAX
    #define PATH_MAX (4096)
#endif

PACMXR_DEF int pacmxr_copy_file(char *dst, char *src)
{
    int ret = 0;
    FILE *dstf = fopen(dst, "wb");
    FILE *srcf = fopen(src, "rb");

    if (dstf && srcf) {
        fseek(srcf, 0, SEEK_END);
        size_t src_size = ftell(srcf);
        fseek(srcf, 0, SEEK_SET);
        void *src_data = malloc(src_size);
        if (src_data) {
            fread(src_data, src_size, 1, srcf);
            fwrite(src_data, src_size, 1, dstf);
            free(src_data);
            ret = 1;
        }
        fclose(srcf);
        fclose(dstf);
    }
    return ret;
}

PACMXR_DEF int pacmxr_meta_save(Pacmxr_Metadata *pac_meta)
{
#if PACMXR_UNIX
    if (!pac_meta->in_avf_ctx) { return 0; }
    AVFormatContext *inctx = pac_meta->in_avf_ctx;
    AVFormatContext *outctx = pac_meta->out_avf_ctx;

    if (avformat_find_stream_info(inctx, 0) < 0) {
        fprintf(stderr, "failed to find stream information\n");
        return 0;
    }

    char *filename = inctx->url;
    char tempfile[PATH_MAX];
    //char og_filename[PATH_MAX];
    int fnamelen = strlen(filename);
    char *filename_real = 0;
#ifdef __cplusplus
    AVRounding flags = (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
#else
    int flags = AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX;
#endif
    if (strchr(filename, '/')) {
        for (int i = fnamelen; i >= 0; --i) {
            if (filename[i] == '/') {
                filename_real = &filename[i + 1];
                break;
            }
        }
    } else {
        filename_real = filename;
    }
    snprintf(tempfile, PATH_MAX, "/tmp/%ld-%s", time(0), filename_real);

    if (avformat_alloc_output_context2(&outctx, 0, 0, tempfile) < 0) {
        fprintf(stderr, "avformat failed to allocate output context\n");
        return 0;
    }

    AVStream *in_stream, *out_stream;
    for (uint32_t stream_i = 0; 
        stream_i < inctx->nb_streams; 
        ++stream_i) {
        in_stream = inctx->streams[stream_i];
        out_stream = avformat_new_stream(outctx, 0);
        if (!out_stream) {
            fprintf(stderr, "avformat failed to allocate output stream\n");
            goto error;
        }
        avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
    }

    av_dict_copy(&outctx->metadata, pac_meta->out_metadata, 0);

    if (!(outctx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&outctx->pb, tempfile, AVIO_FLAG_WRITE) < 0) {
            fprintf(stderr, "could not open output file '%s'\n", tempfile);
            goto error;
        }
    }

    if (avformat_write_header(outctx, 0) < 0) {
        fprintf(stderr, "avformat failed to write header\n");
        goto error;
    }

    AVPacket pkt;
    while (av_read_frame(inctx, &pkt) >= 0) {
        in_stream  = inctx->streams[pkt.stream_index];
        out_stream = outctx->streams[pkt.stream_index];

        pkt.pts = av_rescale_q_rnd(pkt.pts, 
                        in_stream->time_base, 
                        out_stream->time_base,
                        flags);
        pkt.dts = av_rescale_q_rnd(pkt.dts, 
                        in_stream->time_base, 
                        out_stream->time_base,
                        flags);
        pkt.duration = av_rescale_q(pkt.duration, 
                            in_stream->time_base, 
                            out_stream->time_base);
        pkt.pos = -1;

        av_interleaved_write_frame(outctx, &pkt);
        av_packet_unref(&pkt);
    }

    av_write_trailer(outctx);

    if (outctx && !(outctx->oformat->flags & AVFMT_NOFILE))
    { avio_closep(&outctx->pb); }
    avformat_free_context(outctx);

    pacmxr_copy_file(filename, tempfile);
    remove(tempfile);

    return 1;

error:
    avformat_free_context(outctx);
    return 0;
#endif
}

#endif //PACMXR_HEADER_ONLY

#ifdef __cplusplus
}
#endif

#define _2PACMIXER_DOT_H 1
#endif
