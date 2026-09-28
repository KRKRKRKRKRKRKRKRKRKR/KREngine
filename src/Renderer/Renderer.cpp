#include "TenFiftyEight/Renderer/Renderer.h"

#include "Graphics/DX12/DX12Buffer.h"
#include "Graphics/DX12/DX12CommandList.h"
#include "Graphics/DX12/DX12CommandQueue.h"
#include "Graphics/DX12/DX12DepthBuffer.h"
#include "Graphics/DX12/DX12Device.h"
#include "Graphics/DX12/DX12Pipeline.h"
#include "Graphics/DX12/DX12ShaderCompiler.h"
#include "Graphics/DX12/DX12SwapChain.h"
#include "Platform/Windows/Window.h"

#include <DirectXMath.h>

#include <filesystem>

using namespace DirectX;

namespace TenFiftyEight
{
    namespace
    {
        // 頂点1つ分のデータ。シェーダーの VSInput と形を合わせる
        struct Vertex
        {
            float position[3];
            float color[4];
        };

        // 頂点データのどこに何が入っているかを GPU に教える(名前はシェーダーの「: POSITION」などと対応)
        constexpr D3D12_INPUT_ELEMENT_DESC VertexInputLayout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, offsetof(Vertex, position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(Vertex, color),    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        // 処理2: 1辺1、中心が原点のキューブ(2-7 で Game から渡す形に置き換える)
        // 面ごとに色を分けるため、面ごとに専用の頂点を4つずつ持つ(4頂点 × 6面 = 24頂点)
        // 各面とも「その面を外側から見て 左上 → 右上 → 右下 → 左下」の順に並べる
        constexpr float Red[4]    = { 1.0f, 0.0f, 0.0f, 1.0f };
        constexpr float Cyan[4]   = { 0.0f, 1.0f, 1.0f, 1.0f };
        constexpr float Green[4]  = { 0.0f, 1.0f, 0.0f, 1.0f };
        constexpr float Purple[4] = { 0.5f, 0.0f, 1.0f, 1.0f };
        constexpr float Blue[4]   = { 0.0f, 0.0f, 1.0f, 1.0f };
        constexpr float Yellow[4] = { 1.0f, 1.0f, 0.0f, 1.0f };

        constexpr Vertex TestCubeVertices[] = {
            // 前(-Z): 赤
            { { -0.5f,  0.5f, -0.5f }, { Red[0], Red[1], Red[2], Red[3] } },
            { {  0.5f,  0.5f, -0.5f }, { Red[0], Red[1], Red[2], Red[3] } },
            { {  0.5f, -0.5f, -0.5f }, { Red[0], Red[1], Red[2], Red[3] } },
            { { -0.5f, -0.5f, -0.5f }, { Red[0], Red[1], Red[2], Red[3] } },
            // 後(+Z): 水色
            { {  0.5f,  0.5f,  0.5f }, { Cyan[0], Cyan[1], Cyan[2], Cyan[3] } },
            { { -0.5f,  0.5f,  0.5f }, { Cyan[0], Cyan[1], Cyan[2], Cyan[3] } },
            { { -0.5f, -0.5f,  0.5f }, { Cyan[0], Cyan[1], Cyan[2], Cyan[3] } },
            { {  0.5f, -0.5f,  0.5f }, { Cyan[0], Cyan[1], Cyan[2], Cyan[3] } },
            // 右(+X): 緑
            { {  0.5f,  0.5f, -0.5f }, { Green[0], Green[1], Green[2], Green[3] } },
            { {  0.5f,  0.5f,  0.5f }, { Green[0], Green[1], Green[2], Green[3] } },
            { {  0.5f, -0.5f,  0.5f }, { Green[0], Green[1], Green[2], Green[3] } },
            { {  0.5f, -0.5f, -0.5f }, { Green[0], Green[1], Green[2], Green[3] } },
            // 左(-X): 紫
            { { -0.5f,  0.5f,  0.5f }, { Purple[0], Purple[1], Purple[2], Purple[3] } },
            { { -0.5f,  0.5f, -0.5f }, { Purple[0], Purple[1], Purple[2], Purple[3] } },
            { { -0.5f, -0.5f, -0.5f }, { Purple[0], Purple[1], Purple[2], Purple[3] } },
            { { -0.5f, -0.5f,  0.5f }, { Purple[0], Purple[1], Purple[2], Purple[3] } },
            // 上(+Y): 青
            { { -0.5f,  0.5f,  0.5f }, { Blue[0], Blue[1], Blue[2], Blue[3] } },
            { {  0.5f,  0.5f,  0.5f }, { Blue[0], Blue[1], Blue[2], Blue[3] } },
            { {  0.5f,  0.5f, -0.5f }, { Blue[0], Blue[1], Blue[2], Blue[3] } },
            { { -0.5f,  0.5f, -0.5f }, { Blue[0], Blue[1], Blue[2], Blue[3] } },
            // 下(-Y): 黄
            { { -0.5f, -0.5f, -0.5f }, { Yellow[0], Yellow[1], Yellow[2], Yellow[3] } },
            { {  0.5f, -0.5f, -0.5f }, { Yellow[0], Yellow[1], Yellow[2], Yellow[3] } },
            { {  0.5f, -0.5f,  0.5f }, { Yellow[0], Yellow[1], Yellow[2], Yellow[3] } },
            { { -0.5f, -0.5f,  0.5f }, { Yellow[0], Yellow[1], Yellow[2], Yellow[3] } },
        };

        // 処理1: どの頂点で三角形を作るかの番号リスト。各面 0,1,2(左上・右上・右下)と 0,2,3(左上・右下・左下)
        constexpr uint16_t TestCubeIndices[] = {
             0,  1,  2,    0,  2,  3, // 前
             4,  5,  6,    4,  6,  7, // 後
             8,  9, 10,    8, 10, 11, // 右
            12, 13, 14,   12, 14, 15, // 左
            16, 17, 18,   16, 18, 19, // 上
            20, 21, 22,   20, 22, 23, // 下
        };

        // Debug: 編集中の元ファイル(ホットリロード用) / Release: exe の横にコピーされたもの
        std::filesystem::path GetShaderDirectory()
        {
#ifdef TFE_DEBUG
            return std::filesystem::path(TFE_SHADER_SOURCE_DIR);
#else
            wchar_t exePath[MAX_PATH] = {};
            GetModuleFileNameW(nullptr, exePath, MAX_PATH);
            return std::filesystem::path(exePath).parent_path() / L"Shaders";
#endif
        }
    }

