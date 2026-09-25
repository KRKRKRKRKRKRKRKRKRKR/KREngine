#pragma once

#include "Graphics/DX12/DX12Common.h"

namespace TenFiftyEight
{
    // Upload ヒープ(CPU から書き込めるメモリ)に置くバッファ
    // 頂点バッファ・インデックスバッファ・定数バッファのどれにも使える
    class DX12UploadBuffer
    {
    public:
        DX12UploadBuffer(ID3D12Device* device, uint64_t size);
        ~DX12UploadBuffer();

        DX12UploadBuffer(const DX12UploadBuffer&) = delete;
        DX12UploadBuffer& operator=(const DX12UploadBuffer&) = delete;

        // GPU がこの範囲を読んでいる最中に書き換えないこと
        void Write(const void* data, uint64_t size, uint64_t offset = 0);

        D3D12_GPU_VIRTUAL_ADDRESS GetGpuAddress() const { return m_Resource->GetGPUVirtualAddress(); }
        uint64_t GetSize() const { return m_Size; }

    private:
        ComPtr<ID3D12Resource> m_Resource;
        uint8_t* m_MappedData = nullptr; // CPU から見たバッファの先頭アドレス(作成時から開きっぱなしにする)
        uint64_t m_Size = 0;
    };
}
