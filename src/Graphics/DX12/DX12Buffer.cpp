#include "Graphics/DX12/DX12Buffer.h"

#include <cstring>

namespace KR
{
    DX12UploadBuffer::DX12UploadBuffer(ID3D12Device* device, uint64_t size)
        : m_Size(size)
    {
        const CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_UPLOAD);
        const CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(size);

        // Upload ヒープのリソースは、作成時から最後まで GENERIC_READ 状態で使う決まり
        TFE_DX_CHECK(device->CreateCommittedResource(
            &heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_Resource)));

        // Map: GPU のバッファを CPU から書き込めるアドレスとして開く
        // 読み込みはしないので、読む範囲は空({0, 0})を指定する
        const D3D12_RANGE readRange = { 0, 0 };
        TFE_DX_CHECK(m_Resource->Map(0, &readRange, reinterpret_cast<void**>(&m_MappedData)));
    }

    DX12UploadBuffer::~DX12UploadBuffer()
    {
        m_Resource->Unmap(0, nullptr);
    }

    void DX12UploadBuffer::Write(const void* data, uint64_t size, uint64_t offset)
    {
        if (offset + size > m_Size)
        {
            TFE_LOG_FATAL("バッファの範囲外への書き込み (offset {} + size {} > {})", offset, size, m_Size);
        }
        std::memcpy(m_MappedData + offset, data, size);
    }
}
