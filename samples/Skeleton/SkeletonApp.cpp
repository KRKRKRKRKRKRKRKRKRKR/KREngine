// KR エンジンのサンプル:glTF を読んで、骨を線で描く(基準姿勢)
// 座標変換(右手系→左手系)や、骨の位置が、モデルに合っているかを、目で確かめるためのもの

#include <KR/KR.h>

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
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

    void OnInit() override
    {
        RunCollisionSelfTest();

        m_Model = KR::LoadGltf("Test/BossObstacle/BossObstacle.glb");
        if (m_Model.IsEmpty())
        {
            return; // 失敗は、LoadGltf がログに出している
        }

        m_Mesh = GetRenderer().CreateMesh(m_Model.mesh);
        m_BoneRenderer.Create(GetRenderer());
        m_JointWorld = KR::GetBindPoseWorldMatrices(m_Model.skeleton);

        for (size_t i = 0; i < m_Model.skeleton.joints.size(); ++i)
        {
            KR_LOG_INFO("  骨 {}: {}(親 {})", i, m_Model.skeleton.joints[i].name, m_Model.skeleton.joints[i].parent);
        }
        for (const KR::AnimationClip& clip : m_Model.clips)
        {
            KR_LOG_INFO("  動き {}: {:.3f} 秒 / 動く骨 {} 本", clip.name, clip.duration, clip.tracks.size());
        }

        // 確認:動き無し(基準姿勢)で求めた骨の位置が、逆バインド行列から求めた位置と、合っているか(合っていれば、差は 0 に近い)
        const std::vector<XMFLOAT4X4> bindFromLocal = KR::EvaluatePose(m_Model.skeleton, nullptr, 0.0f);
        float maxDifference = 0.0f;
        for (size_t i = 0; i < bindFromLocal.size(); ++i)
        {
            for (int row = 0; row < 4; ++row)
            {
                for (int column = 0; column < 4; ++column)
                {
                    maxDifference = std::max(maxDifference, std::fabs(bindFromLocal[i].m[row][column] - m_JointWorld[i].m[row][column]));
                }
            }
        }
        KR_LOG_INFO("  基準姿勢の一致(親子の掛け算 と 逆バインド行列の逆): 差の最大 {:.5f}", maxDifference);

        // 形の大きさに合わせて、カメラを置く:形を囲む箱を求めて、その中心を原点に動かし、半径の 3 倍だけ離れて見る
        XMFLOAT3 minimum = m_Model.mesh.vertices[0].position;
        XMFLOAT3 maximum = minimum;
        for (const KR::Vertex& vertex : m_Model.mesh.vertices)
        {
            minimum = { std::min(minimum.x, vertex.position.x), std::min(minimum.y, vertex.position.y), std::min(minimum.z, vertex.position.z) };
            maximum = { std::max(maximum.x, vertex.position.x), std::max(maximum.y, vertex.position.y), std::max(maximum.z, vertex.position.z) };
        }
        m_Center = { (minimum.x + maximum.x) * 0.5f, (minimum.y + maximum.y) * 0.5f, (minimum.z + maximum.z) * 0.5f };
        const float radius = 0.5f * XMVectorGetX(XMVector3Length(XMVectorSet(maximum.x - minimum.x, maximum.y - minimum.y, maximum.z - minimum.z, 0.0f)));
        KR_LOG_INFO("  大きさ: 中心 ({:.2f}, {:.2f}, {:.2f}) / 半径 {:.2f} m", m_Center.x, m_Center.y, m_Center.z, radius);

        // 判定の形:骨ごとの、とりあえずの形(骨の長さに合わせた箱・先端は球)。太さは、モデルの大きさの 12%
        m_Hitboxes = KR::GenerateDefaultHitboxes(m_Model.skeleton, radius * 0.12f);
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

        // 動きの時刻を進める(長さをこえたら、最初に戻る = ループ)
        if (m_Playing && !m_Model.clips.empty())
        {
            const float duration = m_Model.clips[m_ClipIndex].duration;
            m_Time += deltaTime * m_Speed;
            if (duration > 0.0f)
            {
                m_Time = std::fmod(m_Time, duration);
                if (m_Time < 0.0f)
                {
                    m_Time += duration;
                }
            }
        }
    }

    void OnRender() override
    {
        if (m_Model.IsEmpty())
        {
            return;
        }

        // 形の中心を原点に動かしてから、Y 軸で回す
        const XMMATRIX world = XMMatrixTranslation(-m_Center.x, -m_Center.y, -m_Center.z) * XMMatrixRotationY(m_Angle);

        // 今の時刻の姿勢(動きが無いモデル・「Bind pose」のときは、基準姿勢)
        const bool useClip = !m_BindPose && !m_Model.clips.empty();
        const KR::AnimationClip* clip = useClip ? &m_Model.clips[m_ClipIndex] : nullptr;
        const std::vector<XMFLOAT4X4> pose = KR::EvaluatePose(m_Model.skeleton, clip, m_Time);

        if (m_ShowMesh)
        {
            if (m_Model.mesh.skin.empty())
            {
                GetRenderer().DrawMesh(m_Mesh, world); // 骨なしの形
            }
            else
            {
                // 骨つきの形:骨の行列の表(逆バインド × 今の姿勢)を渡すと、GPU が、頂点を骨に合わせて動かす
                GetRenderer().DrawSkinnedMesh(m_Mesh, world, KR::ComputeSkinMatrices(m_Model.skeleton, pose));
            }
        }
        if (m_ShowBones)
        {
            KR::BoneRenderer::Style style;
            style.selectedJoint = m_SelectedBone; // 選んだ骨は、色が変わる
            style.inFront = m_BonesInFront;
            m_BoneRenderer.Draw(GetRenderer(), m_Model.skeleton, pose, world, style);
        }

        if (m_ShowHitboxes)
        {
            // テストの箱(世界に固定。ImGui で動かす)
            m_TestBox.center = m_TestCenter;
            GetRenderer().DrawWireBox(m_TestBox, { 0.3f, 0.8f, 1.0f, 1.0f });
            KR::WorldHitbox testShape;
            testShape.shape = KR::HitShape::Box;
            testShape.box = m_TestBox;

            // 判定の形を、骨の今の姿勢で、世界の座標にして、描く。テストの箱に触れた形は赤・触れていなければ緑
            const std::vector<KR::WorldHitbox> shapes = KR::EvaluateHitboxes(m_Hitboxes, pose, world);
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
        ImGui::Text("Joints: %zu / Clips: %zu", m_Model.skeleton.joints.size(), m_Model.clips.size());
        ImGui::Checkbox("Bones", &m_ShowBones);
        ImGui::Checkbox("Bones in front", &m_BonesInFront); // 骨を、メッシュの手前に描く(メッシュの中の骨も見える)
        ImGui::Checkbox("Mesh", &m_ShowMesh);
        ImGui::Checkbox("Rotate", &m_Rotate);
        ImGui::Checkbox("Bind pose", &m_BindPose); // チェック:動きを使わず、基準姿勢(形が、骨なしの形と同じに見えるはず)

        // 選んだ骨:色が変わる(橙)。その骨の判定の形も、連動して選ばれる(黄色)
        const int lastJoint = static_cast<int>(m_Model.skeleton.joints.size()) - 1;
        if (ImGui::SliderInt("Bone", &m_SelectedBone, 0, std::max(lastJoint, 0)))
        {
            for (size_t i = 0; i < m_Hitboxes.size(); ++i)
            {
                if (m_Hitboxes[i].jointIndex == m_SelectedBone)
                {
                    m_SelectedHitbox = i;
                    break;
                }
            }
        }

        if (!m_Model.clips.empty())
        {
            const KR::AnimationClip& clip = m_Model.clips[m_ClipIndex];
            ImGui::Separator();
            ImGui::Text("Clip: %s (%.3f s)", clip.name.c_str(), clip.duration);
            ImGui::Checkbox("Play", &m_Playing);
            ImGui::SliderFloat("Speed", &m_Speed, 0.0f, 3.0f);
            ImGui::SliderFloat("Time", &m_Time, 0.0f, clip.duration); // 止めているときに、時刻を動かして、姿勢を確かめる
        }

        // 骨に追従する判定の形
        ImGui::Separator();
        ImGui::Checkbox("Hitboxes", &m_ShowHitboxes);
        const float range = m_ModelRadius * 1.5f;
        ImGui::SliderFloat3("Test box position", &m_TestCenter.x, -range, range);
        ImGui::Text("Hit: %zu / %zu", m_HitIndices.size(), m_Hitboxes.size());
        for (const size_t index : m_HitIndices)
        {
            ImGui::BulletText("hitbox %zu (joint %d)", index, m_Hitboxes[index].jointIndex);
        }

        if (!m_Hitboxes.empty())
        {
            // 形を 1 つ選んで、調整する(形は、骨の中の座標。骨が動いても、この数字は変わらない)
            int selected = static_cast<int>(m_SelectedHitbox);
            if (ImGui::SliderInt("Hitbox", &selected, 0, static_cast<int>(m_Hitboxes.size()) - 1))
            {
                m_SelectedBone = m_Hitboxes[static_cast<size_t>(selected)].jointIndex; // 形を選んだら、その形がくっついた骨も、選ぶ
            }
            m_SelectedHitbox = static_cast<size_t>(selected);
            KR::Hitbox& hitbox = m_Hitboxes[m_SelectedHitbox];

            if (ImGui::SliderInt("Joint", &hitbox.jointIndex, 0, static_cast<int>(m_Model.skeleton.joints.size()) - 1))
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
        ImGui::End();
    }

    void OnShutdown() override
    {
        m_BoneRenderer.Destroy(GetRenderer());
        if (m_Mesh.IsValid())
        {
            GetRenderer().DestroyMesh(m_Mesh);
        }
    }

private:
    KR::GltfModel m_Model;
    KR::MeshHandle m_Mesh;
    std::vector<XMFLOAT4X4> m_JointWorld; // 基準姿勢での、各骨の世界の行列
    XMFLOAT3 m_Center = {};               // 形の中心
    float m_Angle = 0.0f;
    KR::BoneRenderer m_BoneRenderer; // 骨を、八面体の形(Blender の骨に似せた形)で描く
    int m_SelectedBone = 0;          // 選んだ骨(関節の番号)。色が変わる。選んだ形(判定)と連動する
    bool m_ShowBones = true;
    bool m_BonesInFront = true;
    bool m_ShowMesh = true;
    bool m_Rotate = true;
    bool m_BindPose = false;

    // 骨に追従する判定
    std::vector<KR::Hitbox> m_Hitboxes;    // 判定の形(骨の中の座標)
    size_t m_SelectedHitbox = 0;           // ImGui で調整している形
    bool m_ShowHitboxes = true;
    float m_ModelRadius = 1.0f;            // モデルの大きさ(スライダーの範囲に使う)
    XMFLOAT3 m_TestCenter = {};            // テストの箱の位置(世界)
    KR::OBB m_TestBox;                     // テストの箱(軸にそろえた箱)
    std::vector<size_t> m_HitIndices;      // 今、テストの箱に触れている形の番号

    size_t m_ClipIndex = 0;   // 再生する動きの番号
    float m_Time = 0.0f;      // 動きの、今の時刻(秒)
    float m_Speed = 1.0f;     // 再生の速さ(1 = 等倍)
    bool m_Playing = true;
};

std::unique_ptr<KR::Application> KR::CreateApplication()
{
    return std::make_unique<SkeletonApp>();
}
