
/*
File: linux_2pacwav.h
Date: Tue 25 Feb 2025 09:52:47 PM EET
*/

#ifndef LINUX_2PACWAV_DOT_H

#include "ro_posix.h"

#ifdef __cplusplus
extern "C"
{
#endif

//(forward declarations)

void load_font(struct nk_context *nuklear_ctx, char *working_dir);
void platform_log(char *fmt_string, ...);
void platform_dbg_log(char *fmt_string, ...);
char platform_file_exists(char *path);
char platform_directory_exists(char *path);
int platform_get_directory_listing_presorted(char *path, file_list *out_flist, runtime_vars *rtvars);
int platform_get_directory_listing(char *path, file_list *out_flist);
int platform_read_file(char *file_path, char *dest, uint64_t *dest_bytes);
int platform_write_file(char *file_path, char *dest, uint64_t *dest_bytes);

#ifdef __cplusplus
}
#endif

#define LINUX_2PACWAV_DOT_H 1
#endif
