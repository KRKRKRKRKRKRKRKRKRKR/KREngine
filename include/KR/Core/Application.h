#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace KR
{
    // 前方宣言のみ。Window.h(= Windows.h)を Game 側に漏らさないため
    class Window;
    class Renderer;
    class Audio;
    class EditorLauncher;

    struct ApplicationDesc
    {
        std::string title = "KR Engine";
        uint32_t width = 1280;
        uint32_t height = 720;

        // Debug で F1 を押すと、敵の行動のツール(KRBehaviorEditor)を開く。ツール自身は false にする(自分を開かないように)
        bool enableEditorKey = true;

        // F1 でツールを開いたときに、ゲームを止めるか(初期値。開くたびに、この値に戻る)。右上の小さな窓の ☑ で、切り替えられる
        // true = 止める(直してから再開して確かめる)/ false = 動かしたまま(リアルタイム編集:直すと、約 0.2〜0.7 秒で、動いている敵に反映)
        bool pauseGameWhileEditing = true;
    };

    class Application
    {
    public:
        explicit Application(const ApplicationDesc& desc = {});
        virtual ~Application();

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        void Run();

    protected:
        virtual void OnInit() {}
        virtual void OnUpdate(float deltaTime) { (void)deltaTime; }
        virtual void OnRender() {}

        // ImGui の窓を書く場所(ImGui:: の関数を呼ぶ)。OnRender の後に呼ばれるので、ゲームの絵より手前に乗る
        virtual void OnImGui() {}
        virtual void OnShutdown() {}

        Renderer& GetRenderer() { return *m_Renderer; }

        // 音を鳴らす窓口(使い方は KR/Audio/Audio.h)
        Audio& GetAudio() { return *m_Audio; }

        // ゲームを終わらせる依頼(タイトルの「終了」など)。窓の × を押したときと同じ道筋で終わる:
        // このフレームは最後まで進み(OnUpdate → OnRender → OnImGui)、次のフレームの前にループが終わって、OnShutdown が呼ばれる
        void RequestQuit();

    private:
        void OnWindowResize(uint32_t width, uint32_t height);
        void OnKeyDown(uint32_t virtualKey);

        // Debug の F1:ツールを開く・ツールが開いている間の小さな窓(止める / 動かす)
        void OpenBehaviorEditor();
        void DrawEditorPanel();

        // 破棄は宣言の逆順。Renderer(SwapChain)がウィンドウより先に破棄されるよう、この順番にする
        std::unique_ptr<Window> m_Window;
        std::unique_ptr<Renderer> m_Renderer;
        // 音は、ウィンドウにも Renderer にも頼らない(どの順番で作っても・捨ててもよい)。最後に作り、最初に捨てる
        std::unique_ptr<Audio> m_Audio;
        bool m_Minimized = false;

        // ツール(別の exe)を開く係。Debug で、enableEditorKey が true のときだけ作る
        std::unique_ptr<EditorLauncher> m_EditorLauncher;
        bool m_PauseWhileEditing = true; // ツールが開いている間、ゲームを止めるか(小さな窓で切り替える)
        bool m_PauseDefault = true;      // 開くたびに戻す値(ApplicationDesc::pauseGameWhileEditing)
        bool m_EditorWasOpen = false;    // 前のフレームで、ツールが開いていたか(閉じたことに気づくため)
    };

    // Game 側で実装する。EntryPoint.cpp の main() から呼ばれる
    std::unique_ptr<Application> CreateApplication();
}
