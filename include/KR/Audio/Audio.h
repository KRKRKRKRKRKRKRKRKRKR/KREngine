#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>

namespace KR
{
    // 読み込んだ音の番号札(MeshHandle と同じ形)。0 = 無効(まだ Load していない・読めなかった)
    struct SoundHandle
    {
        uint32_t id = 0;

        bool IsValid() const { return id != 0; }
    };

    // 鳴っている音 1 回分の番号札(XAudio2 の言葉で「声 = voice」)。同じ音を 3 回鳴らすと、声は 3 つ
    // 止めるときに使う。鳴り終わると、エンジンが自動で片付ける(札は、そのまま持っていても安全)
    struct VoiceHandle
    {
        uint32_t id = 0;

        bool IsValid() const { return id != 0; }
    };

    // 音を鳴らす窓口。Application が 1 つ持つ(ゲームは GetAudio() で使う)。XAudio2 の型は、ここに出さない
    //   OnInit    : m_Jump = GetAudio().Load("jump.wav");          // ゲームの Assets/Sounds/jump.wav
    //   OnUpdate  : GetAudio().Play(m_Jump);                       // 効果音(重ねて鳴らせる)
    //               m_Bgm = GetAudio().Play(m_Music, true, 0.5f);  // BGM(ループ・音量 0.5)
    //               GetAudio().Stop(m_Bgm);
    //   OnShutdown: GetAudio().Unload(m_Jump);                     // 忘れても、終了時にエンジンが片付ける
    // 音の出る機器が無い・準備に失敗したときは、警告を出して、音を出さずに動き続ける(ゲームは、分岐を書かなくてよい)
    class Audio
    {
    public:
        Audio();
        ~Audio();

        Audio(const Audio&) = delete;
        Audio& operator=(const Audio&) = delete;

        // 音のファイル(WAV・OGG・MP3)を、全部メモリに読む。relativePath は、ゲームの Assets/Sounds/ からの場所
        // 読めなければ、エラーをログに出して、無効の札を返す
        SoundHandle Load(const std::filesystem::path& relativePath);

        // 読んだ音を捨てる。その音で鳴っている声は、先に止める
        void Unload(SoundHandle sound);

        // 鳴らす。loop = true で、Stop するまで繰り返す。volume = 0(無音)〜 1(そのまま)
        // 鳴らせなければ(無効の札・音の機器が無い)、無効の札を返す
        VoiceHandle Play(SoundHandle sound, bool loop = false, float volume = 1.0f);

        // 止める。もう鳴り終わった札・無効の札でも、何もしないだけ
        void Stop(VoiceHandle voice);

        // 全体の音量(0 = 無音、1 = そのまま)。鳴っている音にも、すぐ効く
        void SetMasterVolume(float volume);
        float GetMasterVolume() const;

        // 鳴り終わった声を片付ける。エンジン(Application のメインループ)が毎フレーム呼ぶ。ゲームは呼ばなくてよい
        void Update();

    private:
        // XAudio2 の部品は、Audio.cpp の中だけで定義する(Windows・XAudio2 のヘッダーを、ゲームに見せない)
        struct Impl;
        std::unique_ptr<Impl> m_Impl;
    };
}
