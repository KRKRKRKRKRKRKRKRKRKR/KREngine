#include "KR/Audio/Audio.h"

#include "KR/Core/Assets.h"
#include "KR/Core/Log.h"
#include "Audio/SoundLoader.h"

#include <Windows.h>
#include <wrl/client.h>
#include <xaudio2.h>

#include <algorithm>
#include <unordered_map>

using Microsoft::WRL::ComPtr;

namespace KR
{
    namespace
    {
        // 鳴っている音 1 回分。XAudio2 の「ソースの声」と、どの音を鳴らしているか
        struct Voice
        {
            IXAudio2SourceVoice* source = nullptr; // COM ではない(Release ではなく DestroyVoice で片付ける)
            uint32_t soundId = 0;
        };

        void DestroyVoice(Voice& voice)
        {
            if (voice.source)
            {
                // DestroyVoice は、XAudio2 がこの声を使い終わるまで待ってから戻る。戻った後は、PCM の配列を捨てても安全
                voice.source->DestroyVoice();
                voice.source = nullptr;
            }
        }
    }

    // XAudio2 の部品と、番号札の表
    //   音の流れ:ソースの声(1 回鳴らすごとに 1 つ)→ マスターの声(全体の音量)→ スピーカー
    struct Audio::Impl
    {
        bool comInitialized = false;                  // CoInitializeEx が成功したか(成功したときだけ、最後に CoUninitialize)
        ComPtr<IXAudio2> xaudio;                      // XAudio2 本体(音を混ぜる計算を、別のスレッドで続けている)
        IXAudio2MasteringVoice* master = nullptr;     // マスターの声(スピーカーへの出口。全体の音量)
        float masterVolume = 1.0f;

        std::unordered_map<uint32_t, SoundData> sounds; // 読んだ音(札の番号 → PCM)
        uint32_t nextSoundId = 1;                       // 札の番号は使い回さない(古い札で、別の音を鳴らさないため)
        std::unordered_map<uint32_t, Voice> voices;     // 鳴っている声(札の番号 → 声)
        uint32_t nextVoiceId = 1;

        // マスターの声があれば、音を出せる
        bool IsReady() const { return master != nullptr; }
    };

    Audio::Audio()
        : m_Impl(std::make_unique<Impl>())
    {
        // XAudio2 は COM の仕組みを使うので、先に COM を準備する
        // STA(1 つのスレッドで使う形)にするのは、ツールのフォルダー選択の窓(FolderDialog)と、形をそろえるため
        // すでに別の形で準備済み(RPC_E_CHANGED_MODE)でも、XAudio2 は動くので、そのまま進む
        const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        m_Impl->comInitialized = SUCCEEDED(comResult); // S_FALSE(準備済み)も成功。回数を合わせて、最後に CoUninitialize する

        HRESULT result = XAudio2Create(m_Impl->xaudio.GetAddressOf(), 0, XAUDIO2_DEFAULT_PROCESSOR);
        if (FAILED(result))
        {
            KR_LOG_WARN("XAudio2 を準備できません(0x{:08X})。音を出さずに動きます", static_cast<uint32_t>(result));
            return;
        }

        // マスターの声 = スピーカーへの出口。音の出る機器が無いと、ここで失敗する
        result = m_Impl->xaudio->CreateMasteringVoice(&m_Impl->master);
        if (FAILED(result))
        {
            KR_LOG_WARN("音の出る機器が見つかりません(0x{:08X})。音を出さずに動きます", static_cast<uint32_t>(result));
            m_Impl->master = nullptr;
            m_Impl->xaudio.Reset();
            return;
        }

        XAUDIO2_VOICE_DETAILS details = {};
        m_Impl->master->GetVoiceDetails(&details);
        KR_LOG_INFO("XAudio2 初期化(出力 {} ch・{} Hz)", details.InputChannels, details.InputSampleRate);
    }

    Audio::~Audio()
    {
        // 片付けは、作った順の逆。声 → マスターの声 → XAudio2 本体 → COM
        for (auto& [id, voice] : m_Impl->voices)
        {
            DestroyVoice(voice);
        }
        m_Impl->voices.clear();
        if (m_Impl->master)
        {
            m_Impl->master->DestroyVoice();
            m_Impl->master = nullptr;
        }
        m_Impl->xaudio.Reset();
        m_Impl->sounds.clear(); // PCM は、声が全部消えてから捨てる

        if (m_Impl->comInitialized)
        {
            CoUninitialize();
        }
    }

    SoundHandle Audio::Load(const std::filesystem::path& relativePath)
    {
        // 音の機器が無くても、読み込みはする(札が有効なので、ゲーム側は、いつもと同じに書ける)
        std::optional<SoundData> sound = LoadSoundFile(GetAssetDirectory("Sounds") / relativePath);
        if (!sound)
        {
            return {}; // エラーは、LoadSoundFile が出している
        }
        const uint32_t id = m_Impl->nextSoundId++;
        m_Impl->sounds.emplace(id, std::move(*sound));
        return SoundHandle{ id };
    }

