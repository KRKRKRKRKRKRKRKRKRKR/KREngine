// 音(KR::Audio)の、使い方の見本
//   Space   : 効果音(Beep.wav)を鳴らす。連打すると、重ねて鳴る
//   B       : BGM(Jingle.ogg)をループで鳴らす / 止める
//   上 / 下 : 全体の音量を上げる / 下げる(ImGui のスライダーでも変えられる)

#include <KR/KR.h>

#include <imgui.h>

#include <algorithm>
#include <memory>

class AudioApp : public KR::Application
{
public:
    AudioApp() : Application(KR::ApplicationDesc{ .title = "KR Engine - Audio" }) {}

protected:
    void OnInit() override
    {
        // 音を読む(ゲームの Assets/Sounds/ からの場所)。読めなければ、無効の札(Play しても、何も起きない)
        m_Beep = GetAudio().Load("Beep.wav");
        m_Music = GetAudio().Load("Jingle.ogg");
    }

    void OnUpdate(float deltaTime) override
    {
        (void)deltaTime;
        KR::Audio& audio = GetAudio();

        // 効果音:鳴らしっぱなしでよい(鳴り終わると、エンジンが片付ける)
        if (KR::Input::IsKeyPressed(KR::Key::Space))
        {
            audio.Play(m_Beep);
        }

        // BGM:止めるために、鳴っている声の札を持っておく
        if (KR::Input::IsKeyPressed(KR::Key::B))
        {
            if (m_BgmVoice.IsValid())
            {
                audio.Stop(m_BgmVoice);
                m_BgmVoice = {};
            }
            else
            {
                m_BgmVoice = audio.Play(m_Music, true, 0.6f);
            }
        }

        // 全体の音量(0 〜 1)
        if (KR::Input::IsKeyPressed(KR::Key::Up))
        {
            audio.SetMasterVolume(std::min(audio.GetMasterVolume() + 0.1f, 1.0f));
        }
        if (KR::Input::IsKeyPressed(KR::Key::Down))
        {
            audio.SetMasterVolume(std::max(audio.GetMasterVolume() - 0.1f, 0.0f));
        }
    }

    void OnImGui() override
    {
        KR::Audio& audio = GetAudio();
        ImGui::Begin("Audio");
        ImGui::Text("Space: Beep / B: BGM / Up Down: Volume");
        ImGui::Text("Beep: %s  Music: %s", m_Beep.IsValid() ? "OK" : "-", m_Music.IsValid() ? "OK" : "-");
        ImGui::Text("BGM: %s", m_BgmVoice.IsValid() ? "Playing" : "Stopped");
        float volume = audio.GetMasterVolume();
        if (ImGui::SliderFloat("Master", &volume, 0.0f, 1.0f))
        {
            audio.SetMasterVolume(volume);
        }
        ImGui::End();
    }

    void OnShutdown() override
    {
        // 読んだ音を捨てる(鳴っている声も、一緒に止まる)
        GetAudio().Unload(m_Beep);
        GetAudio().Unload(m_Music);
    }

private:
    KR::SoundHandle m_Beep;     // 効果音
    KR::SoundHandle m_Music;    // BGM
    KR::VoiceHandle m_BgmVoice; // 鳴っている BGM(止めるのに使う)
};

std::unique_ptr<KR::Application> KR::CreateApplication()
{
    return std::make_unique<AudioApp>();
}
