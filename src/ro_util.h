
/*
File: ro_util.h
Date: Tue 8 Oct 2024 03:18:58 PM EET
*/

#ifndef RO_UTIL_DOT_H

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef _DEFAULT_SOURCE
    #define _DEFAULT_SOURCE 1
#endif
#ifndef D_POSIX_C_SOURCE
    #define D_POSIX_C_SOURCE 200809L
#endif
#ifndef _POSIX_C_SOURCE
    #define _POSIX_C_SOURCE 200809L
#endif
#ifndef __USE_XOPEN_EXTENDED
    #define __USE_XOPEN_EXTENDED 1
#endif
#ifndef __USE_XOPEN2K
    #define __USE_XOPEN2K 1
#endif

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <limits.h>

//NOTE: some mingw stuff can use the posix functions
#if RO_UTIL_POSIX && !RO_UTIL_W32
    #define RO_UTIL_POSIX_4REAL 1
#endif

#include <sys/stat.h>
#if RO_UTIL_POSIX_4REAL
    #include <sys/mman.h>
    #include <sys/select.h>
#elif RO_UTIL_W32
    #define WIN32_LEAN_AND_MEAN 1
    #include <windows.h>
    #include <shlwapi.h>
#endif

#ifndef NAME_MAX
    #define NAME_MAX (FILENAME_MAX)
#endif


//int clock_gettime(clockid_t clockid, struct timespec *tp);
//int nanosleep(const struct timespec *rqtp, struct timespec *rmtp);

#ifndef CLOCK_MONOTONIC
    #define CLOCK_MONOTONIC (1)
#endif

#ifndef RO_DEF 
    #define RO_DEF static inline
#endif

#ifndef RO_ZERO_INIT
    #if defined(__cplusplus) && (__cplusplus >= 201103L)
        #define RO_ZERO_INIT
    #else
        #define RO_ZERO_INIT 0
    #endif
#endif

#ifndef RO_MATH_DOT_H

RO_DEF uint64_t ro_abs_i64(int64_t number)
{
    uint64_t result = (uint64_t)number;
    uint64_t mask = number >> 63;
    result ^= mask;
    result += mask & 1;
    return result;
}

RO_DEF uint32_t ro_abs_i32(int32_t number)
{
    uint32_t result = (uint32_t)number;
    uint32_t mask = number >> 31;
    result ^= mask;
    result += mask & 1;
    return result;
}

RO_DEF float ro_abs_f32(float number)
{
    float result = number;
    *(uint32_t *)&result &= (0xFFFFFFFF >> 1);
    return result;
}

RO_DEF double ro_rand_norm(void)
{
    return rand()/(RAND_MAX + 1.0);
}

#define RO_MATH_DOT_H 1
#endif

#if !defined(RO_HEAP_BUFFER) && !defined(RO_HEAPBUF_DOT_H)

typedef struct Ro_Heap_Buffer
{
    void *memory;
    void *write_ptr;
    uint64_t total_bytes;
} Ro_Heap_Buffer;

RO_DEF uint64_t ro_buffer_unallocated_bytes(Ro_Heap_Buffer *buffer)
{
    uint64_t result = 0;
    if (buffer && buffer->memory)
    {
        result = ((uint64_t)buffer->memory + 
                buffer->total_bytes) - 
                (uint64_t)buffer->write_ptr;
    }
    return result;
}


RO_DEF void *ro_buffer_alloc_region(struct Ro_Heap_Buffer *buffer, uint64_t region_bytes)
{
    void *result = 0;
    uint64_t free_bytes = ro_buffer_unallocated_bytes(buffer);
    if (region_bytes <= free_bytes) {
        result = buffer->write_ptr;
        buffer->write_ptr = (void *)((uintptr_t)buffer->write_ptr + region_bytes);
    } 
    return result;
}

RO_DEF void ro_buffer_move_writeptr(Ro_Heap_Buffer *buffer, 
                                    ssize_t bytes, 
                                    char write_zeroes)
{
    if (!(buffer && buffer->memory && bytes))
    { return; }
    uintptr_t buf_begin = (uintptr_t)buffer->memory;
    uintptr_t buf_end = buf_begin + buffer->total_bytes;
    uintptr_t current_pos = (uintptr_t)buffer->write_ptr;
    uintptr_t future_pos = current_pos + bytes;

    if ((future_pos >= buf_begin) && 
        (future_pos < buf_end))
    {
        buffer->write_ptr = (void *)future_pos;
        if (write_zeroes)
        {
            uint64_t move_bytes = ro_abs_i64(bytes);
            memset(buffer->write_ptr, 0, move_bytes);
        }
    }
}