    //レンダー生成
    Renderer::Renderer(const Window& window)
    {
        m_Device = std::make_unique<DX12Device>();
        m_Queue = std::make_unique<DX12CommandQueue>(m_Device->Get(), D3D12_COMMAND_LIST_TYPE_DIRECT);

        DX12SwapChainDesc swapChainDesc;
        swapChainDesc.factory = m_Device->GetFactory();
        swapChainDesc.device = m_Device->Get();
        swapChainDesc.queue = m_Queue->Get();
        swapChainDesc.window = window.GetHandle();
        swapChainDesc.width = window.GetWidth();
        swapChainDesc.height = window.GetHeight();
        m_SwapChain = std::make_unique<DX12SwapChain>(swapChainDesc);

        // 2-5 処理2: バックバッファと同じ大きさの深度バッファ
        m_DepthBuffer = std::make_unique<DX12DepthBuffer>(m_Device->Get(), window.GetWidth(), window.GetHeight());

        m_CommandList = std::make_unique<DX12CommandList>(m_Device->Get(), D3D12_COMMAND_LIST_TYPE_DIRECT, FramesInFlight);

        m_ShaderCompiler = std::make_unique<DX12ShaderCompiler>();
        if (!LoadShaders())
        {
            TFE_LOG_FATAL("起動時のシェーダー読み込みに失敗しました");
        }

        // 頂点リストと番号リストを、それぞれ Upload ヒープのバッファに入れる
        m_TestVertexBuffer = std::make_unique<DX12UploadBuffer>(m_Device->Get(), sizeof(TestCubeVertices));
        m_TestVertexBuffer->Write(TestCubeVertices, sizeof(TestCubeVertices));
        m_TestIndexBuffer = std::make_unique<DX12UploadBuffer>(m_Device->Get(), sizeof(TestCubeIndices));
        m_TestIndexBuffer->Write(TestCubeIndices, sizeof(TestCubeIndices));
    }

