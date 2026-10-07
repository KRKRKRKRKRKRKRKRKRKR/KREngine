#pragma once

#include "Graphics/DX12/DX12Common.h"

#include <memory>

namespace KR
{
    class DX12CommandList;
    class DX12CommandQueue;
    class DX12DescriptorHeap;

    // 画像(テクスチャ)。GPU 専用のメモリ(Default ヒープ)に置き、シェーダーが読むための札(SRV)を、札掛けの枠に掛ける
    //
    // 画像は大きく(player.png で約 4 MB)、毎フレーム何度も読み、書き換えない。だから、CPU が書き込める遅いメモリ(Upload ヒープ)ではなく、
    // GPU 専用の速いメモリ(Default ヒープ)に置く。作るときだけ、Upload ヒープ経由でコピーする
    class DX12Texture
    {
    public:
        // RGBA8(1 ピクセル 4 バイト。左上から、右へ・下へ)の画像から作る。コピーが終わるまで待つ(起動時の読み込み用)
        // copyList: コピー命令を書く命令リスト(使うのは、番号 0 の Allocator)
        // 失敗したら nullptr
        static std::unique_ptr<DX12Texture> Create(ID3D12Device* device, DX12CommandList& copyList, DX12CommandQueue& queue,
                                                   DX12DescriptorHeap& srvHeap, uint32_t width, uint32_t height, const uint8_t* rgbaPixels);

        // 札掛けの枠を返す(GPU が使い終わってから呼ぶこと。Renderer の遅延削除が、それを守る)
        ~DX12Texture();

        DX12Texture(const DX12Texture&) = delete;
        DX12Texture& operator=(const DX12Texture&) = delete;

        // シェーダーが、この画像を読むための札(SRV)の場所(GPU 用)。Root Signature の「画像のテーブル」に設定する
        D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle() const { return m_SrvGpu; }

    private:
        DX12Texture() = default;

        ComPtr<ID3D12Resource> m_Resource;
        DX12DescriptorHeap* m_SrvHeap = nullptr;       // 枠を返す先(所有しない)
        D3D12_CPU_DESCRIPTOR_HANDLE m_SrvCpu = {};     // 枠の場所(CPU 用。札を書く・枠を返すときに使う)
        D3D12_GPU_DESCRIPTOR_HANDLE m_SrvGpu = {};     // 枠の場所(GPU 用)
    };
}
