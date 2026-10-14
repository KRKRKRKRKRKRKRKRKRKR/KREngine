// KR エンジンの、最小のサンプル:ウィンドウを出して、6 色のキューブを回して、ImGui で FPS と入力の状態を出す
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

        // 入力の見本:押した・離したキーとボタンを、ログに出す(全部のキーを、順に調べる)
        for (int i = 0; i < static_cast<int>(KR::Key::Count); ++i)
        {
            const auto key = static_cast<KR::Key>(i);
            if (KR::Input::IsKeyPressed(key))
            {
                KR_LOG_INFO("押した: {}", KR::Input::GetKeyName(key));
            }
            if (KR::Input::IsKeyReleased(key))
            {
                KR_LOG_INFO("離した: {}", KR::Input::GetKeyName(key));
            }
        }
        if (KR::Input::IsKeyPressed(KR::Key::MouseLeft))
        {
            const KR::MousePosition mouse = KR::Input::GetMousePosition();
            KR_LOG_INFO("左クリックの位置: ({}, {})", mouse.x, mouse.y);
        }
        // ホイールは、このフレームの量なので、表示用に足していく
        const float wheel = KR::Input::GetMouseWheel();
        if (wheel != 0.0f)
        {
            KR_LOG_INFO("ホイール: {}", wheel);
        }
        m_WheelTotal += wheel;
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

        // 入力の見本:マウスの位置・ホイールの合計・今押しているキー
        const KR::MousePosition mouse = KR::Input::GetMousePosition();
        ImGui::Text("Mouse: (%.0f, %.0f)  Wheel: %.1f", mouse.x, mouse.y, m_WheelTotal);
        ImGui::Text("Down:");
        for (int i = 0; i < static_cast<int>(KR::Key::Count); ++i)
        {
            const auto key = static_cast<KR::Key>(i);
            if (KR::Input::IsKeyDown(key))
            {
                ImGui::SameLine();
                ImGui::Text("%s", KR::Input::GetKeyName(key));
            }
        }
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
    float m_WheelTotal = 0.0f; // ホイールを回した量の合計(表示用)
};

// エンジンの main() が呼ぶ。作ったゲームを返す
std::unique_ptr<KR::Application> KR::CreateApplication()
{
    return std::make_unique<MinimalApp>();
}
