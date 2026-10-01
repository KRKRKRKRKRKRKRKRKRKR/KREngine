#include "TenFiftyEight/Renderer/Renderer.h"

#include "Graphics/DX12/DX12Buffer.h"
#include "Graphics/DX12/DX12CommandList.h"
#include "Graphics/DX12/DX12CommandQueue.h"
#include "Graphics/DX12/DX12DepthBuffer.h"
#include "Graphics/DX12/DX12DescriptorHeap.h"
#include "Graphics/DX12/DX12Device.h"
#include "Graphics/DX12/DX12Pipeline.h"
#include "Graphics/DX12/DX12ShaderCompiler.h"
#include "Graphics/DX12/DX12SwapChain.h"
#include "Platform/Windows/Window.h"
#include "TenFiftyEight/Core/Assets.h"
#include "UI/ImGuiLayer.h"

#include <DirectXMath.h>

#include <filesystem>

using namespace DirectX;

namespace TenFiftyEight
{
    // 2-7 処理2: 形の実体。一覧表(m_Meshes)に番号札と対応づけて保存する
    struct Renderer::GpuMesh
    {
        std::unique_ptr<DX12UploadBuffer> vertexBuffer;
        std::unique_ptr<DX12UploadBuffer> indexBuffer;
        uint32_t indexCount = 0;
    };

    namespace
    {
        // b1 の別紙(定数バッファ)の中身。シェーダーの cbuffer FrameConstants と形を合わせる
        // float4 にそろえているのは、HLSL の定数バッファが「16 バイトの区切り」で並ぶため(float3 だと、並びがずれる)
        struct FrameConstants
        {
            XMFLOAT4X4 viewProjection;
            XMFLOAT4 toLight;      // xyz = 物から太陽へ向かう向き(長さ 1)
            XMFLOAT4 lightColor;   // rgb = 太陽の光の色
            XMFLOAT4 ambientColor; // rgb = 環境光
            XMFLOAT4 cameraPosition; // xyz = カメラの世界での位置(フォグの距離の計算に使う)
            XMFLOAT4 fogColor;       // rgb = 霧の色(背景色と同じ)
            XMFLOAT4 fogParams;      // x = 濃さ(フォグなしのときは 0)
            XMFLOAT4X4 inverseViewProjection; // viewProjection の逆(空: 画面の座標 → 世界の座標。視線の向きを求める)
            XMFLOAT4 skyHorizon;     // rgb = 地平線の色
            XMFLOAT4 skyZenith;      // rgb = 天頂の色 / w = 地平線の光の幅
            XMFLOAT4 skyNadir;       // rgb = 下の色(視線が地平線より下のとき)
        };

        // シェーダーから見える札掛け(SRV ヒープ)の枠の数。ImGui のフォント画像など、画像の札を掛ける
        constexpr uint32_t SrvHeapSize = 64;

        // 定数バッファの大きさは 256 バイト単位にする決まり(中身が 64 バイトでも 256 バイト確保する)
        constexpr uint64_t FrameConstantBufferSize = (sizeof(FrameConstants) + 255) & ~uint64_t(255);

