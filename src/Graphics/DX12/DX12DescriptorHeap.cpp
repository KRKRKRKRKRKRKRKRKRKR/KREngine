#include "Graphics/DX12/DX12DescriptorHeap.h"

namespace KR
{
    DX12DescriptorHeap::DX12DescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t count, bool shaderVisible)
    {
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.Type = type;
        heapDesc.NumDescriptors = count;
        heapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        KR_DX_CHECK(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_Heap)));

        m_CpuStart = m_Heap->GetCPUDescriptorHandleForHeapStart();
        if (shaderVisible)
        {
            m_GpuStart = m_Heap->GetGPUDescriptorHandleForHeapStart();
        }
        m_DescriptorSize = device->GetDescriptorHandleIncrementSize(type);

        // 最初は全部の枠が空き。0 番から順に貸し出せるよう、大きい番号を先に並べる
        m_FreeIndices.reserve(count);
        for (uint32_t i = count; i > 0; --i)
        {
            m_FreeIndices.push_back(i - 1);
        }
    }

    bool DX12DescriptorHeap::Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* outCpu, D3D12_GPU_DESCRIPTOR_HANDLE* outGpu)
    {
        if (m_FreeIndices.empty())
        {
            KR_LOG_ERROR("DX12DescriptorHeap: 空いている枠がありません");
            return false;
        }

        const uint32_t index = m_FreeIndices.back();
        m_FreeIndices.pop_back();

        // 先頭の場所から、「枠の番号 × 枠1つ分の大きさ」だけ進んだ場所が、その枠
        outCpu->ptr = m_CpuStart.ptr + static_cast<SIZE_T>(index) * m_DescriptorSize;
        outGpu->ptr = m_GpuStart.ptr == 0 ? 0 : m_GpuStart.ptr + static_cast<UINT64>(index) * m_DescriptorSize;
        return true;
    }

    void DX12DescriptorHeap::Free(D3D12_CPU_DESCRIPTOR_HANDLE cpu)
    {
        // 場所から、枠の番号に戻す(Alloc の逆の計算)
        const auto index = static_cast<uint32_t>((cpu.ptr - m_CpuStart.ptr) / m_DescriptorSize);
        m_FreeIndices.push_back(index);
    }
}
