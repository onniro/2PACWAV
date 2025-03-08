
/*
file:       ro_winapi.h
creation:   30 Jan 2025

WARNING: THIS FILE HAS UNTESTED AND POSSIBLY NON-COMPILING CODE
TODO: port standard output reading function(s) and shit
*/

#ifndef RO_WINAPI_DOT_H

#ifdef __cplusplus
extern "C"
{
#endif

//#include <windows.h> 
#include <stdint.h>
#include <winbase.h>
#include <memoryapi.h>
#include <libloaderapi.h>
#include <shlwapi.h>
#include <fileapi.h>

#ifndef RO_DEF
    #define RO_DEF static inline
#endif

#ifndef RO_HEAPBUF
#define RO_HEAPBUF 1
#define RO_HEAPBUF_WINAPI 1

typedef struct ro_heap_buffer 
{
    void *memory;
    void *write_ptr;
    uint64_t total_bytes;
} ro_heap_buffer;

RO_DEF void *ro_winapi_make_heap_buffer(ro_heap_buffer *buffer, uint64_t bytes) 
{
    buffer->memory = VirtualAlloc(0, bytes, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    buffer->write_ptr = buffer->memory;
    buffer->total_bytes = bytes;
    return target->memory;
}
//TODO: free (VirtualFree())

#ifndef RO_HEAPBUF_PLATFORM_AGNOSTIC
#define RO_HEAPBUF_PLATFORM_AGNOSTIC 1

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

RO_DEF void *ro_alloc_buffer_region(struct ro_heap_buffer *buffer, uint64_t region_bytes) 
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

RO_DEF void ro_heap_buffer_move_writeptr(ro_heap_buffer *buffer, 
                                        ssize_t bytes, 
                                        char write_zeroes) 
{
    if(buffer && (((uintptr_t)buffer->write_ptr - bytes) >= (uintptr_t)buffer->memory)) 
    {
        buffer->write_ptr = (void *)((uintptr_t)buffer->write_ptr + bytes);
        if(write_zeroes) 
        { memset(buffer->write_ptr, 0, abs(bytes)); }
    }
}

#endif //RO_HEAPBUF_PLATFORM_AGNOSTIC
#endif //RO_HEAPBUF

RO_DEF char *ro_winapi_get_working_directory(char *destination, DWORD buffer_size) 
{
    char *result = 0;
    DWORD path_length = GetModuleFileNameA(0, destination, buffer_size);
    if(path_length) 
    {
        result = destination;
        for(DWORD char_index = path_length - 1;
                destination[char_index] != '\\';
                --char_index) 
        { destination[char_index] = '\0'; }
    }
    return result;
}

RO_DEF int ro_winapi_file_exists(char *file_path) 
{
    int result = 0;
    if(PathFileExistsA(file_path)) {result = 1;}
    return result;
}

RO_DEF int ro_winapi_directory_exists(char *directory_name) 
{
    int result = 0;
    if(PathIsDirectoryA(directory_name)) {result = 1;}
    return result;
}

RO_DEF int ro_winapi_read_file(char *file_path, char *destination, uint64_t *dest_size) 
{
    int result = false;
    HANDLE file_handle = CreateFileA(file_path, 
                            GENERIC_READ, 
                            FILE_SHARE_READ, 
                            0, 
                            OPEN_EXISTING,
                            0, 
                            0);
    if(file_handle != INVALID_HANDLE_VALUE) 
    {
        LARGE_INTEGER file_size;
        if(GetFileSizeEx(file_handle, &file_size)) 
        {
            *dest_size = file_size.QuadPart;
            DWORD bytes_read;
            if(ReadFile(file_handle, destination, *dest_size, &bytes_read, 0)) 
            { result = true; }
        } 
    }
    return result;
}

RO_DEF int ro_winapi_write_file(char *file_path, void *in_buffer, uint64_t buffer_size) 
{
    int result = false;
    HANDLE file_handle = CreateFileA(file_path, 
                            GENERIC_WRITE,
                            FILE_SHARE_WRITE, 
                            0, 
                            CREATE_ALWAYS, 
                            0,
                            0);
    if(file_handle != INVALID_HANDLE_VALUE) 
    {
        DWORD bytes_written;
        if(WriteFile(file_handle, in_buffer, buffer_size, &bytes_written, 0)) 
        { result = true; }
    }
    CloseHandle(file_handle);
    return result;
}

#ifdef __cplusplus
}
#endif

#define RO_WINAPI_DOT_H 1
#endif 
