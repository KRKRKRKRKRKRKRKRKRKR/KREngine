#pragma once

#include "Graphics/DX12/DX12Common.h"

namespace TenFiftyEight
{
    class DX12DescriptorHeap;

    // ImGui(デバッグ用の画面)を、動かせる状態にする担当。準備と片付けを受け持つ
    // Renderer が持つ。作る順番は「SRV ヒープの後」、捨てる順番は「SRV ヒープの前」(ImGui が枠を借りているため)
    class ImGuiLayer
    {
    public:
        ImGuiLayer(HWND window, ID3D12Device* device, ID3D12CommandQueue* queue, DX12DescriptorHeap* srvHeap,
                   uint32_t framesInFlight, DXGI_FORMAT renderTargetFormat, DXGI_FORMAT depthFormat);
        ~ImGuiLayer();

        ImGuiLayer(const ImGuiLayer&) = delete;
        ImGuiLayer& operator=(const ImGuiLayer&) = delete;
    };
}
