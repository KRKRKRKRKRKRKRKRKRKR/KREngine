#include "Graphics/DX12/DX12SwapChain.h"

namespace TenFiftyEight
{
    namespace
    {
        // Flip Model の SwapChain は _SRGB 形式で作れないため、バッファは通常形式で作り、
        // RTV(描画するときの見方)だけを _SRGB にする
        constexpr DXGI_FORMAT BackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

        // VSync オフ時にティアリングを許可できるか(可変リフレッシュレートのモニターにも必要)
        bool CheckTearingSupport(IDXGIFactory6* factory)
        {
            BOOL allowTearing = FALSE;
            if (FAILED(factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing))))
            {
                return false;
            }
            return allowTearing == TRUE;
        }
    }

    DX12SwapChain::DX12SwapChain(const DX12SwapChainDesc& desc)
        : m_Device(desc.device)
        , m_Width(desc.width)
        , m_Height(desc.height)
        , m_TearingSupported(CheckTearingSupport(desc.factory))
    {
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.Width = desc.width;
        swapChainDesc.Height = desc.height;
        swapChainDesc.Format = BackBufferFormat;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = BackBufferCount;
        swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // Flip Model(DX12 ではこれが必須)
        swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        swapChainDesc.Flags = GetSwapChainFlags();

        ComPtr<IDXGISwapChain1> swapChain1;
        TFE_DX_CHECK(desc.factory->CreateSwapChainForHwnd(
            desc.queue, desc.window, &swapChainDesc, nullptr, nullptr, &swapChain1));

        // IDXGISwapChain3 にすると GetCurrentBackBufferIndex() が使える
        TFE_DX_CHECK(swapChain1.As(&m_SwapChain));

        // DXGI による Alt+Enter の自動フルスクリーン切り替えを止める
        TFE_DX_CHECK(desc.factory->MakeWindowAssociation(desc.window, DXGI_MWA_NO_ALT_ENTER));

        // RTV(Render Target View)を置くためのヒープ。バックバッファの枚数分の枠を確保する
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        heapDesc.NumDescriptors = BackBufferCount;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        TFE_DX_CHECK(m_Device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_RtvHeap)));

        // ヒープ内の1枠のサイズは GPU によって違うため、問い合わせる
        m_RtvDescriptorSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        CreateRenderTargetViews();

        TFE_LOG_INFO("SwapChain 作成 {}x{} (ティアリング対応: {})", desc.width, desc.height, m_TearingSupported);
    }

    void DX12SwapChain::CreateRenderTargetViews()
    {
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = RenderTargetFormat;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_RtvHeap->GetCPUDescriptorHandleForHeapStart());
        for (uint32_t i = 0; i < BackBufferCount; ++i)
        {
            TFE_DX_CHECK(m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&m_BackBuffers[i])));
            m_Device->CreateRenderTargetView(m_BackBuffers[i].Get(), &rtvDesc, rtvHandle);
            rtvHandle.Offset(1, m_RtvDescriptorSize);
        }
    }

    UINT DX12SwapChain::GetSwapChainFlags() const
    {
        // 作成時と ResizeBuffers 時で同じフラグを渡す必要がある
        return m_TearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
    }

    void DX12SwapChain::Resize(uint32_t width, uint32_t height)
    {
        if (width == m_Width && height == m_Height)
        {
            return;
        }
        m_Width = width;
        m_Height = height;

        // バックバッファへの参照が1つでも残っていると ResizeBuffers は失敗する
        for (ComPtr<ID3D12Resource>& backBuffer : m_BackBuffers)
        {
            backBuffer.Reset();
        }

        TFE_DX_CHECK(m_SwapChain->ResizeBuffers(BackBufferCount, width, height, BackBufferFormat, GetSwapChainFlags()));

        // 新しいバックバッファに対して RTV を作り直す(ヒープの枠は使い回す)
        CreateRenderTargetViews();

        TFE_LOG_TRACE("SwapChain リサイズ {}x{}", width, height);
    }

    void DX12SwapChain::Present(bool vsync)
    {
        // syncInterval 1 = 次の垂直同期まで待つ / 0 = すぐ表示する
        const UINT syncInterval = vsync ? 1 : 0;
        const UINT flags = (!vsync && m_TearingSupported) ? DXGI_PRESENT_ALLOW_TEARING : 0;
        TFE_DX_CHECK(m_SwapChain->Present(syncInterval, flags));
    }

    ID3D12Resource* DX12SwapChain::GetCurrentBackBuffer() const
    {
        return m_BackBuffers[m_SwapChain->GetCurrentBackBufferIndex()].Get();
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DX12SwapChain::GetCurrentRenderTargetView() const
    {
        return CD3DX12_CPU_DESCRIPTOR_HANDLE(
            m_RtvHeap->GetCPUDescriptorHandleForHeapStart(),
            static_cast<INT>(m_SwapChain->GetCurrentBackBufferIndex()),
            m_RtvDescriptorSize);
    }
}
