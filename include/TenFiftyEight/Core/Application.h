#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace TenFiftyEight
{
    // 前方宣言のみ。Window.h(= Windows.h)を Game 側に漏らさないため
    class Window;
    class Renderer;

    struct ApplicationDesc
    {
        std::string title = "TenFiftyEight";
        uint32_t width = 1280;
        uint32_t height = 720;
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

    private:
        void OnWindowResize(uint32_t width, uint32_t height);
        void OnKeyDown(uint32_t virtualKey);

        // 破棄は宣言の逆順。Renderer(SwapChain)がウィンドウより先に破棄されるよう、この順番にする
        std::unique_ptr<Window> m_Window;
        std::unique_ptr<Renderer> m_Renderer;
        bool m_Minimized = false;
    };

    // Game 側で実装する。EntryPoint.cpp の main() から呼ばれる
    std::unique_ptr<Application> CreateApplication();
}
