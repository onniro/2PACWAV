
/*
File: ro_heapbuf.h
Date: Fri 28 Feb 2025 11:06:42 PM EET
*/

#ifndef RO_HEAPBUF_DOT_H

#define RO_HEAP_BUFFER 1

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct ro_heap_buffer 
{
    void *memory;
    void *write_ptr;
    uint64_t total_bytes;
} ro_heap_buffer;

#ifdef __cplusplus
}
#endif

#endif
