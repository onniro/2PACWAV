# Install script for directory: /home/onni/source/libs/taglib-2.0.2/taglib

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/home/onni/source/libs/taglib-2.0.2/taglib/libtag.a")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/taglib" TYPE FILE FILES
    "/home/onni/source/libs/taglib-2.0.2/taglib/tag.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/fileref.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/audioproperties.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/taglib_export.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/toolkit/taglib.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/toolkit/tstring.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/toolkit/tlist.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/toolkit/tlist.tcc"
    "/home/onni/source/libs/taglib-2.0.2/taglib/toolkit/tstringlist.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/toolkit/tbytevector.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/toolkit/tbytevectorlist.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/toolkit/tvariant.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/toolkit/tbytevectorstream.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/toolkit/tiostream.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/toolkit/tfile.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/toolkit/tfilestream.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/toolkit/tmap.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/toolkit/tmap.tcc"
    "/home/onni/source/libs/taglib-2.0.2/taglib/toolkit/tpicturetype.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/toolkit/tpropertymap.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/toolkit/tdebuglistener.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/toolkit/tversionnumber.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mpeg/mpegfile.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mpeg/mpegproperties.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mpeg/mpegheader.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mpeg/xingheader.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mpeg/id3v1/id3v1tag.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mpeg/id3v1/id3v1genres.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mpeg/id3v2/id3v2.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mpeg/id3v2/id3v2extendedheader.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mpeg/id3v2/id3v2frame.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mpeg/id3v2/id3v2header.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mpeg/id3v2/id3v2synchdata.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mpeg/id3v2/id3v2footer.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mpeg/id3v2/id3v2framefactory.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mpeg/id3v2/id3v2tag.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mpeg/id3v2/frames/attachedpictureframe.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mpeg/id3v2/frames/commentsframe.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mpeg/id3v2/frames/eventtimingcodesframe.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mpeg/id3v2/frames/generalencapsulatedobjectframe.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mpeg/id3v2/frames/ownershipframe.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mpeg/id3v2/frames/popularimeterframe.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mpeg/id3v2/frames/privateframe.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mpeg/id3v2/frames/relativevolumeframe.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mpeg/id3v2/frames/synchronizedlyricsframe.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mpeg/id3v2/frames/textidentificationframe.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mpeg/id3v2/frames/uniquefileidentifierframe.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mpeg/id3v2/frames/unknownframe.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mpeg/id3v2/frames/unsynchronizedlyricsframe.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mpeg/id3v2/frames/urllinkframe.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mpeg/id3v2/frames/chapterframe.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mpeg/id3v2/frames/tableofcontentsframe.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mpeg/id3v2/frames/podcastframe.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/ogg/oggfile.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/ogg/oggpage.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/ogg/oggpageheader.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/ogg/xiphcomment.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/ogg/vorbis/vorbisfile.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/ogg/vorbis/vorbisproperties.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/ogg/flac/oggflacfile.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/ogg/speex/speexfile.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/ogg/speex/speexproperties.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/ogg/opus/opusfile.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/ogg/opus/opusproperties.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/flac/flacfile.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/flac/flacpicture.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/flac/flacproperties.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/flac/flacmetadatablock.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/ape/apefile.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/ape/apeproperties.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/ape/apetag.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/ape/apefooter.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/ape/apeitem.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mpc/mpcfile.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mpc/mpcproperties.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/wavpack/wavpackfile.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/wavpack/wavpackproperties.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/trueaudio/trueaudiofile.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/trueaudio/trueaudioproperties.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/riff/rifffile.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/riff/aiff/aifffile.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/riff/aiff/aiffproperties.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/riff/wav/wavfile.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/riff/wav/wavproperties.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/riff/wav/infotag.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/asf/asffile.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/asf/asfproperties.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/asf/asftag.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/asf/asfattribute.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/asf/asfpicture.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mp4/mp4file.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mp4/mp4atom.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mp4/mp4tag.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mp4/mp4item.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mp4/mp4properties.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mp4/mp4coverart.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mp4/mp4itemfactory.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mod/modfilebase.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mod/modfile.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mod/modtag.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/mod/modproperties.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/it/itfile.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/it/itproperties.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/s3m/s3mfile.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/s3m/s3mproperties.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/xm/xmfile.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/xm/xmproperties.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/dsf/dsffile.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/dsf/dsfproperties.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/dsdiff/dsdifffile.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/dsdiff/dsdiffproperties.h"
    "/home/onni/source/libs/taglib-2.0.2/taglib/dsdiff/dsdiffdiintag.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/taglib/taglib-targets.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/taglib/taglib-targets.cmake"
         "/home/onni/source/libs/taglib-2.0.2/taglib/CMakeFiles/Export/398eef5e047a0959864f2888198961bf/taglib-targets.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/taglib/taglib-targets-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/taglib/taglib-targets.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/taglib" TYPE FILE FILES "/home/onni/source/libs/taglib-2.0.2/taglib/CMakeFiles/Export/398eef5e047a0959864f2888198961bf/taglib-targets.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/taglib" TYPE FILE FILES "/home/onni/source/libs/taglib-2.0.2/taglib/CMakeFiles/Export/398eef5e047a0959864f2888198961bf/taglib-targets-release.cmake")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/taglib" TYPE FILE FILES
    "/home/onni/source/libs/taglib-2.0.2/taglib-config.cmake"
    "/home/onni/source/libs/taglib-2.0.2/taglib-config-version.cmake"
    )
endif()

