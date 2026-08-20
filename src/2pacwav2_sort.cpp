
/*
File: 2pacwav2_sort.cpp
Date: Wed 19 Aug 2026 12:27:04 PM EEST

it was annoying to have all the sorting related stuff scattered across
the main file so i'm putting them here
*/

typedef int (*Sort_Comp_Func)(const void *a, const void *b);

//filename (not used anymore as of 19.8.26)
static int pac_qsort_filename_strcmp(const void *a, const void *b)
{
    Audio_File *afa = (Audio_File *)a, *afb = (Audio_File *)b;
    return strcasecmp((const char *)afa->filename,
                      (const char *)afb->filename);
}

static int pac_qsort_filename_strcmp_rev(const void *a, const void *b)
{
    Audio_File *afa = (Audio_File *)a, *afb = (Audio_File *)b;
    return strcasecmp((const char *)afb->filename,
                      (const char *)afa->filename);
}

//artist
static int pac_qsort_artist_strcmp(const void *a, const void *b)
{
    Audio_File *afa = (Audio_File *)a, *afb = (Audio_File *)b;
    const char *ca = (const char *)afa->metadata.artist;
    const char *cb = (const char *)afb->metadata.artist;
    return strcasecmp((ca) ? ca : "", (cb) ? cb : "");
}

static int pac_qsort_artist_strcmp_rev(const void *a, const void *b)
{
    Audio_File *afa = (Audio_File *)a, *afb = (Audio_File *)b;
    const char *ca = (const char *)afa->metadata.artist;
    const char *cb = (const char *)afb->metadata.artist;
    return strcasecmp((cb) ? cb : "", (ca) ? ca : "");
}

static void sort_list_by_artist(File_List *list, char reverse)
{
    Sort_Comp_Func cmpf = pac_qsort_artist_strcmp;
    if (reverse) { cmpf = pac_qsort_artist_strcmp_rev; }
    qsort(list->file_strings, list->entry_count, sizeof(Audio_File), cmpf);
}

//title
static int pac_qsort_title_strcmp(const void *a, const void *b)
{
    Audio_File *afa = (Audio_File *)a, *afb = (Audio_File *)b;
    const char *ca = (const char *)afa->metadata.title;
    const char *cb = (const char *)afb->metadata.title;
    return strcasecmp((ca) ? ca : (char *)afa->filename,
                      (cb) ? cb : (char *)afb->filename);
}

static int pac_qsort_title_strcmp_rev(const void *a, const void *b)
{
    Audio_File *afa = (Audio_File *)a, *afb = (Audio_File *)b;
    const char *ca = (const char *)afa->metadata.title;
    const char *cb = (const char *)afb->metadata.title;
    return strcasecmp((cb) ? cb : (char *)afb->filename,
                      (ca) ? ca : (char *)afa->filename);
}

static void sort_list_by_title(File_List *list, char reverse)
{
    Sort_Comp_Func cmpf = pac_qsort_title_strcmp;
    if (reverse) { cmpf = pac_qsort_title_strcmp_rev; }
    qsort(list->file_strings, list->entry_count, sizeof(Audio_File), cmpf);
}

//album
static int pac_qsort_album_strcmp(const void *a, const void *b)
{
    Audio_File *afa = (Audio_File *)a, *afb = (Audio_File *)b;
    const char *ca = (const char *)afa->metadata.album;
    const char *cb = (const char *)afb->metadata.album;
    return strcasecmp((ca) ? ca : "", (cb) ? cb : "");
}

static int pac_qsort_album_strcmp_rev(const void *a, const void *b)
{
    Audio_File *afa = (Audio_File *)a, *afb = (Audio_File *)b;
    const char *ca = (const char *)afa->metadata.album;
    const char *cb = (const char *)afb->metadata.album;
    return strcasecmp((cb) ? cb : "", (ca) ? ca : "");
}

static void sort_list_by_album(File_List *list, char reverse)
{
    Sort_Comp_Func cmpf = pac_qsort_album_strcmp;
    if (reverse) { cmpf = pac_qsort_album_strcmp_rev; }
    qsort(list->file_strings, list->entry_count, sizeof(Audio_File), cmpf);
}

//file modification date
static int pac_qsort_moddate(const void *a, const void *b)
{
    Audio_File *afa = (Audio_File *)a, *afb = (Audio_File *)b;
    return (afa->metadata.mod_time - afb->metadata.mod_time);
}

static int pac_qsort_moddate_rev(const void *a, const void *b)
{
    Audio_File *afa = (Audio_File *)a, *afb = (Audio_File *)b;
    return (afb->metadata.mod_time - afa->metadata.mod_time);
}

static void sort_list_by_mod_date(File_List *list, char reverse)
{
    platform_get_file_mod_dates(list);
    Sort_Comp_Func cmpf = pac_qsort_moddate;
    if (reverse) { cmpf = pac_qsort_moddate_rev; }
    qsort(list->file_strings, list->entry_count, sizeof(Audio_File), cmpf);
}

#if 0
static void sort_file_list_alpha(File_List *flist, char reverse)
{
    Audio_File *strings = flist->file_strings;
    int sort_count = flist->entry_count;
    Sort_Comp_Func cmpf = pac_qsort_filename_strcmp;
    if (reverse) { cmpf = pac_qsort_filename_strcmp_rev; }
#if 0
    qsort(strings, sort_count, sizeof(char *), cmpf);
#else
    qsort(strings, sort_count, sizeof(Audio_File), cmpf);
#endif
}
#endif
