#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace KR
{
    // 読み込んだ音の中身(エンジン内部専用)。WAV も OGG も、ここでは同じ形になる
    // PCM = 音の波を、決まった間隔(1 秒に sampleRate 回)で測った数の並び。圧縮していない、そのままの音
    struct SoundData
    {
        uint32_t channels = 0;          // 1 = モノラル、2 = ステレオ
        uint32_t sampleRate = 0;        // 1 秒あたりの、測った回数(44100・48000 など)
        std::vector<int16_t> samples;   // 16 ビットの整数の PCM。ステレオは「左, 右, 左, 右, …」の順に並ぶ

        // 1 フレーム = 同じ瞬間の、全部のチャンネルの値(ステレオなら 2 つ)
        uint64_t GetFrameCount() const { return channels == 0 ? 0 : samples.size() / channels; }
        float GetSeconds() const { return sampleRate == 0 ? 0.0f : static_cast<float>(GetFrameCount()) / static_cast<float>(sampleRate); }
    };

    // 音のファイル(WAV・OGG・MP3)を読んで、16 ビットの PCM にほどく。XAudio2 は知らない(データと計算だけ)
    // 形式は、ファイルの先頭の印("RIFF" = WAV、"OggS" = OGG、"ID3" かフレームの印 = MP3)で決める(拡張子が違っていても読める)
    // 読めなければ、エラーをログに出して、空(nullopt)を返す
    std::optional<SoundData> LoadSoundFile(const std::filesystem::path& path);
}
