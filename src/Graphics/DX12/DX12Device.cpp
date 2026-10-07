#include "Graphics/DX12/DX12Device.h"

#include <dxgidebug.h>

#include <vector>

namespace KR
{
    namespace
    {
        constexpr D3D_FEATURE_LEVEL MinFeatureLevel = D3D_FEATURE_LEVEL_12_0;

        // dxgidebug.h の DXGI_DEBUG_ALL と同じ値(dxguid.lib をリンクせずに済ませるため自前で定義)
        constexpr GUID DxgiDebugAll = { 0xe48ae283, 0xda80, 0x490b, { 0x87, 0xe6, 0x43, 0xe9, 0xa9, 0xcf, 0xda, 0x08 } };

        std::string ToUtf8(const wchar_t* wide)
        {
            const int length = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
            std::string utf8(length > 0 ? length - 1 : 0, '\0');
            WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8.data(), length, nullptr, nullptr);
            return utf8;
        }

        // デバッグレイヤーのメッセージをエンジンのログに流す(コンソールでも見えるようにするため)
        void CALLBACK OnDebugMessage(D3D12_MESSAGE_CATEGORY, D3D12_MESSAGE_SEVERITY severity,
                                     D3D12_MESSAGE_ID, LPCSTR description, void*)
        {
            switch (severity)
            {
            case D3D12_MESSAGE_SEVERITY_CORRUPTION:
            case D3D12_MESSAGE_SEVERITY_ERROR:
                KR_LOG_ERROR("[D3D12] {}", description);
                break;
            case D3D12_MESSAGE_SEVERITY_WARNING:
                KR_LOG_WARN("[D3D12] {}", description);
                break;
            default:
                break;
            }
        }
    }

    DX12Device::DX12Device()
    {
        EnableDebugLayer();

        UINT factoryFlags = 0;
#ifdef KR_DEBUG
        factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
        KR_DX_CHECK(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&m_Factory)));

        SelectAdapter();

        KR_DX_CHECK(D3D12CreateDevice(m_Adapter.Get(), MinFeatureLevel, IID_PPV_ARGS(&m_Device)));

        SetupDebugMessages();
    }

    DX12Device::~DX12Device()
    {
        ComPtr<ID3D12InfoQueue1> infoQueue;
        if (m_DebugCallbackCookie != 0 && SUCCEEDED(m_Device.As(&infoQueue)))
        {
            infoQueue->UnregisterMessageCallback(m_DebugCallbackCookie);
        }
    }

    // デバッグレイヤー: DX12 の使い方の間違いを検出して教えてくれる。重いので Debug ビルドのみ
    void DX12Device::EnableDebugLayer()
    {
#ifdef KR_DEBUG
        ComPtr<ID3D12Debug> debug;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
        {
            debug->EnableDebugLayer();
            KR_LOG_INFO("D3D12 デバッグレイヤー有効");
        }
        else
        {
            KR_LOG_WARN("D3D12 デバッグレイヤーを有効にできません(グラフィックスツールが未インストールの可能性)");
        }
#endif
    }

    // 高性能な GPU から順に調べ、DX12 に対応した最初のハードウェア GPU を使う
    void DX12Device::SelectAdapter()
    {
        ComPtr<IDXGIAdapter1> adapter;
        for (UINT i = 0;
             m_Factory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND;
             ++i)
        {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            // ソフトウェア実装(Microsoft Basic Render Driver)は除外する
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                continue;
            }

            // 最後の引数が nullptr の場合、デバイスは作らず「作れるかどうか」だけを確認する
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), MinFeatureLevel, __uuidof(ID3D12Device), nullptr)))
            {
                m_Adapter = adapter;
                KR_LOG_INFO("GPU: {} (VRAM {} MB)", ToUtf8(desc.Description), desc.DedicatedVideoMemory / (1024 * 1024));
                return;
            }
        }

        KR_LOG_FATAL("DirectX 12 に対応した GPU が見つかりません");
    }

    void DX12Device::SetupDebugMessages()
    {
#ifdef KR_DEBUG
        ComPtr<ID3D12InfoQueue> infoQueue;
        if (FAILED(m_Device.As(&infoQueue)))
        {
            return;
        }

        // デバッガー接続時はエラーの発生箇所で止める(未接続で止めるとクラッシュするため)
        if (IsDebuggerPresent())
        {
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
        }

        // ID3D12InfoQueue1 は Windows 11 以降。無い環境では VS の出力ウィンドウにだけ表示される
        ComPtr<ID3D12InfoQueue1> infoQueue1;
        if (SUCCEEDED(infoQueue.As(&infoQueue1)))
        {
            infoQueue1->RegisterMessageCallback(&OnDebugMessage, D3D12_MESSAGE_CALLBACK_FLAG_NONE,
                                                nullptr, &m_DebugCallbackCookie);
        }
#endif
    }

    void ReportLiveDX12Objects()
    {
#ifdef KR_DEBUG
        ComPtr<IDXGIDebug1> dxgiDebug;
        ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
        if (FAILED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))) ||
            FAILED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiInfoQueue))))
        {
            return;
        }

        dxgiInfoQueue->ClearStoredMessages(DxgiDebugAll);
        dxgiDebug->ReportLiveObjects(DxgiDebugAll,
            static_cast<DXGI_DEBUG_RLO_FLAGS>(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));

        // ReportLiveObjects の結果は InfoQueue に溜まるので、取り出してログに出す
        const UINT64 count = dxgiInfoQueue->GetNumStoredMessages(DxgiDebugAll);
        for (UINT64 i = 0; i < count; ++i)
        {
            SIZE_T size = 0;
            dxgiInfoQueue->GetMessage(DxgiDebugAll, i, nullptr, &size);
            std::vector<char> buffer(size);
            auto* message = reinterpret_cast<DXGI_INFO_QUEUE_MESSAGE*>(buffer.data());
            dxgiInfoQueue->GetMessage(DxgiDebugAll, i, message, &size);
            KR_LOG_WARN("[リーク] {}", std::string_view(message->pDescription, message->DescriptionByteLength));
        }

        if (count == 0)
        {
            KR_LOG_INFO("DX12 オブジェクトの解放漏れなし");
        }
#endif
    }
}
