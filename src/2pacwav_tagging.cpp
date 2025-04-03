
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

char taglib_get_albumcover(bitmap_info *bmpinfo, char *path)
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