    //レンダー削除
    Renderer::~Renderer()
    {
        // GPU が使用中のリソースを解放しないよう、すべての処理の完了を待つ
        m_Queue->Flush();

        // 作成と逆の順番で解放してから、解放漏れを調べる
        m_TestIndexBuffer.reset();
        m_TestVertexBuffer.reset();
        m_Pipeline.reset();
        m_ShaderCompiler.reset();
        m_CommandList.reset();
        m_DepthBuffer.reset();
        m_SwapChain.reset();
        m_Queue.reset();
        m_Device.reset();
        ReportLiveDX12Objects();
    }

    bool Renderer::LoadShaders()
    {
        const std::filesystem::path shaderPath = GetShaderDirectory() / L"Basic.hlsl";

        // 同じファイルなので、頂点シェーダーで失敗したらピクセルシェーダーも同じエラーになる。先に止める
        ComPtr<IDxcBlob> vertexShader = m_ShaderCompiler->Compile(shaderPath, L"VSMain", L"vs_6_0");
        if (!vertexShader)
        {
            return false;
        }
        ComPtr<IDxcBlob> pixelShader = m_ShaderCompiler->Compile(shaderPath, L"PSMain", L"ps_6_0");
        if (!pixelShader)
        {
            return false;
        }

        DX12PipelineDesc pipelineDesc;
        pipelineDesc.vertexShader = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
        pipelineDesc.pixelShader = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
        pipelineDesc.inputLayout = VertexInputLayout;

        std::unique_ptr<DX12Pipeline> pipeline = DX12Pipeline::Create(m_Device->Get(), pipelineDesc);
        if (!pipeline)
        {
            return false;
        }

        // 古いパイプラインを GPU が使い終わってから差し替える
        m_Queue->Flush();
        m_Pipeline = std::move(pipeline);
        return true;
    }

    void Renderer::ReloadShaders()
    {
        if (LoadShaders())
        {
            TFE_LOG_INFO("シェーダー再読み込み: 成功");
        }
        else
        {
            TFE_LOG_ERROR("シェーダー再読み込み: 失敗(前のシェーダーのまま動作を続けます)");
        }
    }

    //画面変化時の調整
    void Renderer::Resize(uint32_t width, uint32_t height)
    {
        // バックバッファを作り直すので、GPU がそれを使い終わるまで待つ
        m_Queue->Flush();
        m_SwapChain->Resize(width, height);
        m_DepthBuffer->Resize(width, height); // 2-5 処理5: 深度バッファも同じ大きさに作り直す
    }

