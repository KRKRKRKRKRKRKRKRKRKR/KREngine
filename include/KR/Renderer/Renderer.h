#pragma once

#include "KR/Renderer/Camera.h"
#include "KR/Renderer/Lighting.h"
#include "KR/Renderer/Mesh.h"
#include "KR/Renderer/Texture.h"
#include "KR/Physics/Collision.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace KR
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
        // texture: 貼る画像。省略(無効な番号札)なら、白の 1×1(頂点の色のまま。画像を貼らない物は、今までと同じ見た目)
        // overlay: true なら、深度テストなしで、ほかの物の「手前」に描く(Blender の「In Front」。物の中にある骨を、見せるときなど)。骨つきの形(DrawSkinnedMesh)には使えない
        void DrawMesh(MeshHandle mesh, const DirectX::XMMATRIX& world, TextureHandle texture = {}, bool overlay = false);

        // 骨つきの形(MeshData::skin を持つ形)を、骨の行列の表(パレット)で動かして描く依頼を、描画リストに積む(OnRender で呼ぶ)
        // palette: 骨ごとの「逆バインド行列 × 骨の今の世界の行列」(ComputeSkinMatrices で作る)。最大 MaxSkinJoints 本
        // 骨なしの形を渡したり、骨つきの形を DrawMesh で描いたりすると、エラー(1 回)を出して、描かない
        // 1 フレームに描ける、骨つきの物は MaxSkinnedDrawsPerFrame 個まで(超えた分は、警告を出して描かない)
        void DrawSkinnedMesh(MeshHandle mesh, const DirectX::XMMATRIX& world, std::span<const DirectX::XMFLOAT4X4> palette, TextureHandle texture = {});

        // 骨の本数の上限(パレットの大きさ。シェーダーの MaxJoints と同じ)・1 フレームに描ける骨つきの物の数
        static constexpr uint32_t MaxSkinJoints = 128;
        static constexpr uint32_t MaxSkinnedDrawsPerFrame = 32;

        // デバッグ用の線を 1 本、積む(OnRender で呼ぶ)。座標は世界の座標。光・フォグはかからず、色のまま描く
        // DrawMesh と同じく、その場では描かず、EndFrame で、形のあとに、まとめて 1 回で描く(深度テストあり。物に隠れる)
        // overlay: true なら、深度テストなしで、ほかの物の手前に描く(DrawMesh の overlay と同じ)
        void DrawLine(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, const DirectX::XMFLOAT4& color, bool overlay = false);

        // OBB の 12 本の辺を、線で積む(当たり判定の箱の確認用)
        void DrawWireBox(const OBB& box, const DirectX::XMFLOAT4& color);

        // 球を、3 つの円(XY・YZ・XZ の面)の線で積む(当たり判定の球の確認用。72 本)
        void DrawWireSphere(const Sphere& sphere, const DirectX::XMFLOAT4& color);

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

        // 積んだ線を、今のフレームの頂点バッファに書いて、まとめて 1 回で描く(ExecuteDrawList の最後に呼ぶ)
        void ExecuteLines(bool overlay);

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

        // デバッグの線の頂点(Line.hlsl の VSInput と、並びを合わせる)
        struct LineVertex
        {
            DirectX::XMFLOAT3 position; // 世界の座標
            DirectX::XMFLOAT4 color;
        };

        // 1 フレームに描ける線の頂点の数(2 つで 1 本 → 32,768 本)。超えた分は描かない
        static constexpr uint32_t MaxLineVertices = 65536;

        struct DrawCommand
        {
            MeshHandle mesh;
            DirectX::XMFLOAT4X4 world;
            TextureHandle texture; // 無効なら、白の 1×1
            bool skinned = false;          // 骨つきか(パレットを使う)
            uint32_t paletteByteOffset = 0; // 骨つきのとき:パレットの表(m_PaletteStaging)の中での、この物のパレットの位置(バイト。256 の倍数)
            bool overlay = false;           // 深度テストなしで、ほかの物の手前に描くか
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
        std::unique_ptr<DX12Pipeline> m_LinePipeline; // デバッグの線専用(2 頂点で 1 本。光・フォグなし・深度あり)。Root Signature は m_Pipeline と共有
        std::unique_ptr<DX12Pipeline> m_SkinnedPipeline; // 骨つきの形専用(頂点シェーダーが、パレットで頂点を動かす。あとは m_Pipeline と同じ)。Root Signature は m_Pipeline と共有
        std::unique_ptr<DX12Pipeline> m_OverlayPipeline;     // 手前に描く物(overlay)専用。物と同じ、深度テストだけなし。Root Signature は m_Pipeline と共有
        std::unique_ptr<DX12Pipeline> m_OverlayLinePipeline; // 手前に描く線(overlay)専用。線と同じ、深度テストだけなし。Root Signature は m_Pipeline と共有

        std::unordered_map<uint32_t, std::unique_ptr<GpuMesh>> m_Meshes; // 一覧表(番号札 → 形の実体)
        uint32_t m_NextMeshId = 1;                                       // 次に発行する番号(使い回さない)
        std::vector<PendingRelease> m_PendingReleases;                   // 捨てる予定リスト
        std::unordered_map<uint32_t, std::unique_ptr<DX12Texture>> m_Textures; // 画像の一覧表(番号札 → 画像の実体)
        uint32_t m_NextTextureId = 1;                                    // 次に発行する画像の番号(使い回さない)
        std::vector<PendingTextureRelease> m_PendingTextureReleases;     // 捨てる予定の画像
        TextureHandle m_WhiteTexture;                                    // 白の 1×1(画像を貼らない物に使う。6-4)
        std::vector<DrawCommand> m_DrawList;                             // 描画リスト(毎フレーム空にする)
        std::unordered_set<uint32_t> m_ReportedInvalidMeshes;            // エラーを出済みの無効な番号(1回だけ出すため)
        std::unordered_set<uint32_t> m_ReportedInvalidTextures;          // 同じく、画像の無効な番号

        // 骨つきの物のパレット(DrawSkinnedMesh で、ここに積み、ExecuteDrawList で、GPU の箱に、まとめて書く。毎フレーム空にする)
        std::vector<DirectX::XMFLOAT4X4> m_PaletteStaging;
        std::array<std::unique_ptr<DX12UploadBuffer>, FramesInFlight> m_PaletteBuffers; // パレットを GPU に渡す箱。フレームの数だけ持つ(GPU が読んでいる途中で書き換えないため)
        bool m_ReportedPaletteOverflow = false;                          // 骨つきの物の上限を超えた警告を出済みか(1 回だけ出すため)

        // デバッグの線(DrawLine で積み、EndFrame で、まとめて描く)
        std::vector<LineVertex> m_LineVertices;                          // 今のフレームで積んだ線の頂点(2 つで 1 本。毎フレーム空にする)
        std::vector<LineVertex> m_OverlayLineVertices;                   // 手前に描く線(overlay)の頂点。通常の線のあとに、同じ箱へ並べて書く
        uint32_t m_LineBufferNormalCount = 0;                            // 今のフレームの箱の先頭から、通常の線の頂点が、いくつあるか(overlay の線の、描き始めの位置)
        std::array<std::unique_ptr<DX12UploadBuffer>, FramesInFlight> m_LineVertexBuffers; // 線の頂点を GPU に渡す箱。GPU が読んでいる途中で書き換えないよう、フレームの数だけ持つ
        bool m_ReportedLineOverflow = false;                             // 線の上限を超えた警告を出済みか(1 回だけ出すため)

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
