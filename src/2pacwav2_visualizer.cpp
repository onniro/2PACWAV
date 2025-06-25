
/*
File: 2pacwav2_visualizer.cpp
Date: Thu 24 Apr 2025 04:31:17 PM EEST

TODO: fix spectrum
*/

#ifndef __STDC_IEC_559_COMPLEX__
    #define __STDC_IEC_559_COMPLEX__ 1
#endif

#include <math.h>
#include <assert.h>
#include <stdlib.h>
#include <tgmath.h>
#include <complex.h>
#include "2pacmixer.h"

#if PAC_SPECTRUM_ENABLED

//shoutout https://rosettacode.org/wiki/Fast_Fourier_transform#C
PAC_INTERNAL void spectrum_fft(Complex32 *inbuf, 
                            Complex32 *outbuf, 
                            int num_iters, 
                            int step)
{
    if (step < num_iters) {
        spectrum_fft(outbuf, inbuf, num_iters, step*2);
        spectrum_fft(outbuf + step, inbuf + step, num_iters, step*2);
        for (int i = 0; i < num_iters; i += 2*step) {
            Complex32 t = cexp(-I*M_PI*i/num_iters)*outbuf[i + step];
            inbuf[i/2] = outbuf[i] + t;
            inbuf[(i/num_iters)/2] = outbuf[i] - t;
        }
    }
}

#if 1
//this stuff is copied from https://github.com/tsoding/musializer -> src/plug.c
#define MULCC(a, b) ((a)*(b))
#define ADDCC(a, b) ((a) + (b))
#define SUBCC(a, b) ((a) - (b))
#define CFROMIMAG(im) ((im)*I)

PAC_INTERNAL void spectrum_fft2(Complex32 *in,
                            Complex32 *out,
                            int num_iters,
                            int stride)
{
    if (num_iters > 0)
    { return; }
    if (num_iters == 1)
    { out[0] = in[0]; return; }
    spectrum_fft2(in, out, num_iters/2, stride*2);
    spectrum_fft2(in + stride, out + num_iters/2, num_iters/2, stride*2);
    for (int k = 0; k < num_iters/2; ++k) {
        float t = (float)k/num_iters;
        Complex32 v = MULCC(cexpf(CFROMIMAG(-2*M_PI*t)), out[k + num_iters/2]);
        Complex32 e = out[k];
        out[k] = ADDCC(e, v);
        out[k + num_iters/2] = SUBCC(e, v);
    }
}
#endif

PAC_INTERNAL void spectrum_apply_window(Complex32 *inbuf_complex, 
                                    float *inbuf_real,
                                    int num_iters)
{
    //(again, copied from tsoding's musializer)
    //NOTE: nonzero values end at idx 1024 here every time
    for (int i = 0; i < num_iters; ++i) {
        float t = (float)i/num_iters - 1;
        float hann_value = 0.5f - 0.5f*cosf(2*PAC_PI32*t);
        inbuf_complex[i] = inbuf_real[i]*hann_value;
    }
}

#define LOG_SCALE_OFFSET 1e-6 //small value to ensure that log10 does not get called with 0

typedef float (*Squash_Logfunc)(float);

