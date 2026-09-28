#include "Graphics/DX12/DX12DepthBuffer.h"

namespace TenFiftyEight
{
    DX12DepthBuffer::DX12DepthBuffer(ID3D12Device* device, uint32_t width, uint32_t height)
        : m_Device(device)
    {
        // DSV を置くためのヒープ。深度バッファは1枚なので枠は1つ
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        heapDesc.NumDescriptors = 1;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        TFE_DX_CHECK(m_Device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_DsvHeap)));

        CreateTexture(width, height);
    }

    void DX12DepthBuffer::Resize(uint32_t width, uint32_t height)
    {
        if (width == m_Width && height == m_Height)
        {
            return;
        }
        // SwapChain と違い自分で作ったリソースなので、古いものを捨てて作り直す(ヒープの枠は使い回す)
        m_Texture.Reset();
        CreateTexture(width, height);
    }

    void DX12DepthBuffer::CreateTexture(uint32_t width, uint32_t height)
    {
        m_Width = width;
        m_Height = height;

        // Default ヒープ = GPU 専用メモリ(VRAM)。深度は GPU だけが読み書きするので、ここが最速
        const CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);

        // 画面と同じ大きさの2D画像。深度として使うことを許可するフラグを付ける
        const CD3DX12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
            DepthBufferFormat, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

        // よく使うクリアの値を作成時に伝えておくと、GPU がクリアを速く処理できる
        D3D12_CLEAR_VALUE clearValue = {};
        clearValue.Format = DepthBufferFormat;
        clearValue.DepthStencil.Depth = DepthClearValue;

        TFE_DX_CHECK(m_Device->CreateCommittedResource(
            &heapProperties, D3D12_HEAP_FLAG_NONE, &textureDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue, IID_PPV_ARGS(&m_Texture)));

        // 「この画像に奥行きを書け」という情報(DSV)をヒープの枠に作る
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DepthBufferFormat;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        m_Device->CreateDepthStencilView(m_Texture.Get(), &dsvDesc, m_DsvHeap->GetCPUDescriptorHandleForHeapStart());
    }
}