    void Audio::Unload(SoundHandle sound)
    {
        if (!m_Impl->sounds.contains(sound.id))
        {
            return;
        }
        // この音の PCM を読んでいる声を、先に止める(声は、PCM をコピーせずに、指しているだけのため)
        std::erase_if(m_Impl->voices, [&](auto& entry)
        {
            if (entry.second.soundId != sound.id)
            {
                return false;
            }
            DestroyVoice(entry.second);
            return true;
        });
        m_Impl->sounds.erase(sound.id);
    }

    VoiceHandle Audio::Play(SoundHandle sound, bool loop, float volume)
    {
        if (!sound.IsValid() || !m_Impl->IsReady())
        {
            return {};
        }
        const auto found = m_Impl->sounds.find(sound.id);
        if (found == m_Impl->sounds.end())
        {
            KR_LOG_WARN("Audio::Play: 音の札 {} がありません(Unload 済み?)", sound.id);
            return {};
        }
        const SoundData& data = found->second;

        // 声の形式:16 ビットの整数の PCM(WAV も OGG も、読み込みのときに、この形にそろえてある)
        // 周波数がスピーカーと違っても(44100 と 48000 など)、XAudio2 が自動で変換する
        WAVEFORMATEX format = {};
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.nChannels = static_cast<WORD>(data.channels);
        format.nSamplesPerSec = data.sampleRate;
        format.wBitsPerSample = 16;
        format.nBlockAlign = static_cast<WORD>(data.channels * sizeof(int16_t)); // 1 フレームのバイト数
        format.nAvgBytesPerSec = data.sampleRate * format.nBlockAlign;

        IXAudio2SourceVoice* source = nullptr;
        HRESULT result = m_Impl->xaudio->CreateSourceVoice(&source, &format);
        if (FAILED(result))
        {
            KR_LOG_WARN("Audio::Play: 声を作れません(0x{:08X})", static_cast<uint32_t>(result));
            return {};
        }

        // 鳴らす PCM を渡す。コピーせず、読み込んだ配列を指すだけ(重ねて鳴らしても、メモリが増えない)
        XAUDIO2_BUFFER buffer = {};
        buffer.AudioBytes = static_cast<UINT32>(data.samples.size() * sizeof(int16_t));
        buffer.pAudioData = reinterpret_cast<const BYTE*>(data.samples.data());
        buffer.Flags = XAUDIO2_END_OF_STREAM;                    // これで終わり(この後に続く PCM は無い)
        buffer.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;     // ループ = 最後まで行ったら、最初に戻る(Stop まで)
        result = source->SubmitSourceBuffer(&buffer);
        if (FAILED(result))
        {
            KR_LOG_WARN("Audio::Play: 音を渡せません(0x{:08X})", static_cast<uint32_t>(result));
            source->DestroyVoice();
            return {};
        }
        source->SetVolume(std::clamp(volume, 0.0f, 1.0f));
        source->Start();

        const uint32_t id = m_Impl->nextVoiceId++;
        m_Impl->voices.emplace(id, Voice{ source, sound.id });
        KR_LOG_TRACE("声 {} を鳴らします(音 {}・ループ {}・鳴っている声 {} 個)", id, sound.id, loop, m_Impl->voices.size());
        return VoiceHandle{ id };
    }

    void Audio::Stop(VoiceHandle voice)
    {
        const auto found = m_Impl->voices.find(voice.id);
        if (found == m_Impl->voices.end())
        {
            return; // 鳴り終わって片付け済み・無効の札
        }
        found->second.source->Stop();
        DestroyVoice(found->second);
        m_Impl->voices.erase(found);
        KR_LOG_TRACE("声 {} を止めました(鳴っている声 {} 個)", voice.id, m_Impl->voices.size());
    }

    void Audio::SetMasterVolume(float volume)
    {
        m_Impl->masterVolume = std::clamp(volume, 0.0f, 1.0f);
        if (m_Impl->master)
        {
            m_Impl->master->SetVolume(m_Impl->masterVolume);
        }
        KR_LOG_TRACE("全体の音量: {:.2f}", m_Impl->masterVolume);
    }

    float Audio::GetMasterVolume() const
    {
        return m_Impl->masterVolume;
    }

    void Audio::Update()
    {
        // 鳴り終わった声を片付ける。XAudio2 の「終わった」の知らせ(コールバック)は別のスレッドで届くため使わず、
        // 毎フレーム、メインのスレッドで「渡した PCM が、まだ残っているか」を見に行く(ループの声は、Stop まで残り続ける)
        std::erase_if(m_Impl->voices, [](auto& entry)
        {
            XAUDIO2_VOICE_STATE state = {};
            entry.second.source->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED); // 鳴らした数は要らない(軽い呼び方)
            if (state.BuffersQueued > 0)
            {
                return false;
            }
            KR_LOG_TRACE("声 {} が鳴り終わりました", entry.first);
            DestroyVoice(entry.second);
            return true;
        });
    }
}