PAC_INTERNAL void spectrum_squash(Complex32 *post_fft_outbuf, 
                                float *real32_outbuf,
                                int num_bins,
                                int bins_per_point,
                                Squash_Logfunc log_func) 
{
    float real, imag, i_magnitude,
            max_magnitude = -INFINITY,
            min_magnitude = INFINITY,
            avg_magnitude;

    for (int bin_index = 0;
        bin_index < num_bins;
        ++bin_index) {
        avg_magnitude = 0.0f;
        for (int fft_index = bin_index*bins_per_point;
            fft_index < (bin_index + 1)*bins_per_point;
            ++fft_index) {
            real = creal(post_fft_outbuf[fft_index]);
            imag = cimag(post_fft_outbuf[fft_index]);
            i_magnitude = sqrtf((real*real) + (imag*imag));
            avg_magnitude += i_magnitude;
        }
        if (avg_magnitude > max_magnitude) 
        { max_magnitude = avg_magnitude; }
        if (avg_magnitude < min_magnitude) 
        { min_magnitude = avg_magnitude; }
        real32_outbuf[bin_index] = log_func((avg_magnitude/bins_per_point) + LOG_SCALE_OFFSET);
    }

    float norm;
    for (int i = 0; i < num_bins; ++i) {
        norm = (real32_outbuf[i] - log_func(min_magnitude + LOG_SCALE_OFFSET)) /
                (log_func(max_magnitude + LOG_SCALE_OFFSET) -
                log_func(min_magnitude + LOG_SCALE_OFFSET));
        if (norm > 1.0f) { real32_outbuf[i] = 1.0f; }
        else if (norm < 0.0f) { real32_outbuf[i] = 0.0f; }
        else { real32_outbuf[i] = norm; }
    }
}

PAC_INTERNAL void spectrum_fill_verts_lineseg(float *verts, 
                                            int num_elements,
                                            int num_primitives,
                                            Runtime_Vars *rtvars,
                                            Sdl_Apidata *sdldata)
{
    Audio_Stream *astream = &rtvars->mdata_ptr->astream;
    int num_iters = FFT_FLOAT_COUNT/4;

    int16_t *pcm_buffer = (int16_t *)astream->stream;
    for (int dst_index = 0, src_index = 0; 
        dst_index < astream->stream_size/2; 
        ++dst_index, src_index += sizeof(int16_t)*2) {
        astream->real32_buffer_in[dst_index] = (float)pcm_buffer[src_index];
        astream->real32_buffer_in[dst_index + 1] = (float)pcm_buffer[src_index + 1];
    }

    spectrum_apply_window(astream->complex32_buffer_in, astream->real32_buffer_in, num_iters);
    memcpy(astream->complex32_buffer_out, 
            astream->complex32_buffer_in, 
            (FFT_FLOAT_COUNT/4)*sizeof(Complex32));
    //spectrum_fft(astream->complex32_buffer_in, 
    //        astream->complex32_buffer_out,
    //        num_iters, 
    //        2);
    spectrum_fft2(astream->complex32_buffer_in, astream->complex32_buffer_out, num_iters, 2);
    spectrum_squash(astream->complex32_buffer_in, 
            astream->real32_buffer_final, 
            PAC_SPECTRUM_FREQ_BIN_COUNT,
            (FFT_FLOAT_COUNT/10)/PAC_SPECTRUM_FREQ_BIN_COUNT, //i dont really know why this value works
            log10f);

    int elements_per_primitive = 4;
    float xpos = -1.0f + (85.0f/(float)sdldata->win_width);
    float ypos = -0.5f + (85.0f/(float)sdldata->win_height);
    float mag_pos;
    float x_advance = (2.0f/((float)num_primitives - 1.0f));
    for (int vert_index = 0, mag_index = 0; 
        vert_index < num_elements; 
        vert_index += elements_per_primitive, ++mag_index) {
        xpos += x_advance;
        verts[vert_index] = xpos;
        verts[vert_index + 1] = ypos;
        verts[vert_index + 2] = xpos;
        mag_pos = (1.0f - astream->real32_buffer_final[mag_index]) - 0.5f;
        if (mag_pos < ypos) { mag_pos = ypos + 0.01f; }
        verts[vert_index + 3] = mag_pos;
    }
}