        // 頂点データ(Mesh.h の Vertex)のどこに何が入っているかを GPU に教える(名前はシェーダーの「: POSITION」などと対応)
        constexpr D3D12_INPUT_ELEMENT_DESC VertexInputLayout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, offsetof(Vertex, position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, offsetof(Vertex, normal),   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(Vertex, color),    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        // Debug: 編集中の元ファイル(ホットリロード用) / Release: exe の横にコピーされたもの(場所の決まりは GetAssetDirectory にある)
        std::filesystem::path GetShaderDirectory()
        {
            return GetAssetDirectory("Shaders");
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

        // ImGui I-2: シェーダーから見える札掛け(SRV 用)。命令リストに設定できる SRV ヒープは1つだけなので、これが唯一のもの
        m_SrvHeap = std::make_unique<DX12DescriptorHeap>(m_Device->Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, SrvHeapSize, true);

        // ImGui I-3: デバッグ用の画面の準備。SRV ヒープの枠を借りるので、ヒープの後に作る
        m_ImGui = std::make_unique<ImGuiLayer>(window.GetHandle(), m_Device->Get(), m_Queue->Get(), m_SrvHeap.get(),
                                               FramesInFlight, LinearRenderTargetFormat, DXGI_FORMAT_UNKNOWN);
        // ↑ I-6c: ImGui は、変換なしの見方(UNORM)で、深度なしで描く(色が最初からモニター用の値で、常に手前に描くため)

        m_ShaderCompiler = std::make_unique<DX12ShaderCompiler>();
        if (!LoadShaders())
        {
            TFE_LOG_FATAL("起動時のシェーダー読み込みに失敗しました");
        }

        // 2-6 処理4: カメラの行列用の定数バッファ(フレームごとに1枚、計2枚)
        for (std::unique_ptr<DX12UploadBuffer>& buffer : m_FrameConstantBuffers)
        {
            buffer = std::make_unique<DX12UploadBuffer>(m_Device->Get(), FrameConstantBufferSize);
        }
    }

    //レンダー削除
    Renderer::~Renderer()
    {
        // GPU が使用中のリソースを解放しないよう、すべての処理の完了を待つ
        m_Queue->Flush();

        // 作成と逆の順番で解放してから、解放漏れを調べる
        // (Game が DestroyMesh し忘れた形も、ここでまとめて解放する)
        m_PendingReleases.clear();
        m_Meshes.clear();
        for (std::unique_ptr<DX12UploadBuffer>& buffer : m_FrameConstantBuffers)
        {
            buffer.reset();
        }
        m_SkyPipeline.reset();
        m_Pipeline.reset();
        m_ShaderCompiler.reset();
        m_ImGui.reset(); // SRV ヒープの枠を返しに行くので、ヒープより先に捨てる
        m_SrvHeap.reset();
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

        // 4-7b: 空のシェーダー(Sky.hlsl)もコンパイルして、空のパイプラインを作る
        const std::filesystem::path skyShaderPath = GetShaderDirectory() / L"Sky.hlsl";
        ComPtr<IDxcBlob> skyVertexShader = m_ShaderCompiler->Compile(skyShaderPath, L"VSMain", L"vs_6_0");
        if (!skyVertexShader)
        {
            return false;
        }
        ComPtr<IDxcBlob> skyPixelShader = m_ShaderCompiler->Compile(skyShaderPath, L"PSMain", L"ps_6_0");
        if (!skyPixelShader)
        {
            return false;
        }

        DX12PipelineDesc skyDesc;
        skyDesc.vertexShader = { skyVertexShader->GetBufferPointer(), skyVertexShader->GetBufferSize() };
        skyDesc.pixelShader = { skyPixelShader->GetBufferPointer(), skyPixelShader->GetBufferSize() };
        // 頂点バッファなし(入力レイアウトは空)。深度テストなし・カリングなし。Root Signature は、物のパイプラインと共有する
        skyDesc.sharedRootSignature = pipeline->GetRootSignature();
        skyDesc.depthEnabled = false;
        skyDesc.cullBackFaces = false;

        std::unique_ptr<DX12Pipeline> skyPipeline = DX12Pipeline::Create(m_Device->Get(), skyDesc);
        if (!skyPipeline)
        {
            return false;
        }

        // 古いパイプラインを GPU が使い終わってから差し替える(物と空は、同時に差し替える)
        m_Queue->Flush();
        m_Pipeline = std::move(pipeline);
        m_SkyPipeline = std::move(skyPipeline);
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

    bool Renderer::HandleWindowMessage(void* window, unsigned int message, uintptr_t wParam, intptr_t lParam)
    {
        return ImGuiLayer::HandleMessage(static_cast<HWND>(window), message, static_cast<WPARAM>(wParam), static_cast<LPARAM>(lParam));
    }

    bool Renderer::ImGuiWantsKeyboard() const
    {
        return m_ImGui && m_ImGui->WantsKeyboard();
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

        // 2-7 処理3: GPU が使い終わった形を捨てる
        ReleaseFinishedMeshes();

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

        // 2-6 処理6: 縦横比を今のバックバッファの大きさから計算する(リサイズしても毎フレーム正しい値になる)
        const float aspectRatio = width / height;

        // 2-6 処理4: ビュー行列 × プロジェクション行列を CPU で1つにまとめ、このフレーム用の別紙に書く
        FrameConstants frameConstants;
        const XMMATRIX viewProjection = m_Camera.GetViewMatrix() * m_Camera.GetProjectionMatrix(aspectRatio);
        XMStoreFloat4x4(&frameConstants.viewProjection, viewProjection);

        // 4-7b: 空は、画面の座標から視線の向きを求めるので、逆行列も渡す
        XMStoreFloat4x4(&frameConstants.inverseViewProjection, XMMatrixInverse(nullptr, viewProjection));
        frameConstants.skyHorizon = { m_Sky.horizonColor.x, m_Sky.horizonColor.y, m_Sky.horizonColor.z, 1.0f };
        frameConstants.skyZenith = { m_Sky.zenithColor.x, m_Sky.zenithColor.y, m_Sky.zenithColor.z, m_Sky.horizonWidth };
        frameConstants.skyNadir = { m_Sky.nadirColor.x, m_Sky.nadirColor.y, m_Sky.nadirColor.z, 1.0f };

        // 4-3: 光も同じ別紙に書く。光の向きは、陰影の計算(内積)のため、長さ 1 にそろえる
        XMStoreFloat4(&frameConstants.toLight, XMVector3Normalize(XMLoadFloat3(&m_Lighting.toLight)));
        frameConstants.lightColor = { m_Lighting.color.x, m_Lighting.color.y, m_Lighting.color.z, 0.0f };
        frameConstants.ambientColor = { m_Lighting.ambient.x, m_Lighting.ambient.y, m_Lighting.ambient.z, 0.0f };

        // 4-4: フォグも同じ別紙に書く。霧の色は、空の地平線の色と同じにする(遠くが空に溶けるように)。フォグなしは、濃さ 0(見える割合が常に 1)
        const XMFLOAT3& cameraPosition = m_Camera.GetPosition();
        frameConstants.cameraPosition = { cameraPosition.x, cameraPosition.y, cameraPosition.z, 1.0f };
        frameConstants.fogColor = { m_Sky.horizonColor.x, m_Sky.horizonColor.y, m_Sky.horizonColor.z, 1.0f };
        frameConstants.fogParams = { m_Fog.enabled ? m_Fog.density : 0.0f, 0.0f, 0.0f, 0.0f };

        DX12UploadBuffer& frameConstantBuffer = *m_FrameConstantBuffers[m_FrameIndex];
        frameConstantBuffer.Write(&frameConstants, sizeof(frameConstants));

        // 受け渡し棚の設計図を設定してから、b1 の箱に別紙の住所を入れる
        // (設計図を設定し直すと箱の中身は消えるため、設計図の設定もフレームの最初に1回だけ行う)
        commandList->SetGraphicsRootSignature(m_Pipeline->GetRootSignature());
        commandList->SetGraphicsRootConstantBufferView(FrameConstantsRootIndex, frameConstantBuffer.GetGpuAddress());

        // 4-7b: 空を、最初に、画面全体に描く(頂点バッファなし。三角形 1 つ = 頂点 3 つ)
        // 深度には何も書かないので、深度バッファは「一番奥」のまま。あとから描く地面や物が、空の上に上書きする
        commandList->SetPipelineState(m_SkyPipeline->Get());
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->DrawInstanced(3, 1, 0, 0);

        // ImGui I-5: このフレームの ImGui を始める。この後 OnImGui() で、Game が窓を書く
        m_ImGui->BeginFrame();
    }

    // 2-7 処理2: 形のデータを GPU のバッファに入れて一覧表に登録し、番号札を返す
    MeshHandle Renderer::CreateMesh(const MeshData& data)
    {
        if (data.vertices.empty() || data.indices.empty())
        {
            TFE_LOG_ERROR("CreateMesh: 頂点または番号が空です");
            return {};
        }
        if (data.vertices.size() > UINT32_MAX)
        {
            TFE_LOG_ERROR("CreateMesh: 頂点が多すぎます({} 個。uint32_t の番号で扱えるのは 約42億個まで)", data.vertices.size());
            return {};
        }

        const uint64_t vertexBytes = data.vertices.size() * sizeof(Vertex);
        const uint64_t indexBytes = data.indices.size() * sizeof(uint32_t);

        auto mesh = std::make_unique<GpuMesh>();
        mesh->vertexBuffer = std::make_unique<DX12UploadBuffer>(m_Device->Get(), vertexBytes);
        mesh->vertexBuffer->Write(data.vertices.data(), vertexBytes);
        mesh->indexBuffer = std::make_unique<DX12UploadBuffer>(m_Device->Get(), indexBytes);
        mesh->indexBuffer->Write(data.indices.data(), indexBytes);
        mesh->indexCount = static_cast<uint32_t>(data.indices.size());

        // 番号は 1 から順に発行し、使い回さない(捨てた番号札で別の形が描かれる事故を防ぐ)
        const MeshHandle handle{ m_NextMeshId++ };
        m_Meshes.emplace(handle.id, std::move(mesh));
        return handle;
    }

    // 2-7 処理3: 番号札はすぐ無効にし、実体は「捨てる予定リスト」へ(GPU が使い終わってから捨てる)
    void Renderer::DestroyMesh(MeshHandle mesh)
    {
        auto it = m_Meshes.find(mesh.id);
        if (it == m_Meshes.end())
        {
            TFE_LOG_WARN("DestroyMesh: 無効な番号札です (id {})", mesh.id);
            return;
        }

        // 引換券の番号(fenceValue)は、このフレームの EndFrame で Signal したときに決まる
        m_PendingReleases.push_back({ std::move(it->second), 0 });
        m_Meshes.erase(it);
    }

    void Renderer::ReleaseFinishedMeshes()
    {
        std::erase_if(m_PendingReleases, [this](const PendingRelease& pending)
        {
            return pending.fenceValue != 0 && m_Queue->IsComplete(pending.fenceValue);
        });
    }

    // 2-7 処理4: その場では描かず、依頼メモを描画リストに積むだけ
    void Renderer::DrawMesh(MeshHandle mesh, const XMMATRIX& world)
    {
        DrawCommand command;
        command.mesh = mesh;
        XMStoreFloat4x4(&command.world, world); // 計算用の XMMATRIX を、保存用の XMFLOAT4X4 に書き出す
        m_DrawList.push_back(command);
    }

    // 2-7 処理5: 描画リストの依頼を1つずつ GPU への命令にする
    void Renderer::ExecuteDrawList()
    {
        ID3D12GraphicsCommandList* commandList = m_CommandList->Get();

        // リスト全体で1回だけ: パイプラインと「番号を3つずつ区切って三角形にする」設定(Root Signature は BeginFrame で設定済み)
        commandList->SetPipelineState(m_Pipeline->Get());
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        uint32_t boundMeshId = 0; // 今、頂点バッファと番号バッファに設定している形
        for (const DrawCommand& command : m_DrawList)
        {
            // 番号札から形を探す。無効・捨てた後の番号札なら飛ばす(エラーは番号ごとに最初の1回だけ)
            auto it = m_Meshes.find(command.mesh.id);
            if (it == m_Meshes.end())
            {
                if (m_ReportedInvalidMeshes.insert(command.mesh.id).second)
                {
                    TFE_LOG_ERROR("DrawMesh: 無効な番号札です (id {})。この描画を飛ばします", command.mesh.id);
                }
                continue;
            }
            const GpuMesh& mesh = *it->second;

            // 直前と同じ形なら、頂点バッファと番号バッファの設定を省く
            if (command.mesh.id != boundMeshId)
            {
                // 頂点バッファの場所・全体の大きさ・1頂点の大きさを伝える
                D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};
                vertexBufferView.BufferLocation = mesh.vertexBuffer->GetGpuAddress();
                vertexBufferView.SizeInBytes = static_cast<UINT>(mesh.vertexBuffer->GetSize());
                vertexBufferView.StrideInBytes = sizeof(Vertex);
                commandList->IASetVertexBuffers(0, 1, &vertexBufferView);

                // 番号リストの場所・全体の大きさ・番号の型(uint32_t)を伝える
                D3D12_INDEX_BUFFER_VIEW indexBufferView = {};
                indexBufferView.BufferLocation = mesh.indexBuffer->GetGpuAddress();
                indexBufferView.SizeInBytes = static_cast<UINT>(mesh.indexBuffer->GetSize());
                indexBufferView.Format = DXGI_FORMAT_R32_UINT;
                commandList->IASetIndexBuffer(&indexBufferView);

                boundMeshId = command.mesh.id;
            }

            // ワールド行列(数字16個)を b0 の箱に入れて、描く
            commandList->SetGraphicsRoot32BitConstants(ObjectConstantsRootIndex, 16, &command.world, 0);
            commandList->DrawIndexedInstanced(mesh.indexCount, 1, 0, 0, 0);
        }

        m_DrawList.clear();
    }

    //フレーム終了
    void Renderer::EndFrame()
    {
        ExecuteDrawList();

        ID3D12GraphicsCommandList* commandList = m_CommandList->Get();

        // ImGui I-5: ゲームの絵の上に、Game が OnImGui() で書いた窓を貼る(バックバッファを表示用に戻す前)
        // I-6c: 描画先を、同じバックバッファの「変換なし」の見方(UNORM)・深度なしに切り替える(ImGui の色が明るくなりすぎないように)
        const D3D12_CPU_DESCRIPTOR_HANDLE linearRtv = m_SwapChain->GetCurrentLinearRenderTargetView();
        commandList->OMSetRenderTargets(1, &linearRtv, FALSE, nullptr);
        m_ImGui->Render(commandList);

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

        // 2-7 処理3: このフレームまでに捨てる予定になった形に、引換券の番号を書く
        // (このフレームの命令が GPU で終われば、それより前のフレームも終わっているので安全に捨てられる)
        for (PendingRelease& pending : m_PendingReleases)
        {
            if (pending.fenceValue == 0)
            {
                pending.fenceValue = m_FrameFenceValues[m_FrameIndex];
            }
        }

        m_FrameIndex = (m_FrameIndex + 1) % FramesInFlight;
    }
}
