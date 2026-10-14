#include "Audio/SoundLoader.h"

#include "KR/Core/Log.h"

// 音を読むライブラリの「宣言」だけを読む(関数の中身は AudioCodecs.cpp に 1 回だけ入れる)
#include <dr_wav.h>
#define STB_VORBIS_HEADER_ONLY
#define STB_VORBIS_NO_STDIO // AudioCodecs.cpp と同じ(ファイルを開く関数は、使わない)
#include <stb_vorbis.c>

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

namespace KR
{
    namespace
    {
        // ログ用。path::string() は、日本語の Windows で CP932 になり、ログ(UTF-8)が文字化けするため
        std::string ToUtf8(const std::filesystem::path& path)
        {
            const std::u8string utf8 = path.u8string();
            return std::string(utf8.begin(), utf8.end());
        }

        // ファイルの中身を、全部メモリに読む
        // ライブラリにファイル名を渡さないのは、stb_vorbis がファイル名を char(CP932)で開くため。日本語のフォルダーでも読めるよう、開くのはこちらで行う
        std::optional<std::vector<uint8_t>> ReadAllBytes(const std::filesystem::path& path)
        {
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            if (!file)
            {
                return std::nullopt;
            }
            const std::streamsize size = file.tellg();
            std::vector<uint8_t> bytes(static_cast<size_t>(size));
            file.seekg(0);
            if (size > 0 && !file.read(reinterpret_cast<char*>(bytes.data()), size))
            {
                return std::nullopt;
            }
            return bytes;
        }

        bool StartsWith(const std::vector<uint8_t>& bytes, const char (&magic)[5])
        {
            return bytes.size() >= 4 && std::memcmp(bytes.data(), magic, 4) == 0;
        }

        // WAV(dr_wav)。どんな形式(8・24 ビット・小数など)でも、16 ビットの整数に直してもらう
        std::optional<SoundData> DecodeWav(const std::vector<uint8_t>& bytes)
        {
            unsigned int channels = 0;
            unsigned int sampleRate = 0;
            drwav_uint64 frameCount = 0;
            drwav_int16* pcm = drwav_open_memory_and_read_pcm_frames_s16(bytes.data(), bytes.size(), &channels, &sampleRate, &frameCount, nullptr);
            if (!pcm)
            {
                return std::nullopt;
            }
            SoundData sound;
            sound.channels = channels;
            sound.sampleRate = sampleRate;
            sound.samples.assign(pcm, pcm + frameCount * channels);
            drwav_free(pcm, nullptr); // ライブラリが確保したメモリは、ライブラリの関数で返す
            return sound;
        }

        // OGG(stb_vorbis)。圧縮をほどいて、16 ビットの整数にする
        std::optional<SoundData> DecodeOgg(const std::vector<uint8_t>& bytes)
        {
            int channels = 0;
            int sampleRate = 0;
            short* pcm = nullptr;
            const int frameCount = stb_vorbis_decode_memory(bytes.data(), static_cast<int>(bytes.size()), &channels, &sampleRate, &pcm);
            if (frameCount < 0 || !pcm)
            {
                return std::nullopt;
            }
            SoundData sound;
            sound.channels = static_cast<uint32_t>(channels);
            sound.sampleRate = static_cast<uint32_t>(sampleRate);
            sound.samples.assign(pcm, pcm + static_cast<size_t>(frameCount) * static_cast<size_t>(channels));
            std::free(pcm); // stb_vorbis は malloc で確保するので、free で返す
            return sound;
        }
    }

    std::optional<SoundData> LoadSoundFile(const std::filesystem::path& path)
    {
        const std::optional<std::vector<uint8_t>> bytes = ReadAllBytes(path);
        if (!bytes)
        {
            KR_LOG_ERROR("音のファイルを開けません: {}", ToUtf8(path));
            return std::nullopt;
        }

        std::optional<SoundData> sound;
        const char* format = "";
        if (StartsWith(*bytes, "RIFF"))
        {
            format = "WAV";
            sound = DecodeWav(*bytes);
        }
        else if (StartsWith(*bytes, "OggS"))
        {
            format = "OGG";
            sound = DecodeOgg(*bytes);
        }
        else
        {
            KR_LOG_ERROR("音のファイルの形式が分かりません(WAV・OGG に対応): {}", ToUtf8(path));
            return std::nullopt;
        }

        // XAudio2 は、1〜2 チャンネル以外・周波数 0 は扱えない(5.1ch などは、今は対応しない)
        if (!sound || sound->samples.empty() || sound->channels == 0 || sound->channels > 2 || sound->sampleRate == 0)
        {
            KR_LOG_ERROR("音のファイルを読めません({} として壊れている・空・3 チャンネル以上): {}", format, ToUtf8(path));
            return std::nullopt;
        }

        KR_LOG_INFO("音を読みました: {}({}・{} ch・{} Hz・{:.2f} 秒)",
            ToUtf8(path.filename()), format, sound->channels, sound->sampleRate, sound->GetSeconds());
        return sound;
    }
}
