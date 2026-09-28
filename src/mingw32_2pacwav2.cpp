
/*
File: mingw32_2pacwav2.cpp
Date: Tue 22 Sep 2026 02:19:15 PM EEST

Some mingw32 specific code
*/

static int platform_list_files_mlist(char *path, File_List *out_flist)
{
    int result = 0;
    if (!path[0]) { return result; }
    wchar_t pattern[PATH_MAX];
    int path_len = MultiByteToWideChar(CP_UTF8,
                        MB_ERR_INVALID_CHARS,
                        path,
                        -1,
                        pattern,
                        PATH_MAX);
    if (pattern[path_len - 2] != L'*')
    { wcsncat(pattern, L"\\*", PATH_MAX - 1); }

    WIN32_FIND_DATAW ffd;
    HANDLE handle = FindFirstFileW(pattern, &ffd);

    if (handle != INVALID_HANDLE_VALUE)
    {
        file_list_push_dirname(path, out_flist);
        uint16_t dir_hash = hash_fnv1a16((uint8_t *)path, strlen(path));

        int filename_len;
        char *write_ptr;
        Audio_File *loclist = out_flist->file_strings, *file;
        while (1)
        {
            if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                file = &loclist[out_flist->entry_count];
                write_ptr = file->filename;
                file->containing_dir = out_flist->dirnames_string_loclist[out_flist->dirs_added - 1];
                file->containing_dir_hash = dir_hash;
                filename_len = WideCharToMultiByte(CP_UTF8,
                                    0, ffd.cFileName,
                                    -1, write_ptr,
                                    NAME_MAX, 0, 0);
                write_ptr[filename_len] = 0x0;
                loclist[out_flist->entry_count + 1].filename = write_ptr + filename_len;
                ++out_flist->entry_count;
            }

            if (!FindNextFileW(handle, &ffd)) { break; }
        }
        result = 1;
        FindClose(handle);
    }
    else
    {
        platform_dbg_log("directory listing failed. reason: failed to initialize directory struct\n"); 
    }

    return result;
}

static char *platform_get_working_directory(char *destination, DWORD buffer_size)
{
    return ro_w32_get_working_directory(destination, buffer_size);
}

static char platform_file_exists(char *path)
{
    return ro_w32_file_exists(path);
}

static char platform_directory_exists(char *path)
{
    return ro_w32_directory_exists(path);
}

static char platform_path_exists(char *path)
{
    return ro_w32_path_exists(path);
}
