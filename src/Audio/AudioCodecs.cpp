// 音を読むライブラリ(dr_wav・dr_mp3・stb_vorbis)の本体(関数の中身)を、このファイルに、1 回だけ取り込む
// ほかのファイルでは、宣言だけを読む(SoundLoader.cpp)。他人のコードなので、このファイルだけ警告を出さない(CMakeLists.txt で /W0)

#define DR_WAV_IMPLEMENTATION
#include <dr_wav.h>

#define DR_MP3_IMPLEMENTATION
#include <dr_mp3.h>

// stb_vorbis にファイルを開かせない(開くのは SoundLoader。日本語のフォルダーのため)
#define STB_VORBIS_NO_STDIO
#include <stb_vorbis.c>
