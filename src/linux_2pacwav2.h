
/*
File: linux_2pacwav2.h
Date: Thu 24 Apr 2025 04:30:54 PM EEST
*/

#ifndef LINUX_2PACWAV_DOT_H

#include "ro_posix.h"

#ifdef __cplusplus
extern "C"
{
#endif

//(forward declarations)

PAC_INTERNAL void platform_log(char *fmt_string, ...);
PAC_INTERNAL void platform_dbg_log(char *fmt_string, ...);
PAC_INTERNAL char platform_file_exists(char *path);
PAC_INTERNAL char platform_directory_exists(char *path);
PAC_INTERNAL char platform_path_exists(char *path);
PAC_INTERNAL int platform_list_files_simple(char *path, File_List *out_flist, char sort);
PAC_INTERNAL int platform_list_files_mlist(char *path, File_List *out_flist);
PAC_INTERNAL uint64_t platform_read_file(char *file_path, char *dest, uint64_t dest_bytes);
PAC_INTERNAL int platform_write_file(char *file_path, char *dest, uint64_t *dest_bytes);

#ifdef __cplusplus
}
#endif

#define LINUX_2PACWAV_DOT_H 1
#endif
