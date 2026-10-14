#pragma once

#include "Graphics/DX12/DX12Common.h"

namespace KR
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

        // 毎フレームの最初(Renderer::BeginFrame の最後)に呼ぶ。この後 ImGui:: の関数で窓を書ける
        void BeginFrame();

        // 毎フレームの最後(ゲームの絵を描いた後)に呼ぶ。書かれた窓を、命令リストに積んで画面に貼る
        void Render(ID3D12GraphicsCommandList* commandList);

        // ImGui が今、キーボードを使っているか(文字入力欄に打っているときなど)。true の間は、ゲームにキーを渡さない
        // (値は NewFrame のたびに更新されるので、キーを押した時点では、1フレーム前の状態)
        bool WantsKeyboard() const;

        // ImGui が今、マウスを使っているか(マウスが窓の上にある・窓をドラッグしているときなど)。true の間は、ゲームにクリックを渡さない
        // (WantsKeyboard と同じく、1フレーム前の状態)
        bool WantsMouse() const;

        // OS のメッセージ(マウス・キー・文字入力)を ImGui に教える。ImGui が処理したら true
        static bool HandleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

    private:
        ID3D12DescriptorHeap* m_SrvHeap = nullptr; // 描画の前に、命令リストへ設定する(所有しない)
    };
}