#define RO_HEAP_BUFFER 1
#define RO_HEAPBUF_DOT_H 1
#endif

#if defined(RO_UTIL_POSIX) && RO_UTIL_POSIX

RO_DEF void *ro_posix_make_heap_buffer(Ro_Heap_Buffer *target, uint64_t bytes)
{
#if RO_UTIL_POSIX_4REAL
    target->memory = mmap(0, bytes, PROT_READ|PROT_WRITE, 
                        MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
#elif RO_UTIL_W32
    target->memory = VirtualAlloc(0, bytes,
                        MEM_RESERVE|MEM_COMMIT,
                        PAGE_READWRITE);
#endif
    target->write_ptr = target->memory;
    target->total_bytes = bytes;
    return target->memory;
}

RO_DEF void ro_posix_free_heap_buffer(Ro_Heap_Buffer *buffer)
{
    if (buffer && buffer->memory && buffer->total_bytes)
    {
#if RO_UTIL_POSIX_4REAL
    munmap(buffer->memory, buffer->total_bytes);
#elif RO_UTIL_W32
    VirtualFree(buffer->memory, buffer->total_bytes, MEM_RELEASE);
#endif
    }
}

RO_DEF uint64_t ro_posix_get_timestamp(void)
{
    uint64_t result;
    struct timespec tspec = {RO_ZERO_INIT};
    clock_gettime(CLOCK_MONOTONIC, &tspec);
    result = (tspec.tv_sec*1000000) + (tspec.tv_nsec/1000);
    return result;
}

RO_DEF void ro_posix_sleep_usec(uint64_t usec)
{
    uint64_t nanoseconds = usec*1000;
    struct timespec tspec = {RO_ZERO_INIT};
    tspec.tv_nsec = nanoseconds;
    nanosleep(&tspec, 0);
}

RO_DEF uint64_t ro_posix_read_file(char *file_path, 
                                char *destination, 
                                uint64_t dest_size)
{
    int file_descriptor = open(file_path, O_RDONLY);
    uint64_t bytes_read = 0;
    if (file_descriptor != -1)
    {
        struct stat stat_buf;
        fstat(file_descriptor, &stat_buf);
        if (stat_buf.st_size <= (int64_t)dest_size)
        {
            bytes_read = read(file_descriptor, destination, stat_buf.st_size);
            close(file_descriptor);
        }
    }
    return bytes_read;
}

RO_DEF int ro_posix_write_file(char *file_path, 
                            void *in_buffer, 
                            uint64_t buffer_size)
{
    int result = 0;
    int file_descriptor = open(file_path, 
                            O_CREAT|O_WRONLY|O_TRUNC, 
                            S_IRUSR|S_IWUSR);
    if (file_descriptor != -1)
    {
        int64_t write_status = write(file_descriptor, 
                                    in_buffer, 
                                    buffer_size);  
        close(file_descriptor);
        if (write_status == (int64_t)buffer_size)
        { result = 1; }
    }
    return result;
}

#if RO_UTIL_POSIX_4REAL

RO_DEF char ro_posix_path_exists(char *path)
{
    char result = 0;
    struct stat stat_struct;
    if (!stat(path, &stat_struct))
    { result = 1; }
    return result;
}

RO_DEF char ro_posix_file_exists(char *file_path)
{
    char result = 0;
    struct stat stat_struct;
    if (!stat(file_path, &stat_struct) && 
        !(S_ISDIR(stat_struct.st_mode)))
    { result = 1; }
    return result;
}

RO_DEF char ro_posix_directory_exists(char *directory_name)
{
    char result = 0;
    struct stat stat_struct;
    if (!stat(directory_name, &stat_struct) && 
        S_ISDIR(stat_struct.st_mode))
    { result = 1; }
    return result;
}

RO_DEF char *ro_posix_get_working_directory(char *destination, uint64_t buffer_size)
{
    char delimiter = '/';
    size_t bytes_read = readlink("/proc/self/exe", destination, buffer_size);
    destination[bytes_read] = 0x0;
    for (int char_index = (int)bytes_read; 
        char_index >= 0; 
        --char_index)
    {
        if (destination[char_index] != '/') { destination[char_index] = 0x0; }
        else { break; }
    }

    return destination;
}

RO_DEF int ro_posix_run_command(char *command, 
                            int *output_fd, 
                            pid_t *proc_id, 
                            bool include_stderr)
{
    int result = 0;
    int pipe_fd[2];
    if (-1 == pipe(pipe_fd))
    {
        perror("pipe");
        _exit(1);
    }

    *proc_id = fork();
    if (-1 == *proc_id)
    {
        perror("fork");
        _exit(1);
    }
    else if (0 == *proc_id)
    {
        close(pipe_fd[STDIN_FILENO]);
        dup2(pipe_fd[STDOUT_FILENO], STDOUT_FILENO);
        if (include_stderr) { dup2(STDOUT_FILENO, STDERR_FILENO); }
        close(pipe_fd[STDOUT_FILENO]);
        char tempbuf[1024*8];
        snprintf(tempbuf, sizeof(tempbuf), "''%s''", command);
        execl("/bin/sh", "sh", "-c", tempbuf, (char *)0);
        perror("execl");
        _exit(1);
    }
    else
    {
        close(pipe_fd[STDOUT_FILENO]);
        *output_fd = pipe_fd[STDIN_FILENO];
        result = 1;
    }
    return result;
}

#endif

RO_DEF size_t ro_posix_read_stream_from_fd(int file_descriptor,
                                        char *dest_buffer,
                                        size_t dest_buffer_size,
                                        bool close_descriptor)
{
    size_t bytes_read = 0, bytes_read_total = 0;
    int timeout = 1000;
    while (timeout--)
    {
        if (((ssize_t)dest_buffer_size - (ssize_t)bytes_read_total) < 0)
        { break; }
        bytes_read = read(file_descriptor,
                        dest_buffer + bytes_read_total,
                        dest_buffer_size - bytes_read_total);
        bytes_read_total += bytes_read;
        if (!bytes_read) { break; }
    }
    dest_buffer[bytes_read_total] = 0x0;
    if (close_descriptor) { close(file_descriptor); }
    return bytes_read_total;
}

#endif

#if defined(RO_UTIL_W32) && RO_UTIL_W32

RO_DEF char *ro_w32_get_working_directory(char *destination, DWORD buffer_size)
{
    char *result = 0;
    wchar_t dest_temp[PATH_MAX];
    GetModuleFileNameW(0, dest_temp, buffer_size);
    if (!dest_temp[0]) { return 0; }
    int path_length = WideCharToMultiByte(CP_UTF8,
                            0,
                            dest_temp,
                            -1,
                            destination,
                            buffer_size,
                            0, 0);
    if (path_length)
    {
        result = destination;
        for (DWORD char_index = path_length - 1;
            destination[char_index] != '\\';
            --char_index) 
        { destination[char_index] = '\0'; }
    }

    return result;
}

#ifndef RO_PATH_MAX
    #define RO_W32_PATH_MAX (4096)
#endif
static wchar_t ro_w32__global_wchar_buf[RO_W32_PATH_MAX];

RO_DEF int ro_w32_utf8_to_utf16(wchar_t *dest, char *src, size_t num_chars)
{
    int path_len = MultiByteToWideChar(CP_UTF8,
                        MB_ERR_INVALID_CHARS,
                        src,
                        -1,
                        dest,
                        num_chars);
    return path_len;
}

RO_DEF int ro_w32_utf16_to_utf8(char *dest, wchar_t *src, size_t num_chars)
{
    int path_len = WideCharToMultiByte(CP_UTF8, 0, src, -1, dest, num_chars, 0, 0);
    return path_len;
}

RO_DEF char ro_w32_file_exists(char *file_path)
{
    char result = 0;
    wchar_t *tempbuf = ro_w32__global_wchar_buf;
    tempbuf[0] = L'\0';
    int path_len = ro_w32_utf8_to_utf16(tempbuf, file_path, RO_W32_PATH_MAX);
    if (PathFileExistsW(tempbuf) && !PathIsDirectoryW(tempbuf))
    { result = 1; }
    return result;
}

RO_DEF char ro_w32_directory_exists(char *directory_name)
{
    char result = 0;
    wchar_t *tempbuf = ro_w32__global_wchar_buf;
    tempbuf[0] = L'\0';
    int path_len = ro_w32_utf8_to_utf16(tempbuf, directory_name, RO_W32_PATH_MAX);
    if (PathIsDirectoryW(tempbuf)) { result = 1; }
    return result;
}

RO_DEF char ro_w32_path_exists(char *path_name)
{
    char result = 0;
    wchar_t *tempbuf = ro_w32__global_wchar_buf;
    tempbuf[0] = L'\0';
    int path_len = ro_w32_utf8_to_utf16(tempbuf, path_name, RO_W32_PATH_MAX);
    if (PathFileExistsW(tempbuf)) { result = 1; }
    return result;
}

#endif

#ifdef __cplusplus

}
#endif

#define RO_UTIL_DOT_H 1
#endif
