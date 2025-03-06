
/*
File: ro_posix.h
Date: Tue 8 Oct 2024 03:18:58 PM EET
*/

#ifndef RO_POSIX_DOT_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>

#ifndef RO_DEF 
    #define RO_DEF static inline
#endif

#if !defined(RO_HEAP_BUFFER) && !defined(RO_HEAPBUF_DOT_H)

typedef struct ro_heap_buffer 
{
    void *memory;
    void *write_ptr;
    uint64_t total_bytes;
} ro_heap_buffer;

#ifndef RO_MATH

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

#endif

RO_DEF void *ro_posix_make_heap_buffer(ro_heap_buffer *target, uint64_t bytes) 
{
    target->memory = mmap(0, bytes, PROT_READ|PROT_WRITE, 
                        MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    target->write_ptr = target->memory;
    target->total_bytes = bytes;
    return target->memory;
}

RO_DEF void ro_posix_free_heap_buffer(ro_heap_buffer *buffer) 
{
    if(buffer && buffer->memory && buffer->total_bytes) 
    { munmap(buffer->memory, buffer->total_bytes); }
}

//NOTE: neither of these next couple are POSIX-specific

RO_DEF uint64_t ro_buffer_unallocated_bytes(ro_heap_buffer *buffer) 
{
    uint64_t result = 0;
    if(buffer && buffer->memory) 
    {
        result = ((uint64_t)buffer->memory + 
                buffer->total_bytes) - 
                (uint64_t)buffer->write_ptr;
    }
    return result;
}

RO_DEF void *ro_buffer_alloc_region(struct ro_heap_buffer *buffer, uint64_t region_bytes) 
{
    void *result = 0;
    uint64_t free_bytes = ro_buffer_unallocated_bytes(buffer);
    if(region_bytes <= free_bytes) 
    {
        result = buffer->write_ptr;
        buffer->write_ptr = (void *)((uintptr_t)buffer->write_ptr + region_bytes);
    } 
    return result;
}

//(pass negative value in place of bytes to decrement write_ptr)
RO_DEF void ro_buffer_move_writeptr(ro_heap_buffer *buffer, ssize_t bytes, char write_zeroes) 
{
    if(buffer && (((uintptr_t)buffer->write_ptr - bytes) >= (uintptr_t)buffer->memory)) 
    {
        buffer->write_ptr = (void *)((uintptr_t)buffer->write_ptr + bytes);
        if(write_zeroes) 
        { memset(buffer->write_ptr, 0, ro_abs_i64(bytes)); }
    }
}

#define RO_HEAP_BUFFER 1
#define RO_HEAPBUF_DOT_H 1
#endif

RO_DEF char *ro_posix_get_working_directory(char *destination, uint64_t buffer_size) 
{
    size_t bytes_read = readlink("/proc/self/exe", destination, buffer_size);
    destination[bytes_read] = 0x0;
    for(int char_index = (int)bytes_read; char_index >= 0; --char_index) 
    {
        if(destination[char_index] != '/') 
        { destination[char_index] = 0x0; }
        else 
        { break; }
    }
    return destination;
}

RO_DEF uint64_t ro_posix_get_timestamp(void) 
{
    uint64_t result;
    struct timespec tspec = {0};
    clock_gettime(CLOCK_MONOTONIC, &tspec);
    result = (tspec.tv_sec*1000000) + (tspec.tv_nsec/1000);
    return result;
}

RO_DEF void ro_posix_sleep_microsec(uint64_t microseconds) 
{
    uint64_t nanoseconds = microseconds*1000;
    struct timespec tspec = {0};
    tspec.tv_nsec = nanoseconds;
    nanosleep(&tspec, 0);
}

RO_DEF void ro_posix_sleep_sec(uint64_t seconds) 
{
    uint64_t nanoseconds = seconds*1000000000;
    struct timespec tspec = {0};
    tspec.tv_nsec = nanoseconds;
    nanosleep(&tspec, 0);
}

RO_DEF char ro_posix_file_exists(char *file_path) 
{
    char result = 0;
    struct stat stat_struct;
    if(!stat(file_path, &stat_struct) && !(S_ISDIR(stat_struct.st_mode))) 
    { result = 1; }
    return result;
}

RO_DEF char ro_posix_directory_exists(char *directory_name) 
{
    char result = 0;
    struct stat stat_struct;
    if(!stat(directory_name, &stat_struct) && S_ISDIR(stat_struct.st_mode)) 
    { result = 1; }
    return result;
}

RO_DEF int ro_posix_read_file(char *file_path, char *destination, uint64_t *dest_bytes) 
{
    int result = 0;
    int file_descriptor = open(file_path, O_RDONLY);
    if(file_descriptor != -1) 
    {
        struct stat stat_buf;
        fstat(file_descriptor, &stat_buf);
        *dest_bytes = read(file_descriptor, destination, stat_buf.st_size);
        close(file_descriptor);
        if(*dest_bytes == (uint64_t)stat_buf.st_size) 
        { result = 1; }
    }
    return result;
}

RO_DEF int ro_posix_write_file(char *file_path, void *in_buffer, uint64_t buffer_size) 
{
    int result = 0;
    int file_descriptor = open(file_path, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR);
    if(file_descriptor != -1) 
    {
        int64_t write_status = write(file_descriptor, in_buffer, buffer_size);  
        close(file_descriptor);
        if(write_status == (int64_t)buffer_size) 
        { result = 1; }
    }
    return result;
}

RO_DEF int ro_posix_get_stdout(char *command, 
                            int *output_fd, 
                            pid_t *proc_id, 
                            char include_stderr) 
{
    int result = 0;
    int pipe_fd[2];
    if(-1 == pipe(pipe_fd)) 
    {
        perror("pipe");
        _exit(1);
    }

    *proc_id = fork();
    if(-1 == *proc_id) 
    {
        perror("fork");
        _exit(1);
    } 
    else if(0 == *proc_id) 
    {
        close(pipe_fd[STDIN_FILENO]);
        dup2(pipe_fd[STDOUT_FILENO], STDOUT_FILENO);
        if(include_stderr) 
        { dup2(STDOUT_FILENO, STDERR_FILENO); }
        close(pipe_fd[STDOUT_FILENO]);
        char _temp[1024*8];
        snprintf(_temp, (1024*8) - 1, "''%s''", command);
        execl("/bin/sh", "sh", "-c", _temp, (char *)0);
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

#ifdef __cplusplus
}
#endif

#define RO_POSIX_DOT_H 1
#endif
