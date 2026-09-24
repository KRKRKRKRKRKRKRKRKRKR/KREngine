#pragma once

#include "Graphics/DX12/DX12Common.h"

#include <vector>

namespace TenFiftyEight
{
    // GPU への命令を記録するクラス
    //
    // Command List : 命令を書き込む「ペン」。記録 → Close → 提出 → Reset で使い回せる
    // Allocator    : 命令の実体を保存する「メモ帳」。GPU が読み終わるまでリセットできない
    //                → フレームインフライトの数だけ用意し、フレームごとに切り替える
    class DX12CommandList
    {
    public:
        DX12CommandList(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type, uint32_t allocatorCount);

        DX12CommandList(const DX12CommandList&) = delete;
        DX12CommandList& operator=(const DX12CommandList&) = delete;

        // 呼ぶ前に、allocatorIndex の Allocator を GPU が使い終わっていること
        ID3D12GraphicsCommandList* Begin(uint32_t allocatorIndex);
        void End();

        ID3D12GraphicsCommandList* Get() const { return m_List.Get(); }

    private:
        std::vector<ComPtr<ID3D12CommandAllocator>> m_Allocators;
        ComPtr<ID3D12GraphicsCommandList> m_List;
    };
}
