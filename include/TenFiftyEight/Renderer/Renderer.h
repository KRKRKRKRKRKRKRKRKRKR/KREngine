#pragma once

#include "TenFiftyEight/Renderer/Camera.h"
#include "TenFiftyEight/Renderer/Lighting.h"
#include "TenFiftyEight/Renderer/Mesh.h"
#include "TenFiftyEight/Renderer/Texture.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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
    class DX12DescriptorHeap;
    class DX12Texture;
    class ImGuiLayer;

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

        // このフレームで使う光(コピーして保持する)。呼ばなければ Lighting の初期値で描く
        void SetLighting(const Lighting& lighting) { m_Lighting = lighting; }

        // このフレームで使うフォグ(コピーして保持する)。霧の色は、空の地平線の色(SetSky)に自動で合わせる
        void SetFog(const Fog& fog) { m_Fog = fog; }

        // このフレームで使う空(コピーして保持する)。画面全体に、地平線から天頂へのグラデーションを、最初に描く
        void SetSky(const Sky& sky) { m_Sky = sky; }

        // 形のデータを GPU のバッファに入れ、番号札を返す(失敗したら無効な番号札)
        MeshHandle CreateMesh(const MeshData& data);

        // 番号札をすぐに無効にし、GPU が使い終わってから実際に捨てる(遅延削除)
        void DestroyMesh(MeshHandle mesh);

        // 画像ファイル(PNG など)を読んで、GPU のメモリに置き、番号札を返す(失敗したら無効な番号札)
        // コピーが終わるまで待つので、起動時の読み込み用(走りながら作ると、止まる)
        TextureHandle CreateTexture(const std::filesystem::path& file);

        // 番号札をすぐに無効にし、GPU が使い終わってから実際に捨てる(遅延削除。CreateMesh / DestroyMesh と同じ)
        void DestroyTexture(TextureHandle texture);

        // 「この形を、この行列で描いて」という依頼を描画リストに積む(OnRender で呼ぶ)
        // 実際の描画命令は EndFrame でまとめて作る
        void DrawMesh(MeshHandle mesh, const DirectX::XMMATRIX& world);

    private:
        // フレームの開始・終了は Application だけが呼ぶ(Game からは呼べない)
        friend class Application;
        void BeginFrame();
        void EndFrame();
        void Resize(uint32_t width, uint32_t height);
        void ReloadShaders();

        // OS のメッセージを ImGui に渡す。処理されたら true
        // (Windows.h の型を、この公開ヘッダーに出さないため、型は素の整数・ポインタにしてある)
        bool HandleWindowMessage(void* window, unsigned int message, uintptr_t wParam, intptr_t lParam);

        // ImGui が今、キーボードを使っているか(使っている間は、ゲームにキーを渡さない)
        bool ImGuiWantsKeyboard() const;

        // シェーダーをコンパイルしてパイプラインを作り直す。失敗したら false(前のパイプラインのまま)
        bool LoadShaders();

        // 描画リストの依頼を1つずつ GPU への命令にして、リストを空にする
        void ExecuteDrawList();

        // RGBA8 のピクセルから、テクスチャを作る(CreateTexture と、白の 1×1 が使う)
        TextureHandle CreateTextureFromPixels(uint32_t width, uint32_t height, const uint8_t* rgbaPixels);

        // 捨てる予定リストのうち、GPU が使い終わった物を本当に捨てる
        void ReleaseFinishedMeshes();

        // CPU が先行して準備できるフレーム数
        static constexpr uint32_t FramesInFlight = 2;

        // 形の実体(頂点バッファ + 番号バッファ)。DX12 の型を含むので中身は Renderer.cpp で定義する
        struct GpuMesh;

        struct PendingRelease
        {
            std::unique_ptr<GpuMesh> mesh;
            uint64_t fenceValue = 0; // この引換券の番号まで GPU が進んだら捨ててよい(0 = まだ決まっていない)
        };

        struct PendingTextureRelease
        {
            std::unique_ptr<DX12Texture> texture;
            uint64_t fenceValue = 0; // PendingRelease と同じ
        };

        struct DrawCommand
        {
            MeshHandle mesh;
            DirectX::XMFLOAT4X4 world;
        };

        std::unique_ptr<DX12Device> m_Device;
        std::unique_ptr<DX12CommandQueue> m_Queue;
        std::unique_ptr<DX12SwapChain> m_SwapChain;
        std::unique_ptr<DX12DepthBuffer> m_DepthBuffer;
        std::unique_ptr<DX12CommandList> m_CommandList;
        std::unique_ptr<DX12CommandList> m_UploadCommandList; // 画像を、Default ヒープへコピーする命令を書く(起動時の読み込み用。毎フレームの m_CommandList とは別)
        std::unique_ptr<DX12DescriptorHeap> m_SrvHeap; // シェーダーが読む画像の札掛け(ImGui のフォント画像など)
        std::unique_ptr<ImGuiLayer> m_ImGui;           // デバッグ用の画面(SrvHeap の枠を借りるので、SrvHeap の後に作り、前に捨てる)
        std::unique_ptr<DX12ShaderCompiler> m_ShaderCompiler;
        std::unique_ptr<DX12Pipeline> m_Pipeline;
        std::unique_ptr<DX12Pipeline> m_SkyPipeline; // 空専用(画面全体を覆う三角形を、深度なしで描く)。Root Signature は m_Pipeline と共有

        std::unordered_map<uint32_t, std::unique_ptr<GpuMesh>> m_Meshes; // 一覧表(番号札 → 形の実体)
        uint32_t m_NextMeshId = 1;                                       // 次に発行する番号(使い回さない)
        std::vector<PendingRelease> m_PendingReleases;                   // 捨てる予定リスト
        std::unordered_map<uint32_t, std::unique_ptr<DX12Texture>> m_Textures; // 画像の一覧表(番号札 → 画像の実体)
        uint32_t m_NextTextureId = 1;                                    // 次に発行する画像の番号(使い回さない)
        std::vector<PendingTextureRelease> m_PendingTextureReleases;     // 捨てる予定の画像
        TextureHandle m_WhiteTexture;                                    // 白の 1×1(画像を貼らない物に使う。6-4)
        std::vector<DrawCommand> m_DrawList;                             // 描画リスト(毎フレーム空にする)
        std::unordered_set<uint32_t> m_ReportedInvalidMeshes;            // エラーを出済みの無効な番号(1回だけ出すため)

        // カメラの行列を入れる定数バッファ(別紙)。GPU が読んでいる途中で書き換えないよう、フレームインフライトの数だけ持つ
        std::array<std::unique_ptr<DX12UploadBuffer>, FramesInFlight> m_FrameConstantBuffers;
        Camera m_Camera;
        Lighting m_Lighting;
        Fog m_Fog;
        Sky m_Sky;

        // 各フレームの命令が GPU で終わったときに到達する Fence 値
        std::array<uint64_t, FramesInFlight> m_FrameFenceValues = {};
        uint32_t m_FrameIndex = 0;

        std::array<float, 4> m_ClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
        bool m_VSync = true;
    };
}