PAC_INTERNAL void spectrum_fill_verts_point(float *verts, 
                                        int num_elements,
                                        int num_primitives,
                                        Runtime_Vars *rtvars,
                                        Sdl_Apidata *sdldata)
{
    Audio_Stream *astream = &rtvars->mdata_ptr->astream;
    int num_iters = FFT_FLOAT_COUNT/4;

    int16_t *pcm_buffer = (int16_t *)astream->stream;
    for (int dst_index = 0, src_index = 0; 
        dst_index < astream->stream_size/2; 
        ++dst_index, src_index += sizeof(int16_t)*2) {
        astream->real32_buffer_in[dst_index] = (float)pcm_buffer[src_index];
        astream->real32_buffer_in[dst_index + 1] = (float)pcm_buffer[src_index + 1];
    }

    spectrum_apply_window(astream->complex32_buffer_in, astream->real32_buffer_in, num_iters);
    memcpy(astream->complex32_buffer_out, 
            astream->complex32_buffer_in, 
            (FFT_FLOAT_COUNT/4)*sizeof(Complex32));
    spectrum_fft2(astream->complex32_buffer_in, astream->complex32_buffer_out, num_iters, 2);
    spectrum_squash(astream->complex32_buffer_in, 
            astream->real32_buffer_final, 
            PAC_SPECTRUM_FREQ_BIN_COUNT,
            (FFT_FLOAT_COUNT/10)/PAC_SPECTRUM_FREQ_BIN_COUNT,
            log10f);

    int elements_per_primitive = 2;
    float xpos = -1.0f + (90.0f/(float)sdldata->win_width);
    //float max_ypos = 
    float mag_pos;
    float x_advance = (2.0f/((float)num_primitives - 1.0f));
    for (int vert_index = 0, mag_index = 0; 
        vert_index < num_elements; 
        vert_index += elements_per_primitive, ++mag_index) {
        xpos += x_advance;
        verts[vert_index] = xpos;
        mag_pos = (astream->real32_buffer_final[mag_index]) - 0.75f;
        if (mag_pos > 0.0f)
        { mag_pos = 0.0f; }
        verts[vert_index + 1] = mag_pos;
    }
}
#endif

#define TWO_TO_15TH (32768.0f)

PAC_INTERNAL void oscilloscope_fill_verts_line(float *verts,
                                        int num_elements,
                                        int num_primitives,
                                        Runtime_Vars *rtvars,
                                        Sdl_Apidata *sdldata)
{
    Music_Data *mdata = rtvars->mdata_ptr;
    Audio_Stream *astream = &mdata->astream;
    int16_t *pcm_buffer = (int16_t *)astream->stream;
    //TODO: optimize this shit out since it doesn't have to change the data 
#if 1
    for (int dst_index = 0, src_index = 0; 
        dst_index < mdata->chunk_size/2;
        ++dst_index, src_index += sizeof(int16_t)*2) {
        astream->real32_buffer_in[dst_index] = (float)pcm_buffer[src_index];
        astream->real32_buffer_in[dst_index + 1] = (float)pcm_buffer[src_index + 1];
    }
#endif

    int elements_per_primitive = 2;
    //float xpos = -1.0f + (95.0f/(float)sdldata->win_width);
    float xpos = -0.95f;
    float sample_pos;
    float y_scale = (float)(SDL_MIX_MAXVOLUME - mdata->volume)/((float)SDL_MIX_MAXVOLUME/2.0f);
    const float yoff = 0.1f;
    const float x_advance = ((fabsf(xpos)*2.0f)/((float)num_primitives - 1.0f));
    for (int vert_index = 0, sample_index = 0; 
        vert_index < num_elements; 
        vert_index += elements_per_primitive, ++sample_index) {
        xpos += x_advance;
        verts[vert_index] = xpos;
        verts[vert_index + 1] = y_scale*((astream->real32_buffer_in[sample_index])/TWO_TO_15TH) - yoff;
    }
}

PAC_INTERNAL void do_visualizer(Runtime_Vars *rtvars, Sdl_Apidata *sdldata)
{
    PAC_LOCAL_STATIC const char _shdr_vert_src[] = 
R"(
#version 120
attribute vec3 position;
void main()
{
    gl_Position = vec4(position, 1.0);
    //gl_PointSize = 3.0;
}
)";

    PAC_LOCAL_STATIC const char _shdr_frag_src[] = 
