#include "TenFiftyEight/Renderer/Renderer.h"

#include "Graphics/DX12/DX12Buffer.h"
#include "Graphics/DX12/DX12CommandList.h"
#include "Graphics/DX12/DX12CommandQueue.h"
#include "Graphics/DX12/DX12Device.h"
#include "Graphics/DX12/DX12Pipeline.h"
#include "Graphics/DX12/DX12ShaderCompiler.h"
#include "Graphics/DX12/DX12SwapChain.h"
#include "Platform/Windows/Window.h"

#include <filesystem>

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

        // 画面の座標(中央が 0,0 / 右上が 1,1 / 左下が -1,-1)で直接指定した三角形
        // 表から見て時計回りに並べる(反時計回りは裏向きとみなされ、描かれない)
        constexpr Vertex TestTriangle[] = {
            { {  0.0f,  0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } }, // 上: 赤
            { {  0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } }, // 右下: 緑
            { { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }, // 左下: 青
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

        m_CommandList = std::make_unique<DX12CommandList>(m_Device->Get(), D3D12_COMMAND_LIST_TYPE_DIRECT, FramesInFlight);

        m_ShaderCompiler = std::make_unique<DX12ShaderCompiler>();
        if (!LoadShaders())
        {
            TFE_LOG_FATAL("起動時のシェーダー読み込みに失敗しました");
        }

        m_TestVertexBuffer = std::make_unique<DX12UploadBuffer>(m_Device->Get(), sizeof(TestTriangle));
        m_TestVertexBuffer->Write(TestTriangle, sizeof(TestTriangle));
    }

    Renderer::~Renderer()
    {
        // GPU が使用中のリソースを解放しないよう、すべての処理の完了を待つ
        m_Queue->Flush();

        // 作成と逆の順番で解放してから、解放漏れを調べる
        m_TestVertexBuffer.reset();
        m_Pipeline.reset();
        m_ShaderCompiler.reset();
        m_CommandList.reset();
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

    void Renderer::Resize(uint32_t width, uint32_t height)
    {
        // バックバッファを作り直すので、GPU がそれを使い終わるまで待つ
        m_Queue->Flush();
        m_SwapChain->Resize(width, height);
    }

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
        commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

        // ビューポート: 描画結果をバックバッファのどの範囲に映すか / シザー: どの範囲の外を切り捨てるか
        const auto width = static_cast<float>(m_SwapChain->GetWidth());
        const auto height = static_cast<float>(m_SwapChain->GetHeight());
        const D3D12_VIEWPORT viewport = { 0.0f, 0.0f, width, height, 0.0f, 1.0f };
        const D3D12_RECT scissor = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
        commandList->RSSetViewports(1, &viewport);
        commandList->RSSetScissorRects(1, &scissor);
    }

    void Renderer::DrawTestTriangle()
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

        // 頂点を3つずつ区切って三角形として扱う
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        commandList->DrawInstanced(3, 1, 0, 0);
    }

    void Renderer::EndFrame()
    {
        DrawTestTriangle();

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
