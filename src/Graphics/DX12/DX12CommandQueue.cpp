#include "Graphics/DX12/DX12CommandQueue.h"

namespace KR
{
    DX12CommandQueue::DX12CommandQueue(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type)
    {
        D3D12_COMMAND_QUEUE_DESC desc = {};
        desc.Type = type;
        desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        KR_DX_CHECK(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_Queue)));

        KR_DX_CHECK(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence)));
    }

    void DX12CommandQueue::Execute(ID3D12CommandList* commandList)
    {
        m_Queue->ExecuteCommandLists(1, &commandList);
    }

    uint64_t DX12CommandQueue::Signal()
    {
        const uint64_t value = m_NextFenceValue++;
        KR_DX_CHECK(m_Queue->Signal(m_Fence.Get(), value));
        return value;
    }

    bool DX12CommandQueue::IsComplete(uint64_t fenceValue) const
    {
        return m_Fence->GetCompletedValue() >= fenceValue;
    }

    void DX12CommandQueue::WaitForValue(uint64_t fenceValue)
    {
        if (IsComplete(fenceValue))
        {
            return;
        }
        // イベントに nullptr を渡すと、Fence が指定値に達するまでこの関数がブロックする
        KR_DX_CHECK(m_Fence->SetEventOnCompletion(fenceValue, nullptr));
    }

    void DX12CommandQueue::Flush()
    {
        WaitForValue(Signal());
    }
}
