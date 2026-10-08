// KR エンジンの、最小のサンプル:ウィンドウを出して、6 色のキューブを回して、ImGui で FPS を出す
// 新しいゲームを作るときの、雛形にもなる(詳しい使い方は、docs/Usage.md)
//
// ゲームの作り方(3 つだけ):
//   ① KR::Application を継承したクラスを作る
//   ② OnInit(最初に 1 回)・OnUpdate(毎フレーム。動かす)・OnRender(毎フレーム。描く)・OnImGui(デバッグの窓)・OnShutdown(最後に 1 回)を書く
//   ③ KR::CreateApplication() を書いて、①のクラスを返す(main は、エンジンが持っている)

#include <KR/KR.h>

#include <imgui.h>

#include <memory>
#include <numbers>

using namespace DirectX;

class MinimalApp : public KR::Application
{
public:
    // ウィンドウのタイトルを渡す(渡さなければ "KR Engine")
    MinimalApp() : Application(KR::ApplicationDesc{ .title = "KR Engine - Minimal" }) {}

protected:
    void OnInit() override
    {
        // 形を作って、番号札をもらう(キューブのデータは、エンジンが、用意している)
        m_Cube = GetRenderer().CreateMesh(KR::CreateCubeMeshData());
    }

    void OnUpdate(float deltaTime) override
    {
        // 1 秒あたり 1 ラジアンで回す。deltaTime(前のフレームからの秒数)を掛けるので、どのモニターでも、同じ速さ
        m_Angle += deltaTime;
        constexpr float FullTurn = 2.0f * std::numbers::pi_v<float>;
        if (m_Angle >= FullTurn)
        {
            m_Angle -= FullTurn; // 数字が大きくなり続けて、精度が落ちるのを防ぐ
        }
    }

    void OnRender() override
    {
        // 「この形を、この行列で、描いて」という依頼を、描画リストに積む(実際に描くのは、フレームの最後)
        GetRenderer().DrawMesh(m_Cube, XMMatrixRotationY(m_Angle) * XMMatrixRotationX(m_Angle * 0.5f));
    }

    void OnImGui() override
    {
        const ImGuiIO& io = ImGui::GetIO();
        ImGui::Begin("Minimal");
        ImGui::Text("FPS: %.1f  (%.2f ms)", io.Framerate, 1000.0f / io.Framerate);
        ImGui::End();
    }

    void OnShutdown() override
    {
        // 使い終わった番号札を返す
        GetRenderer().DestroyMesh(m_Cube);
    }

private:
    KR::MeshHandle m_Cube; // 6 色のキューブの番号札
    float m_Angle = 0.0f;  // 回転の角度(ラジアン)
};

// エンジンの main() が呼ぶ。作ったゲームを返す
std::unique_ptr<KR::Application> KR::CreateApplication()
{
    return std::make_unique<MinimalApp>();
}
