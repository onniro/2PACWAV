
/*
File: 2pacwav_tagging.cpp
Date: Sun 23 Mar 2025 01:28:14 PM EET
*/

#include "tag.h"
#include "fileref.h"
#include "frames/attachedpictureframe.h"
#include "id3v2frame.h"
#include "id3v2header.h"
#include "id3v2tag.h"
#include "mpegfile.h"

#include "2pacwav.h"
#include "2pacwav_tagging.h"

PAC_INTERNAL char tag_open_file(char *path, Tag_Ref *ref)
{
    char result = 0;
    if(platform_file_exists(path))
    {
        TagLib::FileRef f(path);
        ref->ref = f;
        result = 1;
    }
    return(result);
}

//get

PAC_INTERNAL void tag_get_title(Tag_Ref *ref, char *recv, int recv_size)
{
    if(ref && recv && recv_size && !ref->ref.isNull())
    {
        TagLib::String _title = ref->ref.tag()->title().to8Bit(1);
        //NOTE: passing false for utf8 below here since i called this to8Bit thing
        //seemingly saying you want utf8 twice makes it break
        const char *title = _title.toCString(0);
        if(title)
        { strncpy(recv, title, recv_size); }
    }
}

PAC_INTERNAL void tag_get_artist(Tag_Ref *ref, char *recv, int recv_size)
{
    if(ref && recv && recv_size && !ref->ref.isNull()) 
    {
        TagLib::String _artist = ref->ref.tag()->artist().to8Bit(1);
        const char *artist = _artist.toCString(0);
        if(artist)
        { strncpy(recv, artist, recv_size); }
    }
}

PAC_INTERNAL void tag_get_album(Tag_Ref *ref, char *recv, int recv_size)
{
    if(ref && recv && recv_size && !ref->ref.isNull())
    {
        TagLib::String _album = ref->ref.tag()->album().to8Bit(1);
        const char *album = _album.toCString(0);
        if(album)
        { strncpy(recv, album, recv_size); }
    }
}

PAC_INTERNAL char tag_get_albumcover(Bitmap_Info *bmpinfo, char *path)
{
    char result = 0;
    TagLib::MPEG::File file(path);
    TagLib::ID3v2::Tag *id3tag = file.ID3v2Tag();
    TagLib::ID3v2::FrameList frames;
    TagLib::ID3v2::AttachedPictureFrame *pic_frame;

    if(id3tag) 
    {
        frames = id3tag->frameListMap()["APIC"];
        if(!frames.isEmpty()) 
        {
            for(TagLib::ID3v2::FrameList::ConstIterator iter = frames.begin(); 
                    iter != frames.end(); 
                    ++iter) 
            {
                pic_frame = (TagLib::ID3v2::AttachedPictureFrame *)(*iter);
                if(pic_frame->type() == TagLib::ID3v2::AttachedPictureFrame::FrontCover) 
                {
                    bmpinfo->img_data = (uint8_t *)pic_frame->picture().data();
                    bmpinfo->img_data_bytes = pic_frame->picture().size();
                    result = 1;
                    break;
                }
            }
        }
    }

    return result;
}

//set

PAC_INTERNAL void tag_set_title(Tag_Ref *ref, char *string)
{
    if(ref && string && !ref->ref.isNull() && ref->ref.tag())
    {
        TagLib::String s(string, TagLib::String::UTF8);
        ref->ref.tag()->setTitle(s);
        ref->ref.save();
    }
}

PAC_INTERNAL void tag_set_artist(Tag_Ref *ref, char *string)
{
    if(ref && string && !ref->ref.isNull() && ref->ref.tag())
    {
        TagLib::String s(string, TagLib::String::UTF8);
        ref->ref.tag()->setArtist(s);
        ref->ref.save();
    }
}

PAC_INTERNAL void tag_set_album(Tag_Ref *ref, char *string)
{
    if(ref && string && !ref->ref.isNull() && ref->ref.tag())
    {
        TagLib::String s(string, TagLib::String::UTF8);
        ref->ref.tag()->setAlbum(s);
        ref->ref.save();
    }
}

PAC_INTERNAL char tag_set_all(Tag_Ref *ref, Metadata_Editor *meta)
{
    char result = 0;
    if(ref && meta && !ref->ref.isNull() && ref->ref.tag())
    {
        tag_set_title(ref, meta->inbuf_title);
        tag_set_artist(ref, meta->inbuf_artist);
        tag_set_album(ref, meta->inbuf_album);
        result = 1;
    }
    return(result);
}
