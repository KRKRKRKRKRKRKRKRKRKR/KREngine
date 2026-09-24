#pragma once

#include "Graphics/DX12/DX12Common.h"

#include <string>

namespace TenFiftyEight
{
    // GPU の選択と ID3D12Device の作成を担当する
    class DX12Device
    {
    public:
        DX12Device();
        ~DX12Device();

        DX12Device(const DX12Device&) = delete;
        DX12Device& operator=(const DX12Device&) = delete;

        ID3D12Device* Get() const { return m_Device.Get(); }
        IDXGIFactory6* GetFactory() const { return m_Factory.Get(); }

    private:
        void EnableDebugLayer();
        void SelectAdapter();
        void SetupDebugMessages();

        ComPtr<IDXGIFactory6> m_Factory;
        ComPtr<IDXGIAdapter1> m_Adapter;
        ComPtr<ID3D12Device> m_Device;
        DWORD m_DebugCallbackCookie = 0;
    };

    // 解放されずに残っている DXGI / D3D12 オブジェクトをログに出す(Debug ビルドのみ)
    // すべての DX12 オブジェクトを解放した後に呼ぶ
    void ReportLiveDX12Objects();
}
