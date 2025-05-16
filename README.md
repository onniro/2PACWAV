## 2PACWAV - music player for linux

the point of this program is to play music on the GNU/+Linux/systemd operating system. this project started because one day vlc media player crashed while i was trying to listen to some Tupac so i got real angry and took matters into my own hands

### contents:
[supported formats](#supported-formats)  
[notable unsupported formats](#notable-unsupported-formats)  
[installation](#installation)  
[credits/dependencies](#credits-dependencies)

### supported formats:
- raw PCM (e.g. WAV and AIFF)
- AAC (only in MP3 container)
- Vorbis (only in Ogg container)
- Opus (also only in Ogg container)
- FLAC (only in FLAC container)
- WavPack

### notable unsupported formats:
- anything in a Matroska container
- anything QuickTime related
- ALAC
- WMA (lol)
- Musepack

note: the above things about supported/unsupported formats is subject to change as the current plan is to stop using SDL mixer

### installation:
0. (make sure g++ is installed)
1. run `git clone --depth 1 https://github.com/onniro/2PACWAV.git && cd 2PACWAV && sh build_release.sh`
2. on success, the executable (named "2w") will be in build/linux_x64_release

### credits/dependencies:
- SDL2 - abstraction for window and OpenGL context creation as well as actually playing music (from loading files to audio output)
- Dear ImGui - GUI library
- TagLib - metadata library
- stb - completely unused for now but its good