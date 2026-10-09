// KR エンジンのサンプル:敵の行動(KR::Behavior)の、使い方の見本
// ツール KRBehaviorEditor で作ったグラフ(Assets/Enemies/Walker.json)を読んで、敵 1 体を動かす
//   エンジン = 「今の状態」を決める(待機・巡回・追う・攻撃 …)/ ゲーム = その状態を見て、動く(この見本の、動く部分)
//   エンジンは、物理・移動・当たり判定を持たない。ゲームが、状況(距離など)を Context に入れ、返された状態に従って、自分で動く
//
// 操作:上の窓のスライダーで、プレイヤーの位置を動かす。下の窓に、グラフが出て、今の状態のノードが光る
// ファイル(Debug では、元の Assets/Enemies/Walker.json)を、ツールで直して保存すると、再起動なしで、反映される(GraphAsset)

#include <KR/KR.h>
#include <KR/Behavior/Context.h>
#include <KR/Behavior/GraphAsset.h>
#include <KR/Behavior/GraphDebug.h>
#include <KR/Behavior/Runner.h>

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>

namespace
{
    constexpr float WorldHalfWidth = 10.0f; // 世界の左右の端(m)。巡回は、ここで折り返す(「壁」)
    constexpr float PixelsPerMeter = 40.0f; // 画面の絵の縮尺
}

class BehaviorApp : public KR::Application
{
public:
    BehaviorApp() : Application(KR::ApplicationDesc{ .title = "KR Engine - Behavior", .width = 1280, .height = 800 }) {}

protected:
    void OnInit() override
    {
        // 起動時:グラフを読む(読めなければ、エラーを出して、待機だけのグラフ。落ちない)
        m_Asset.Load(KR::GetAssetDirectory("Enemies") / "Walker.json");
        m_Runner = std::make_unique<KR::Behavior::Runner>(m_Asset.GetGraph());
        m_SeenVersion = m_Asset.GetVersion();
    }

    void OnUpdate(float deltaTime) override
    {
        // ファイルが書き換わっていたら、読み直して、Runner に渡す(今の状態の id が残っていれば、引き継ぐ)
        m_Asset.ReloadIfChanged(deltaTime);
        if (m_Asset.GetVersion() != m_SeenVersion)
        {
            m_Runner->SetGraph(m_Asset.GetGraph());
            m_SeenVersion = m_Asset.GetVersion();
        }

        // ① ゲームの状況を、Context に入れる(キーの名前は、グラフの条件ノードの設定と、そろえる)
        KR::Behavior::Context context;
        context.SetFloat("distanceToPlayerX", std::fabs(m_PlayerX - m_EnemyX));
        context.SetFloat("distanceToPlayerY", 0.0f);
        context.SetBool("attackFinished", m_AttackFinished);

        // ② 「今の状態」を決めてもらう
        m_Runner->Update(deltaTime, context);
        const KR::Behavior::StateView state = m_Runner->GetCurrentState();

        // ③ 返された状態に従って、ゲームが動く(移動・攻撃は、ゲームの仕事)
        const std::string& type = state.GetType();
        if (type == "Patrol")
        {
            m_Facing = m_PatrolDirection;
            m_EnemyX += m_PatrolDirection * state.GetFloat("speed", 1.0f) * deltaTime;
            if (m_EnemyX > WorldHalfWidth || m_EnemyX < -WorldHalfWidth) // 壁で反転
            {
                m_EnemyX = std::clamp(m_EnemyX, -WorldHalfWidth, WorldHalfWidth);
                m_PatrolDirection = -m_PatrolDirection;
            }
        }
        else if (type == "MoveToPlayer")
        {
            const float toPlayer = m_PlayerX - m_EnemyX;
            const float step = state.GetFloat("speed", 1.0f) * deltaTime;
            m_Facing = toPlayer >= 0.0f ? 1.0f : -1.0f;
            m_EnemyX = std::fabs(toPlayer) <= step ? m_PlayerX : m_EnemyX + m_Facing * step;
        }

        // 攻撃:溜め → 出ている → 硬直(各秒は、グラフの値)。入った最初のフレームで、数え始める。全部終わったら、「攻撃が終わった」
        if (type == "Attack")
        {
            if (state.EnteredThisFrame())
            {
                m_AttackTime = 0.0f;
                m_AttackHit = false;
                m_Facing = (m_PlayerX - m_EnemyX) >= 0.0f ? 1.0f : -1.0f;
            }
            m_AttackTime += deltaTime;
            m_Windup = state.GetFloat("windupSeconds", 0.4f);
            m_Active = state.GetFloat("activeSeconds", 0.2f);
            m_Recovery = state.GetFloat("recoverySeconds", 0.4f);
            m_BoxWidth = state.GetFloat("boxWidth", 1.0f);
            m_BoxHeight = state.GetFloat("boxHeight", 0.5f);
            m_Damage = state.GetInt("damage", 1);
            m_AttackFinished = m_AttackTime >= m_Windup + m_Active + m_Recovery;

            // 出ている間に、箱がプレイヤーに重なったら、ヒット(1 回の攻撃で 1 回)
            if (IsAttackActive() && !m_AttackHit)
            {
                const float boxEnd = m_EnemyX + m_Facing * m_BoxWidth;
                const float low = std::min(m_EnemyX, boxEnd);
                const float high = std::max(m_EnemyX, boxEnd);
                if (m_PlayerX >= low && m_PlayerX <= high)
                {
                    m_AttackHit = true;
                    m_TotalDamage += m_Damage;
                }
            }
        }
        else
        {
            m_AttackFinished = false;
            m_AttackTime = -1.0f;
        }
    }

