// KR エンジンのサンプル:動くモデル(AnimatedModel)の使い方
// glTF を読んで、骨・動き・判定の形を使う。ゲームが AnimatedModel を使うときの、見本にもなる(部品を直接呼ばない)
//
//   OnInit    : Load(読み込み)
//   OnUpdate  : Update(時刻を進めて、姿勢を求める)
//   OnRender  : Draw(描く)・GetWorldHitboxes(判定の形)
//   OnShutdown: Unload

#include <KR/KR.h>

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

using namespace DirectX;

class SkeletonApp : public KR::Application
{
public:
    SkeletonApp() : Application(KR::ApplicationDesc{ .title = "KR Engine - Skeleton" }) {}

protected:
    // 当たり判定の確認:答えが分かっている例で、Intersects を調べる。全部合っていれば OK のログ・違えばエラーのログ
    static void RunCollisionSelfTest()
    {
        int failures = 0;
        auto check = [&](const char* name, bool actual, bool expected)
        {
            if (actual != expected)
            {
                KR_LOG_ERROR("  判定の確認 NG: {}(結果 {} / 期待 {})", name, actual, expected);
                ++failures;
            }
        };

        const KR::Sphere sphere{ { 0.0f, 0.0f, 0.0f }, 1.0f };
        check("球×球 重なる", KR::Intersects(sphere, KR::Sphere{ { 1.5f, 0.0f, 0.0f }, 1.0f }), true);
        check("球×球 離れる", KR::Intersects(sphere, KR::Sphere{ { 2.5f, 0.0f, 0.0f }, 1.0f }), false);
        check("球×球 ちょうど接する", KR::Intersects(sphere, KR::Sphere{ { 2.0f, 0.0f, 0.0f }, 1.0f }), true);

        // 1 辺 2 m(半分の大きさ 1)の箱。軸にそろえた箱と、Y 軸まわりに 45° 回した箱
        const KR::OBB box = KR::MakeOBB({ 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, XMMatrixIdentity());
        check("球×箱 面に近い(当たり)", KR::Intersects(KR::Sphere{ { 1.4f, 0.0f, 0.0f }, 0.5f }, box), true);
        check("球×箱 面から離れる", KR::Intersects(KR::Sphere{ { 1.6f, 0.0f, 0.0f }, 0.5f }, box), false);
        // 角 (1,1,1) から、(1.5,1.5,1.5) までの距離は 0.866。半径 0.9 なら当たり・0.8 なら外れ
        check("球×箱 角に近い(当たり)", KR::Intersects(KR::Sphere{ { 1.5f, 1.5f, 1.5f }, 0.9f }, box), true);
        check("球×箱 角から離れる", KR::Intersects(KR::Sphere{ { 1.5f, 1.5f, 1.5f }, 0.8f }, box), false);
        check("球×箱 箱の中に入っている", KR::Intersects(KR::Sphere{ { 0.2f, 0.1f, 0.0f }, 0.1f }, box), true);
        check("箱×球 引数の順を入れ替えても同じ", KR::Intersects(box, KR::Sphere{ { 1.6f, 0.0f, 0.0f }, 0.5f }), false);

        // Y 軸まわりに 45° 回した箱の角は、X 軸の上で、中心から 1.414 m のところに出る
        const KR::OBB rotated = KR::MakeOBB({ 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, XMMatrixRotationY(XM_PIDIV4));
        check("回した箱 角の先(当たり)", KR::Intersects(KR::Sphere{ { 1.7f, 0.0f, 0.0f }, 0.5f }, rotated), true);
        check("軸にそろえた箱 同じ点は外れ", KR::Intersects(KR::Sphere{ { 1.7f, 0.0f, 0.0f }, 0.5f }, box), false);

        if (failures == 0)
        {
            KR_LOG_INFO("  判定の確認: OK(球×球・球×箱・回した箱)");
        }
    }

    // 判定の形の、保存 → 読み込みの往復の確認:同じ値に戻るか(一時フォルダーに書いて、消す。絶対パスは、そのまま使われる)
    static void RunHitboxFileSelfTest(const KR::AnimatedModel& model)
    {
        const std::filesystem::path file = std::filesystem::temp_directory_path() / "kr_hitbox_roundtrip.hitboxes.json";
        std::vector<KR::Hitbox> loaded;
        const bool saved = KR::SaveHitboxes(file, model.GetHitboxes(), model.GetSkeleton());
        const KR::HitboxLoadResult result = KR::LoadHitboxes(file, model.GetSkeleton(), loaded);

        bool same = saved && result == KR::HitboxLoadResult::Loaded && loaded.size() == model.GetHitboxes().size();
        for (size_t i = 0; same && i < loaded.size(); ++i)
        {
            const KR::Hitbox& a = model.GetHitboxes()[i];
            const KR::Hitbox& b = loaded[i];
            same = a.name == b.name && a.jointIndex == b.jointIndex && a.shape == b.shape
                && a.center.x == b.center.x && a.center.y == b.center.y && a.center.z == b.center.z
                && a.halfSize.x == b.halfSize.x && a.halfSize.y == b.halfSize.y && a.halfSize.z == b.halfSize.z
                && a.radius == b.radius;
        }
        std::error_code ignored;
        std::filesystem::remove(file, ignored);

        if (same)
        {
            KR_LOG_INFO("  判定の形の保存の確認: OK(保存 → 読み込みで、同じ値)");
        }
        else
        {
            KR_LOG_ERROR("  判定の形の保存の確認 NG(保存 → 読み込みで、値が変わった)");
        }
    }

    void OnInit() override
    {
        RunCollisionSelfTest();

        if (!m_Model.Load(GetRenderer(), "Test/BossObstacle/BossObstacle.glb"))
        {
            return; // 失敗は、Load がログに出している
        }
        RunHitboxFileSelfTest(m_Model);

        m_BoneRenderer.Create(GetRenderer());

        // カメラ:モデルの中心を原点に動かして(OnRender の world)、大きさの 3 倍だけ離れて見る
        const KR::AnimatedModel::Bounds& bounds = m_Model.GetBounds();
        m_Center = bounds.Center();
        const float radius = bounds.Radius();
        KR_LOG_INFO("  大きさ: 中心 ({:.2f}, {:.2f}, {:.2f}) / 半径 {:.2f} m", m_Center.x, m_Center.y, m_Center.z, radius);

        m_ModelRadius = radius;
        m_TestBox.halfSize = { radius * 0.15f, radius * 0.15f, radius * 0.15f };
        m_TestCenter = { radius * 0.8f, 0.0f, 0.0f }; // 最初は、モデルの横に置く(スライダーで、動かして、骨の形に、ぶつける)

        KR::Camera camera;
        camera.SetPosition({ 0.0f, radius * 0.5f, -radius * 3.0f });
        camera.LookAt({ 0.0f, 0.0f, 0.0f });
        GetRenderer().SetCamera(camera);
    }

    void OnUpdate(float deltaTime) override
    {
        if (m_Rotate)
        {
            m_Angle = std::fmod(m_Angle + deltaTime * 0.5f, 2.0f * std::numbers::pi_v<float>);
        }
        m_Model.Update(deltaTime); // 時刻を進めて、姿勢を求める
    }

    void OnRender() override
    {
        if (!m_Model.IsLoaded())
        {
            return;
        }

        // 形の中心を原点に動かしてから、Y 軸で回す
        const XMMATRIX world = XMMatrixTranslation(-m_Center.x, -m_Center.y, -m_Center.z) * XMMatrixRotationY(m_Angle);

        if (m_ShowMesh)
        {
            m_Model.Draw(GetRenderer(), world);
        }
        if (m_ShowBones)
        {
            KR::BoneRenderer::Style style;
            style.selectedJoint = m_SelectedBone; // 選んだ骨は、色が変わる
            style.inFront = m_BonesInFront;
            m_BoneRenderer.Draw(GetRenderer(), m_Model.GetSkeleton(), m_Model.GetPose(), world, style);
        }

        if (m_ShowHitboxes)
        {
            // テストの箱(世界に固定。ImGui で動かす)
            m_TestBox.center = m_TestCenter;
            GetRenderer().DrawWireBox(m_TestBox, { 0.3f, 0.8f, 1.0f, 1.0f });
            KR::WorldHitbox testShape;
            testShape.shape = KR::HitShape::Box;
            testShape.box = m_TestBox;

            // 判定の形を、今の姿勢で、世界の座標にして、描く
            const std::vector<KR::WorldHitbox> shapes = m_Model.GetWorldHitboxes(world);
            m_HitIndices.clear();
            for (size_t i = 0; i < shapes.size(); ++i)
            {
                const bool hit = KR::Intersects(shapes[i], testShape);
                if (hit)
                {
                    m_HitIndices.push_back(i);
                }
                // 当たり = 赤 / 選んでいる形 = 黄 / ほか = 緑
                const bool isSelected = (i == m_SelectedHitbox);
                const XMFLOAT4 color = hit ? XMFLOAT4{ 1.0f, 0.2f, 0.2f, 1.0f }
                                     : isSelected ? XMFLOAT4{ 1.0f, 0.9f, 0.1f, 1.0f } : XMFLOAT4{ 0.2f, 1.0f, 0.2f, 1.0f };
                if (shapes[i].shape == KR::HitShape::Box)
                {
                    GetRenderer().DrawWireBox(shapes[i].box, color);
                }
                else
                {
                    GetRenderer().DrawWireSphere(shapes[i].sphere, color);
                }
            }
        }
    }

    void OnImGui() override
    {
        ImGui::Begin("Skeleton");
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        if (!m_Model.IsLoaded())
        {
            ImGui::Text("Model not loaded (see log)");
            ImGui::End();
            return;
        }

        ImGui::Text("Joints: %zu / Clips: %zu", m_Model.GetSkeleton().joints.size(), m_Model.GetClipCount());
        ImGui::Checkbox("Bones", &m_ShowBones);
        ImGui::Checkbox("Bones in front", &m_BonesInFront); // 骨を、メッシュの手前に描く(メッシュの中の骨も見える)
        ImGui::Checkbox("Mesh", &m_ShowMesh);
        ImGui::Checkbox("Rotate", &m_Rotate);

        // 選んだ骨:色が変わる(橙)。その骨の判定の形も、連動して選ばれる(黄色)
        const int lastJoint = static_cast<int>(m_Model.GetSkeleton().joints.size()) - 1;
        if (ImGui::SliderInt("Bone", &m_SelectedBone, 0, std::max(lastJoint, 0)))
        {
            const std::vector<KR::Hitbox>& hitboxes = m_Model.GetHitboxes();
            for (size_t i = 0; i < hitboxes.size(); ++i)
            {
                if (hitboxes[i].jointIndex == m_SelectedBone)
                {
                    m_SelectedHitbox = i;
                    break;
                }
            }
        }

        // 動き(AnimatedModel の再生)
        ImGui::Separator();
        if (m_Model.GetClipCount() > 0)
        {
            int clip = std::max(m_Model.GetCurrentClip(), 0);
            if (ImGui::SliderInt("Clip", &clip, 0, static_cast<int>(m_Model.GetClipCount()) - 1))
            {
                m_Model.Play(static_cast<size_t>(clip), m_Model.IsLooping());
            }
            ImGui::Text("%s (%.3f s)", m_Model.GetClipName(static_cast<size_t>(clip)).c_str(), m_Model.GetDuration());

            bool paused = m_Model.IsPaused();
            if (ImGui::Checkbox("Pause", &paused))
            {
                m_Model.Pause(paused);
            }
            bool loop = m_Model.IsLooping();
            if (ImGui::Checkbox("Loop", &loop))
            {
                m_Model.SetLooping(loop);
                if (loop)
                {
                    m_Model.Play(static_cast<size_t>(clip), true); // ループに戻したら、最初から
                }
            }
            ImGui::SameLine();
            ImGui::Text(m_Model.IsFinished() ? "(finished)" : "");
            float speed = m_Model.GetSpeed();
            if (ImGui::SliderFloat("Speed", &speed, 0.0f, 3.0f))
            {
                m_Model.SetSpeed(speed);
            }
            float time = m_Model.GetTime();
            if (ImGui::SliderFloat("Time", &time, 0.0f, m_Model.GetDuration())) // 止めているときに、時刻を動かして、姿勢を確かめる
            {
                m_Model.SetTime(time);
            }
            if (ImGui::Button("Stop (bind pose)"))
            {
                m_Model.Stop();
            }
        }

        // 骨に追従する判定の形
        ImGui::Separator();
        ImGui::Checkbox("Hitboxes", &m_ShowHitboxes);
        const float range = m_ModelRadius * 1.5f;
        ImGui::SliderFloat3("Test box position", &m_TestCenter.x, -range, range);
        std::vector<KR::Hitbox>& hitboxes = m_Model.GetHitboxes();
        ImGui::Text("Hit: %zu / %zu", m_HitIndices.size(), hitboxes.size());
        for (const size_t index : m_HitIndices)
        {
            ImGui::BulletText("hitbox %zu (joint %d)", index, hitboxes[index].jointIndex);
        }

        if (!hitboxes.empty())
        {
            m_SelectedHitbox = std::min(m_SelectedHitbox, hitboxes.size() - 1);

            // 形を 1 つ選んで、調整する(形は、骨の中の座標。骨が動いても、この数字は変わらない)
            int selected = static_cast<int>(m_SelectedHitbox);
            if (ImGui::SliderInt("Hitbox", &selected, 0, static_cast<int>(hitboxes.size()) - 1))
            {
                m_SelectedBone = hitboxes[static_cast<size_t>(selected)].jointIndex; // 形を選んだら、その形がくっついた骨も、選ぶ
            }
            m_SelectedHitbox = static_cast<size_t>(selected);
            KR::Hitbox& hitbox = hitboxes[m_SelectedHitbox];

            if (ImGui::SliderInt("Joint", &hitbox.jointIndex, 0, lastJoint))
            {
                m_SelectedBone = hitbox.jointIndex;
            }
            int shape = (hitbox.shape == KR::HitShape::Box) ? 0 : 1;
            ImGui::Combo("Shape", &shape, "Box\0Sphere\0");
            hitbox.shape = (shape == 0) ? KR::HitShape::Box : KR::HitShape::Sphere;
            ImGui::DragFloat3("Center (in joint)", &hitbox.center.x, 0.05f);
            if (hitbox.shape == KR::HitShape::Box)
            {
                ImGui::DragFloat3("Half size", &hitbox.halfSize.x, 0.05f, 0.01f, 100.0f);
            }
            else
            {
                ImGui::DragFloat("Radius", &hitbox.radius, 0.05f, 0.01f, 100.0f);
            }
        }

        // 判定の形の、保存・読み直し(<モデル名>.hitboxes.json。Debug では、元の Assets フォルダーに書く)
        if (ImGui::Button("Save hitboxes"))
        {
            m_Model.SaveHitboxes();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reload hitboxes"))
        {
            m_Model.ReloadHitboxes();
        }
        ImGui::End();
    }

    void OnShutdown() override
    {
        m_BoneRenderer.Destroy(GetRenderer());
        m_Model.Unload(GetRenderer());
    }

private:
    KR::AnimatedModel m_Model; // 動くモデル(形・骨・動き・判定の形)
    KR::BoneRenderer m_BoneRenderer; // 骨を、八面体の形(Blender の骨に似せた形)で描く(デバッグ用)
    XMFLOAT3 m_Center = {};          // 形の中心
    float m_ModelRadius = 1.0f;      // モデルの大きさ(スライダーの範囲に使う)
    float m_Angle = 0.0f;
    int m_SelectedBone = 0;          // 選んだ骨(関節の番号)。色が変わる。選んだ形(判定)と連動する
    bool m_ShowBones = true;
    bool m_BonesInFront = true;
    bool m_ShowMesh = true;
    bool m_Rotate = true;

    // 判定の形の確認用
    size_t m_SelectedHitbox = 0;           // ImGui で調整している形
    bool m_ShowHitboxes = true;
    XMFLOAT3 m_TestCenter = {};            // テストの箱の位置(世界)
    KR::OBB m_TestBox;                     // テストの箱(軸にそろえた箱)
    std::vector<size_t> m_HitIndices;      // 今、テストの箱に触れている形の番号
};

std::unique_ptr<KR::Application> KR::CreateApplication()
{
    return std::make_unique<SkeletonApp>();
}
