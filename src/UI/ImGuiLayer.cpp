#include "UI/ImGuiLayer.h"

#include "Graphics/DX12/DX12DescriptorHeap.h"

#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>

#include <filesystem>

// imgui_impl_win32.h には宣言だけ(#if 0 の中)なので、ここで宣言する
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace KR
{
    namespace
    {
        // 日本語を表示するためのフォント(Windows に入っているメイリオ)。別のフォントにしたいときは、ここを直す
        constexpr const char* JapaneseFontFile = "meiryo.ttc";
        constexpr float FontSizePixels = 18.0f; // 画面の拡大率が 100% のときの文字の大きさ(画素)

        // ImGui に渡す「SRV の枠の借り方・返し方」。DX12DescriptorHeap の Alloc / Free につなぐだけ
        // ImGui は関数のポインタしか受け取れないので、ヒープは InitInfo の UserData から取り出す
        void AllocateSrv(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* outCpu, D3D12_GPU_DESCRIPTOR_HANDLE* outGpu)
        {
            auto* heap = static_cast<DX12DescriptorHeap*>(info->UserData);
            heap->Alloc(outCpu, outGpu);
        }

        void FreeSrv(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE /*gpu*/)
        {
            auto* heap = static_cast<DX12DescriptorHeap*>(info->UserData);
            heap->Free(cpu);
        }
    }

    ImGuiLayer::ImGuiLayer(HWND window, ID3D12Device* device, ID3D12CommandQueue* queue, DX12DescriptorHeap* srvHeap,
                           uint32_t framesInFlight, DXGI_FORMAT renderTargetFormat, DXGI_FORMAT depthFormat)
        : m_SrvHeap(srvHeap->Get())
    {
        // ① ImGui の作業場を作る
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // 窓をタブでまとめたり、端にくっつけたりできるようにする
        ImGui::StyleColorsDark();

        // ② ウィンドウ・マウス・キーボードとつなぐ
        if (!ImGui_ImplWin32_Init(window))
        {
            TFE_LOG_FATAL("ImGui_ImplWin32_Init に失敗しました");
        }

        // I-6b 画面の拡大率(DPI)に合わせて、文字と余白の大きさを変える(拡大率が高いモニターで、小さくなりすぎないように)
        const float dpiScale = ImGui_ImplWin32_GetDpiScaleForHwnd(window);
        ImGuiStyle& style = ImGui::GetStyle();
        style.ScaleAllSizes(dpiScale); // 余白・角の丸みなど
        style.FontScaleDpi = dpiScale; // 文字

        // I-6b 日本語のフォントを読み込む。Windows のフォルダーの場所は調べる(C:\Windows と決め打ちしない)
        // 使う文字は、画面に出るときに ImGui が必要な分だけフォントから読み込む(範囲の指定は要らない)
        wchar_t windowsDirectory[MAX_PATH] = {};
        GetWindowsDirectoryW(windowsDirectory, MAX_PATH);
        const std::filesystem::path fontPath = std::filesystem::path(windowsDirectory) / "Fonts" / JapaneseFontFile;
        if (std::filesystem::exists(fontPath))
        {
            io.Fonts->AddFontFromFileTTF(fontPath.string().c_str(), FontSizePixels);
        }
        else
        {
            TFE_LOG_WARN("日本語のフォントが見つかりません: {}(標準のフォントのままにします。日本語は表示できません)", fontPath.string());
        }

        // ③ 描画の道具を渡す(Device・Queue・フレーム数・描画先の形式・SRV ヒープと、枠の借り方・返し方)
        ImGui_ImplDX12_InitInfo initInfo;
        initInfo.Device = device;
        initInfo.CommandQueue = queue;
        initInfo.NumFramesInFlight = static_cast<int>(framesInFlight);
        initInfo.RTVFormat = renderTargetFormat;
        initInfo.DSVFormat = depthFormat;
        initInfo.UserData = srvHeap;
        initInfo.SrvDescriptorHeap = srvHeap->Get();
        initInfo.SrvDescriptorAllocFn = AllocateSrv;
        initInfo.SrvDescriptorFreeFn = FreeSrv;
        if (!ImGui_ImplDX12_Init(&initInfo))
        {
            TFE_LOG_FATAL("ImGui_ImplDX12_Init に失敗しました");
        }

        TFE_LOG_INFO("ImGui 初期化 (v{})", IMGUI_VERSION);
    }

    void ImGuiLayer::BeginFrame()
    {
        // DX12 → Win32 → ImGui の順(この順番で呼ぶ決まり)。この後、ImGui:: の関数で窓を書ける
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiLayer::Render(ID3D12GraphicsCommandList* commandList)
    {
        // 書かれた窓を、描画用のデータ(三角形の集まり)にまとめる
        ImGui::Render();

        // ImGui のフォント画像を読むため、命令リストに SRV ヒープ(札掛け)を設定してから描く
        ID3D12DescriptorHeap* heaps[] = { m_SrvHeap };
        commandList->SetDescriptorHeaps(1, heaps);
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
    }

    bool ImGuiLayer::WantsKeyboard() const
    {
        return ImGui::GetIO().WantCaptureKeyboard;
    }

    bool ImGuiLayer::HandleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        return ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam) != 0;
    }

    ImGuiLayer::~ImGuiLayer()
    {
        // 初期化と逆の順番で片付ける(DX12 の部分は、借りた SRV の枠を返しに行くので、ヒープが生きているうちに)
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }
}
