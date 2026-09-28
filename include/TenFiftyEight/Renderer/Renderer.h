#pragma once

#include "TenFiftyEight/Renderer/Camera.h"

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
    class DX12ShaderCompiler;
    class DX12Pipeline;
    class DX12UploadBuffer;
    class DX12DepthBuffer;

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

        // このフレームで使うカメラ(コピーして保持する)。呼ばなければ Camera の初期値で描く
        void SetCamera(const Camera& camera) { m_Camera = camera; }

        // 2-4 の動作確認用(ラジアン)。2-7 で DrawMesh に置き換える
        void SetTestCubeAngle(float angle) { m_TestCubeAngle = angle; }

    private:
        // フレームの開始・終了は Application だけが呼ぶ(Game からは呼べない)
        friend class Application;
        void BeginFrame();
        void EndFrame();
        void Resize(uint32_t width, uint32_t height);
        void ReloadShaders();

        // シェーダーをコンパイルしてパイプラインを作り直す。失敗したら false(前のパイプラインのまま)
        bool LoadShaders();

        // 2-4 の動作確認用。2-7 で DrawMesh に置き換える
        void DrawTestCube();

        // CPU が先行して準備できるフレーム数
        static constexpr uint32_t FramesInFlight = 2;

        std::unique_ptr<DX12Device> m_Device;
        std::unique_ptr<DX12CommandQueue> m_Queue;
        std::unique_ptr<DX12SwapChain> m_SwapChain;
        std::unique_ptr<DX12DepthBuffer> m_DepthBuffer;
        std::unique_ptr<DX12CommandList> m_CommandList;
        std::unique_ptr<DX12ShaderCompiler> m_ShaderCompiler;
        std::unique_ptr<DX12Pipeline> m_Pipeline;
        std::unique_ptr<DX12UploadBuffer> m_TestVertexBuffer;
        std::unique_ptr<DX12UploadBuffer> m_TestIndexBuffer;
        float m_TestCubeAngle = 0.0f;

        // カメラの行列を入れる定数バッファ(別紙)。GPU が読んでいる途中で書き換えないよう、フレームインフライトの数だけ持つ
        std::array<std::unique_ptr<DX12UploadBuffer>, FramesInFlight> m_FrameConstantBuffers;
        Camera m_Camera;

        // 各フレームの命令が GPU で終わったときに到達する Fence 値
        std::array<uint64_t, FramesInFlight> m_FrameFenceValues = {};
        uint32_t m_FrameIndex = 0;

        std::array<float, 4> m_ClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
        bool m_VSync = true;
    };
}
