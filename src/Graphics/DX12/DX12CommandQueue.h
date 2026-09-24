#pragma once

#include "Graphics/DX12/DX12Common.h"

namespace TenFiftyEight
{
    // GPU へ命令を提出する窓口と、GPU の進み具合を知るための Fence をまとめたクラス
    //
    // Fence の考え方:
    //   CPU は Signal() で「ここまで終わったら値 N にして」と GPU に予約する。
    //   GPU がその地点まで処理すると Fence の値が N になる。
    //   CPU は Fence の値を見て「N まで終わったか」を判断したり、待ったりできる。
    class DX12CommandQueue
    {
    public:
        DX12CommandQueue(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type);

        DX12CommandQueue(const DX12CommandQueue&) = delete;
        DX12CommandQueue& operator=(const DX12CommandQueue&) = delete;

        void Execute(ID3D12CommandList* commandList);

        // これまでに提出した命令がすべて終わったら到達する Fence 値を返す
        uint64_t Signal();

        bool IsComplete(uint64_t fenceValue) const;
        void WaitForValue(uint64_t fenceValue);

        // 提出済みの命令がすべて終わるまで待つ(リサイズ・終了時に使う)
        void Flush();

        ID3D12CommandQueue* Get() const { return m_Queue.Get(); }

    private:
        ComPtr<ID3D12CommandQueue> m_Queue;
        ComPtr<ID3D12Fence> m_Fence;
        uint64_t m_NextFenceValue = 1;
    };
}
