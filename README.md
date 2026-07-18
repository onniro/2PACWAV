## 2PACWAV - music player for linux

the point of this program is to play music on the GNU/+Linux/systemd operating system.  
this project started because one day vlc media player crashed while i was trying to listen to some Tupac so i got real angry and took matters into my own hands

### contents:
[features](#features)  
[installation](#installation)  
[configuration file](#configuration-file)  
[credits and dependencies](#credits-and-dependencies)  
[2pacmixer.h](#2pacmixer)

### features <a name="features"></a>
- 2pacwav supports many different codecs and containers thanks to FFmpeg, such as WAV, MP3, AAC, Vorbis, Opus, FLAC, AIFF, WavPack, MusePack and more
- some metadata editing stuff
- cool oscilloscope audio visualizer
- basic music player things like playback controls, seeking and whatnot

### installation <a name="installation"></a>
0. (make sure a C++ (sorry) compiler is installed)
1. run `git clone --depth 1 https://github.com/onniro/2PACWAV.git && cd 2PACWAV && sh build_release.sh`
2. on success, the executable (named "2w") will be in build/linux_x64_release

### configuration file <a name="configuration-file"></a>
- the configuration file named "2wconf" should be placed either in `$HOME/.config/2pacwav` or in the same directory as the executable
- the file is parsed using a C parser, meaning that lines end on semicolons and comments are C & C++ style
- to get a full list of available options, see the "CONGFIGURATION" -section of `--longhelp`  

### credits and dependencies <a name="credits-and-dependencies"></a>
- SDL2 - abstraction for window and OpenGL context creation as well as audio output
- FFmpeg libs (avcodec, avformat, swresample et al) - decoding and resampling audio and handling metadata
- Dear ImGui - GUI library
- stb - image handling for displaying album covers

### 2pacmixer.h <a name="2pacmixer"></a>
- single header file that implements the entire audio mixer for 2pacwav (but can also be used in other projects)
- uses SDL2 for playback and ffmpeg for handling different codecs and formats and whatnot
- defining PACMXR_INCLUDE_ONLY_METADATA allows it to be used for only metadata purposes (2wmeta.c is an example program of such a use case)