    //フレーム開始
    void Renderer::BeginFrame()
    {
        // このフレーム番号の Allocator を前回使ったフレーム(2フレーム前)が、GPU で終わるまで待つ
        m_Queue->WaitForValue(m_FrameFenceValues[m_FrameIndex]);

        ID3D12GraphicsCommandList* commandList = m_CommandList->Begin(m_FrameIndex);

        // バックバッファを「表示用」から「描画用」に切り替える
        const CD3DX12_RESOURCE_BARRIER toRenderTarget = CD3DX12_RESOURCE_BARRIER::Transition(
            m_SwapChain->GetCurrentBackBuffer(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        commandList->ResourceBarrier(1, &toRenderTarget);

        const D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_SwapChain->GetCurrentRenderTargetView();
        commandList->ClearRenderTargetView(rtv, m_ClearColor.data(), 0, nullptr);

        // 2-5 処理3: 深度を 1.0(一番奥)で塗りつぶし、前のフレームの奥行きの記録を消す
        const D3D12_CPU_DESCRIPTOR_HANDLE dsv = m_DepthBuffer->GetView();
        commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, DepthClearValue, 0, 0, nullptr);

        // 描画先: 色はバックバッファへ、奥行きは深度バッファへ
        commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

        // ビューポート: 描画結果をバックバッファのどの範囲に映すか / シザー: どの範囲の外を切り捨てるか
        const auto width = static_cast<float>(m_SwapChain->GetWidth());
        const auto height = static_cast<float>(m_SwapChain->GetHeight());
        const D3D12_VIEWPORT viewport = { 0.0f, 0.0f, width, height, 0.0f, 1.0f };
        const D3D12_RECT scissor = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
        commandList->RSSetViewports(1, &viewport);
        commandList->RSSetScissorRects(1, &scissor);
    }

    void Renderer::DrawTestCube()
    {
        ID3D12GraphicsCommandList* commandList = m_CommandList->Get();

        commandList->SetGraphicsRootSignature(m_Pipeline->GetRootSignature());
        commandList->SetPipelineState(m_Pipeline->Get());

        // 頂点バッファの場所・全体の大きさ・1頂点の大きさを伝える
        D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};
        vertexBufferView.BufferLocation = m_TestVertexBuffer->GetGpuAddress();
        vertexBufferView.SizeInBytes = static_cast<UINT>(m_TestVertexBuffer->GetSize());
        vertexBufferView.StrideInBytes = sizeof(Vertex);
        commandList->IASetVertexBuffers(0, 1, &vertexBufferView);

        // 処理1: 番号リストの場所・全体の大きさ・番号の型(uint16_t)を伝える
        D3D12_INDEX_BUFFER_VIEW indexBufferView = {};
        indexBufferView.BufferLocation = m_TestIndexBuffer->GetGpuAddress();
        indexBufferView.SizeInBytes = static_cast<UINT>(m_TestIndexBuffer->GetSize());
        indexBufferView.Format = DXGI_FORMAT_R16_UINT;
        commandList->IASetIndexBuffer(&indexBufferView);

        // 番号を3つずつ区切って三角形として扱う
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // 同じ頂点・番号のバッファを使い、b0 の箱の中身(ワールド行列)だけを入れ替えて何個でも描ける
        auto drawCube = [&](const XMMATRIX& world)
        {
            // 処理4: 行列(数字16個)を b0 の箱に入れる。XMMATRIX は計算用の型なので、ふつうの数字の並びに書き出してから渡す
            XMFLOAT4X4 worldData;
            XMStoreFloat4x4(&worldData, world);
            commandList->SetGraphicsRoot32BitConstants(ObjectConstantsRootIndex, 16, &worldData, 0);

            // 番号を36個使って描く(= 三角形12個 = 6面)
            commandList->DrawIndexedInstanced(static_cast<UINT>(std::size(TestCubeIndices)), 1, 0, 0, 0);
        };

        // 処理3: ワールド行列を作る。S(拡大)→ R(回転)→ T(移動)の順に掛ける
        //   拡大と「奥へ移動」は、カメラ(2-6)ができるまでの一時的な対応
        //   (画面の奥行きは 0〜1 の範囲しか映らないため、回転しても収まる大きさ・位置にしている)
        // 1つ目: 手前で回転するキューブ(先に描く)
        drawCube(
            XMMatrixScaling(0.5f, 0.5f, 0.5f) *
            XMMatrixRotationY(m_TestCubeAngle) *
            XMMatrixRotationX(m_TestCubeAngle * 0.5f) *
            XMMatrixTranslation(0.0f, 0.0f, 0.5f));

        // 2-5 処理1: 2つ目: 右上の奥にあるキューブ(後から描く)
        //   深度テストがないと、奥にあるのに1つ目の上に描かれてしまう
        drawCube(
            XMMatrixScaling(0.35f, 0.35f, 0.35f) *
            XMMatrixRotationY(-m_TestCubeAngle) *
            XMMatrixTranslation(0.3f, 0.25f, 0.65f));
    }

    //フレーム終了
    void Renderer::EndFrame()
    {
        DrawTestCube();

        ID3D12GraphicsCommandList* commandList = m_CommandList->Get();

        // バックバッファを「描画用」から「表示用」に戻す
        const CD3DX12_RESOURCE_BARRIER toPresent = CD3DX12_RESOURCE_BARRIER::Transition(
            m_SwapChain->GetCurrentBackBuffer(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT);
        commandList->ResourceBarrier(1, &toPresent);

        m_CommandList->End();
        m_Queue->Execute(commandList);

        m_SwapChain->Present(m_VSync);

        // このフレームの命令が GPU で終わったら到達する値を記録しておく
        m_FrameFenceValues[m_FrameIndex] = m_Queue->Signal();
        m_FrameIndex = (m_FrameIndex + 1) % FramesInFlight;
    }
}
