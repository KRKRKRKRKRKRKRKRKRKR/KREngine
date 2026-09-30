#pragma once

#include "Graphics/DX12/DX12Common.h"

#include <vector>

namespace TenFiftyEight
{
    // 「札掛け」(ディスクリプタヒープ)と、枠の貸し借り。
    // 札(ディスクリプタ)= 「この画像をこう読め」「この画像にこう描け」という情報。ヒープはその札を並べて掛けておく棚
    // ここでは主に、シェーダーが読む画像の札(SRV)を掛けるための、シェーダーから見える(shader-visible)ヒープに使う
    class DX12DescriptorHeap
    {
    public:
        DX12DescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t count, bool shaderVisible);

        DX12DescriptorHeap(const DX12DescriptorHeap&) = delete;
        DX12DescriptorHeap& operator=(const DX12DescriptorHeap&) = delete;

        ID3D12DescriptorHeap* Get() const { return m_Heap.Get(); }

        // 枠を1つ借りる。札を作る場所(CPU 用)と、シェーダーが札を読む場所(GPU 用)を返す。空きがなければ false
        // (shader-visible でないヒープでは、GPU 用の場所は 0)
        bool Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* outCpu, D3D12_GPU_DESCRIPTOR_HANDLE* outGpu);

        // 借りた枠を返す(Alloc で受け取った CPU 用の場所を渡す)。返した枠は、次の Alloc で使い回される
        void Free(D3D12_CPU_DESCRIPTOR_HANDLE cpu);

    private:
        ComPtr<ID3D12DescriptorHeap> m_Heap;
        D3D12_CPU_DESCRIPTOR_HANDLE m_CpuStart = {};
        D3D12_GPU_DESCRIPTOR_HANDLE m_GpuStart = {};
        uint32_t m_DescriptorSize = 0;       // 枠1つ分の大きさ(バイト)
        std::vector<uint32_t> m_FreeIndices; // 空いている枠の番号(末尾から借りる)
    };
}
