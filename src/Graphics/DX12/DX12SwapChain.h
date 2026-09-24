#pragma once

#include "Graphics/DX12/DX12Common.h"

#include <array>

namespace TenFiftyEight
{
    struct DX12SwapChainDesc
    {
        IDXGIFactory6* factory = nullptr;
        ID3D12Device* device = nullptr;
        ID3D12CommandQueue* queue = nullptr; // DX12 の SwapChain は Device ではなく Queue に結びつく
        HWND window = nullptr;
        uint32_t width = 0;
        uint32_t height = 0;
    };

    // 画面に表示する画像(バックバッファ)を2枚持ち、描画用と表示用を交互に入れ替える
    class DX12SwapChain
    {
    public:
        explicit DX12SwapChain(const DX12SwapChainDesc& desc);

        DX12SwapChain(const DX12SwapChain&) = delete;
        DX12SwapChain& operator=(const DX12SwapChain&) = delete;

        void Present(bool vsync);

        // 呼ぶ前に、GPU がバックバッファを使い終わっていること(Flush 済み)
        void Resize(uint32_t width, uint32_t height);

        ID3D12Resource* GetCurrentBackBuffer() const;
        D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRenderTargetView() const;

    private:
        void CreateRenderTargetViews();
        UINT GetSwapChainFlags() const;

        ID3D12Device* m_Device = nullptr; // 所有しない(DX12Device が持つ)
        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
        ComPtr<IDXGISwapChain3> m_SwapChain;
        ComPtr<ID3D12DescriptorHeap> m_RtvHeap;
        uint32_t m_RtvDescriptorSize = 0;
        std::array<ComPtr<ID3D12Resource>, BackBufferCount> m_BackBuffers;
        bool m_TearingSupported = false;
    };
}