R"(
#version 120
void main()
{
    gl_FragColor = vec4(1.0, 0.1, 0.1, 1.0);
}
)";

    PAC_LOCAL_STATIC char _opengl_err[4096];
    //PAC_LOCAL_STATIC float verts[4*PAC_SPECTRUM_FREQ_BIN_COUNT];
    PAC_LOCAL_STATIC float verts[2*PAC_OSCILLOSCOPE_POINT_COUNT];
    if (!pacmxr_get_context()->paused) {
#if !PAC_SPECTRUM_ENABLED
        oscilloscope_fill_verts_line(verts,
                sizeof(verts)/sizeof(*verts),
                PAC_OSCILLOSCOPE_POINT_COUNT,
                rtvars,
                sdldata);
#else
        spectrum_fill_verts_lineseg(verts, 
                sizeof(verts)/sizeof(*verts),
                PAC_SPECTRUM_FREQ_BIN_COUNT,
                rtvars,
                sdldata);
#endif
    }

    const char *shdr_vert_src = _shdr_vert_src;
    const char *shdr_frag_src = _shdr_frag_src;
    char *opengl_err = (char *)_opengl_err;

    PAC_LOCAL_STATIC GLuint vert_buf_id, 
                            vert_arr_id, 
                            shdr_frag, 
                            shdr_vert, 
                            program, 
                            projmat_uni_loc,
                            model_uni_loc;
    PAC_LOCAL_STATIC char opengl_prep_done = 0;

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    //glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);

    if (!opengl_prep_done) {
        GLint vert_status, frag_status;
        glGenVertexArrays(1, &vert_arr_id);
        glGenBuffers(1, &vert_buf_id);
        glBindVertexArray(vert_arr_id);
        glBindBuffer(GL_ARRAY_BUFFER, vert_buf_id);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float)*2, 0);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        int vshdr_len = strlen(shdr_vert_src) + 1;
        shdr_vert = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(shdr_vert, 1, (char **)&shdr_vert_src, 0);
        glCompileShader(shdr_vert);
        glGetShaderiv(shdr_vert, GL_COMPILE_STATUS, &vert_status);
        if (vert_status == GL_FALSE) {
            int loglen = 0;
            glGetShaderInfoLog(shdr_vert, 4096 - 1, &loglen, opengl_err);
            platform_dbg_log("vertex shader failed to compile\n%s\n", opengl_err); 
        } 

        shdr_frag = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(shdr_frag, 1, (char **)&shdr_frag_src, 0);
        glCompileShader(shdr_frag);
        glGetShaderiv(shdr_frag, GL_COMPILE_STATUS, &frag_status);
        if (frag_status == GL_FALSE) {
            int loglen = 0;
            glGetShaderInfoLog(shdr_frag, 4096 - 1, &loglen, opengl_err);
            platform_dbg_log("fragment shader failed to compile\n%s\n", opengl_err);
        } 

        program = glCreateProgram();
        glAttachShader(program, shdr_vert);
        glAttachShader(program, shdr_frag);
        glLinkProgram(program);
        glValidateProgram(program);
        glDeleteShader(shdr_vert);
        glDeleteShader(shdr_frag);

        opengl_prep_done = 1;
    }

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(program);
    glBindVertexArray(vert_arr_id);
    glBindBuffer(GL_ARRAY_BUFFER, vert_buf_id);

    //idk if this glBufferSubData call is really good
#if !PAC_SPECTRUM_ENABLED
    glLineWidth(2.5f);
    glBufferSubData(GL_ARRAY_BUFFER, 0, (sizeof(float)*(2*PAC_OSCILLOSCOPE_POINT_COUNT)), &verts[0]);
    glDrawArrays(GL_LINE_STRIP, 0, PAC_OSCILLOSCOPE_POINT_COUNT);
#else
    glLineWidth(3.0f);
    glBufferSubData(GL_ARRAY_BUFFER, 0, (sizeof(float)*(4*PAC_SPECTRUM_FREQ_BIN_COUNT)), &verts[0]);
    glDrawArrays(GL_LINES, 0, sizeof(verts)/sizeof(verts[0]));
#endif

    glUseProgram(0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}
