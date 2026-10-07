#include "Graphics/DX12/DX12CommandList.h"

namespace KR
{
    DX12CommandList::DX12CommandList(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type, uint32_t allocatorCount)
        : m_Allocators(allocatorCount)
    {
        for (ComPtr<ID3D12CommandAllocator>& allocator : m_Allocators)
        {
            KR_DX_CHECK(device->CreateCommandAllocator(type, IID_PPV_ARGS(&allocator)));
        }

        KR_DX_CHECK(device->CreateCommandList(0, type, m_Allocators[0].Get(), nullptr, IID_PPV_ARGS(&m_List)));

        // 作成直後は「記録中」の状態なので、いったん閉じて Begin() から使える状態にそろえる
        KR_DX_CHECK(m_List->Close());
    }

    ID3D12GraphicsCommandList* DX12CommandList::Begin(uint32_t allocatorIndex)
    {
        ID3D12CommandAllocator* allocator = m_Allocators[allocatorIndex].Get();
        KR_DX_CHECK(allocator->Reset());
        KR_DX_CHECK(m_List->Reset(allocator, nullptr));
        return m_List.Get();
    }

    void DX12CommandList::End()
    {
        KR_DX_CHECK(m_List->Close());
    }
}
