#pragma once

#include <array>
#include <cstdint>
#include <memory>

namespace TenFiftyEight
{
    // 前方宣言のみ。DX12 の型を Game 側に漏らさないため
    class Window;
    class DX12Device;
    class DX12CommandQueue;
    class DX12SwapChain;
    class DX12CommandList;

    // Game から使う描画の窓口。中身は DX12 だが、Game は DX12 を意識しない
    class Renderer
    {
    public:
        explicit Renderer(const Window& window);
        ~Renderer();

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        void SetClearColor(float r, float g, float b) { m_ClearColor = { r, g, b, 1.0f }; }
        void SetVSync(bool enabled) { m_VSync = enabled; }

    private:
        // フレームの開始・終了は Application だけが呼ぶ(Game からは呼べない)
        friend class Application;
        void BeginFrame();
        void EndFrame();
        void Resize(uint32_t width, uint32_t height);

        // CPU が先行して準備できるフレーム数
        static constexpr uint32_t FramesInFlight = 2;

        std::unique_ptr<DX12Device> m_Device;
        std::unique_ptr<DX12CommandQueue> m_Queue;
        std::unique_ptr<DX12SwapChain> m_SwapChain;
        std::unique_ptr<DX12CommandList> m_CommandList;

        // 各フレームの命令が GPU で終わったときに到達する Fence 値
        std::array<uint64_t, FramesInFlight> m_FrameFenceValues = {};
        uint32_t m_FrameIndex = 0;

        std::array<float, 4> m_ClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
        bool m_VSync = true;
    };
}
