
/*
File: 2pacwav2_tagging.h
Date: Thu 24 Apr 2025 05:05:51 PM EEST
NOTE: This file is completely unused as of June 15 2025 and
taglib is no longer included anywhere in this project
*/

#ifndef _2PACWAV_TAGGING_DOT_H

typedef struct Tag_Ref
{
    TagLib::FileRef ref;
} Tag_Ref;

PAC_INTERNAL char tag_open_file(char *path, Tag_Ref *ref);
PAC_INTERNAL void tag_get_title(Tag_Ref *ref, char *recv, int recv_size);
PAC_INTERNAL void tag_get_artist(Tag_Ref *ref, char *recv, int recv_size);
PAC_INTERNAL void tag_get_album(Tag_Ref *ref, char *recv, int recv_size);
PAC_INTERNAL char tag_get_albumcover(Bitmap_Info *bmpinfo, char *path);
PAC_INTERNAL void tag_set_title(Tag_Ref *ref, char *string);
PAC_INTERNAL void tag_set_artist(Tag_Ref *ref, char *string);
PAC_INTERNAL void tag_set_album(Tag_Ref *ref, char *string);
PAC_INTERNAL char tag_set_all(Tag_Ref *ref, Metadata_Editor *meta);

#define _2PACWAV_TAGGING_DOT_H
#endif
