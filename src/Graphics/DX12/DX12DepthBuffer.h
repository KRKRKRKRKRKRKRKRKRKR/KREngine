#pragma once

#include "Graphics/DX12/DX12Common.h"

namespace TenFiftyEight
{
    // 深度の形式(PSO の DSVFormat と一致させる必要がある)
    constexpr DXGI_FORMAT DepthBufferFormat = DXGI_FORMAT_D32_FLOAT;

    // 深度の初期値。1.0 = 一番奥(まだ何も描かれていない = 無限に遠い)
    constexpr float DepthClearValue = 1.0f;

    // 画素ごとの奥行きを記録する画像(深度バッファ)
    // 構成はバックバッファと同じ: 画像そのもの + DSV ヒープ + DSV
    class DX12DepthBuffer
    {
    public:
        DX12DepthBuffer(ID3D12Device* device, uint32_t width, uint32_t height);

        DX12DepthBuffer(const DX12DepthBuffer&) = delete;
        DX12DepthBuffer& operator=(const DX12DepthBuffer&) = delete;

        // 呼ぶ前に、GPU が深度バッファを使い終わっていること(Flush 済み)
        void Resize(uint32_t width, uint32_t height);

        D3D12_CPU_DESCRIPTOR_HANDLE GetView() const { return m_DsvHeap->GetCPUDescriptorHandleForHeapStart(); }

    private:
        void CreateTexture(uint32_t width, uint32_t height);

        ID3D12Device* m_Device = nullptr; // 所有しない(DX12Device が持つ)
        ComPtr<ID3D12DescriptorHeap> m_DsvHeap;
        ComPtr<ID3D12Resource> m_Texture;
        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
    };
}