    void OnImGui() override
    {
        // 上の窓:プレイヤー(ダミー)の位置と、世界の絵
        ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(1260.0f, 270.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("敵の行動(サンプル)");
        ImGui::SliderFloat("プレイヤーの位置(m)", &m_PlayerX, -WorldHalfWidth, WorldHalfWidth);
        const KR::Behavior::StateView state = m_Runner->GetCurrentState();
        ImGui::Text("敵:%.2f m  向き %s  今の状態 %s  入ってから %.2f 秒   プレイヤーまで %.2f m   ダメージ合計 %d",
            m_EnemyX, m_Facing > 0.0f ? "→" : "←", state.GetType().c_str(), state.GetTimeInState(), std::fabs(m_PlayerX - m_EnemyX), m_TotalDamage);
        DrawWorld();
        ImGui::End();

        // 下の窓:グラフ(今の状態のノードが光る)
        ImGui::SetNextWindowPos(ImVec2(10.0f, 290.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(1260.0f, 500.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("グラフ(DrawGraphDebug)");
        ImGui::TextDisabled("%s(版 %u)。ツールで書き換えて保存すると、約 0.5 秒で、反映されます", m_Asset.GetFile().filename().string().c_str(), m_Asset.GetVersion());
        KR::Behavior::DrawGraphDebug(*m_Runner);
        ImGui::End();
    }

    void OnShutdown() override
    {
        m_Runner.reset();
    }

private:
    bool IsAttackActive() const { return m_AttackTime >= m_Windup && m_AttackTime < m_Windup + m_Active; }

    // 世界の絵:地面・敵(状態で色が変わる)・プレイヤー・攻撃の箱(溜め = 薄い枠 / 出ている = 赤 / 硬直 = 灰色の枠)
    void DrawWorld()
    {
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const ImVec2 size(WorldHalfWidth * 2.0f * PixelsPerMeter + 40.0f, 140.0f);
        ImGui::Dummy(size);
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), IM_COL32(28, 29, 35, 255));

        const float groundY = origin.y + size.y - 28.0f;
        const auto toScreenX = [&](float meters) { return origin.x + 20.0f + (meters + WorldHalfWidth) * PixelsPerMeter; };
        draw->AddLine(ImVec2(toScreenX(-WorldHalfWidth), groundY), ImVec2(toScreenX(WorldHalfWidth), groundY), IM_COL32(120, 120, 125, 255), 2.0f);
        draw->AddLine(ImVec2(toScreenX(-WorldHalfWidth), groundY - 60.0f), ImVec2(toScreenX(-WorldHalfWidth), groundY), IM_COL32(120, 120, 125, 255), 2.0f); // 壁
        draw->AddLine(ImVec2(toScreenX(WorldHalfWidth), groundY - 60.0f), ImVec2(toScreenX(WorldHalfWidth), groundY), IM_COL32(120, 120, 125, 255), 2.0f);

        // プレイヤー(緑の丸)
        draw->AddCircleFilled(ImVec2(toScreenX(m_PlayerX), groundY - 14.0f), 14.0f, IM_COL32(90, 200, 120, 255));
        draw->AddText(ImVec2(toScreenX(m_PlayerX) - 14.0f, groundY + 4.0f), IM_COL32(200, 230, 205, 255), "プレイヤー");

        // 敵(四角。状態で、色が変わる)
        const std::string& type = m_Runner->GetCurrentState().GetType();
        ImU32 color = IM_COL32(120, 120, 130, 255); // 待機
        if (type == "Patrol") { color = IM_COL32(70, 130, 220, 255); }
        else if (type == "MoveToPlayer") { color = IM_COL32(230, 170, 50, 255); }
        else if (type == "Attack") { color = IM_COL32(220, 70, 70, 255); }
        const float enemyScreenX = toScreenX(m_EnemyX);
        draw->AddRectFilled(ImVec2(enemyScreenX - 16.0f, groundY - 40.0f), ImVec2(enemyScreenX + 16.0f, groundY), color, 3.0f);
        draw->AddTriangleFilled(ImVec2(enemyScreenX + m_Facing * 22.0f, groundY - 20.0f), ImVec2(enemyScreenX + m_Facing * 12.0f, groundY - 28.0f), ImVec2(enemyScreenX + m_Facing * 12.0f, groundY - 12.0f), IM_COL32(255, 255, 255, 255));
        draw->AddText(ImVec2(enemyScreenX - 16.0f, groundY - 58.0f), IM_COL32(230, 230, 230, 255), type.c_str());

        // 攻撃の箱
        if (type == "Attack" && m_AttackTime >= 0.0f)
        {
            const float boxEndScreen = toScreenX(m_EnemyX + m_Facing * m_BoxWidth);
            const ImVec2 low(std::min(enemyScreenX, boxEndScreen), groundY - m_BoxHeight * PixelsPerMeter);
            const ImVec2 high(std::max(enemyScreenX, boxEndScreen), groundY);
            if (IsAttackActive())
            {
                draw->AddRectFilled(low, high, IM_COL32(255, 60, 60, 150));
                draw->AddRect(low, high, IM_COL32(255, 90, 90, 255), 0.0f, 0, 2.0f);
                if (m_AttackHit)
                {
                    draw->AddText(ImVec2(toScreenX(m_PlayerX) - 12.0f, groundY - 52.0f), IM_COL32(255, 120, 120, 255), "ヒット!");
                }
            }
            else
            {
                const bool windup = m_AttackTime < m_Windup;
                draw->AddRect(low, high, windup ? IM_COL32(255, 200, 80, 200) : IM_COL32(130, 130, 135, 200), 0.0f, 0, 1.5f);
            }
        }
    }

    KR::Behavior::GraphAsset m_Asset;
    std::unique_ptr<KR::Behavior::Runner> m_Runner;
    uint32_t m_SeenVersion = 0;

    // ゲーム側の状態(エンジンは、持たない)
    float m_PlayerX = 6.0f;
    float m_EnemyX = -6.0f;
    float m_Facing = 1.0f;
    float m_PatrolDirection = 1.0f;
    float m_AttackTime = -1.0f; // 攻撃に入ってからの秒(攻撃中でないとき = -1)
    bool m_AttackFinished = false;
    bool m_AttackHit = false;
    float m_Windup = 0.4f;
    float m_Active = 0.2f;
    float m_Recovery = 0.4f;
    float m_BoxWidth = 1.0f;
    float m_BoxHeight = 0.5f;
    int m_Damage = 1;
    int m_TotalDamage = 0;
};

std::unique_ptr<KR::Application> KR::CreateApplication()
{
    return std::make_unique<BehaviorApp>();
}
