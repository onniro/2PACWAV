
/*
File: linux_2pacwav2.h
Date: Thu 24 Apr 2025 04:30:54 PM EEST
*/

#ifndef LINUX_2PACWAV_DOT_H

#ifndef RO_UTIL_POSIX
    #define RO_UTIL_POSIX 1
#endif
#include "ro_util.h"

#ifdef __cplusplus
extern "C"
{
#endif

//(forward declarations)

static uint64_t platform_get_timestamp(void);
static void platform_log(char *fmt_string, ...);
static void platform_dbg_log(char *fmt_string, ...);
static char platform_file_exists(char *path);
static char platform_directory_exists(char *path);
static void platform_get_file_mod_dates(File_List *flist);
static void platform_get_font_path(Runtime_Vars *rtvars, char *dest, int dest_size);
static void platform_sleep_ms(int ms);
static char platform_path_exists(char *path);
static void *metadata_bulk_getter_thread_entry(void *arg_ptr);
static void platform_get_metadata_bulk(Runtime_Vars *rtvars, char *);
static int platform_list_files_simple(char *path, File_List *out_flist, char sort);
static int platform_list_files_mlist(char *path, File_List *out_flist);
static uint64_t platform_read_file(char *file_path, char *dest, uint64_t dest_bytes);
static int platform_write_file(char *file_path, char *dest, uint64_t *dest_bytes);
static void platform_dbg_dump_file(char *containing_dir, void *buffer, size_t buffer_size);
static void platform_sort_mlist_mod_date_janky(File_List *, char, Runtime_Vars *);

#ifdef __cplusplus
}
#endif

#define LINUX_2PACWAV_DOT_H 1
#endif
