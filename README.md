# winnipeg

C++ experiments on using FFMPEG. Mostly a video editor using C++ scripting.

Requires FFMPEG include and libraries. These can't be embedded here due to GPL
being incompatible with MIT license.

After cloning this repo, you need to download a binary bundle with FFMPEG
headers, then copy/symlink the target include/lib dirs to `ffmpeg/include` and
`ffmpeg/lib`.
