
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

PAC_INTERNAL void load_font(struct nk_context *nuklear_ctx, char *working_dir);
PAC_INTERNAL void platform_log(char *fmt_string, ...);
PAC_INTERNAL void platform_dbg_log(char *fmt_string, ...);
PAC_INTERNAL char platform_file_exists(char *path);
PAC_INTERNAL char platform_directory_exists(char *path);
PAC_INTERNAL int platform_get_directory_listing_presorted(char *path, File_List *out_flist, Runtime_Vars *rtvars);
PAC_INTERNAL int platform_get_directory_listing(char *path, File_List *out_flist);
PAC_INTERNAL int platform_read_file(char *file_path, char *dest, uint64_t *dest_bytes);
PAC_INTERNAL int platform_write_file(char *file_path, char *dest, uint64_t *dest_bytes);

#ifdef __cplusplus
}
#endif

#define LINUX_2PACWAV_DOT_H 1
#endif
