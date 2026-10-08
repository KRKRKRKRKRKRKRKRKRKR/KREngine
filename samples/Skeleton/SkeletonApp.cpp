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
    void OnInit() override
    {
        m_Model = KR::LoadGltf("Test/BossObstacle/BossObstacle.glb");
        if (m_Model.IsEmpty())
        {
            return; // 失敗は、LoadGltf がログに出している
        }

        m_Mesh = GetRenderer().CreateMesh(m_Model.mesh);
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
            KR::DrawSkeleton(GetRenderer(), m_Model.skeleton, pose, world, { 1.0f, 1.0f, 0.0f, 1.0f });
        }
    }

    void OnImGui() override
    {
        ImGui::Begin("Skeleton");
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Text("Joints: %zu / Clips: %zu", m_Model.skeleton.joints.size(), m_Model.clips.size());
        ImGui::Checkbox("Bones", &m_ShowBones);
        ImGui::Checkbox("Mesh", &m_ShowMesh);
        ImGui::Checkbox("Rotate", &m_Rotate);
        ImGui::Checkbox("Bind pose", &m_BindPose); // チェック:動きを使わず、基準姿勢(形が、骨なしの形と同じに見えるはず)

        if (!m_Model.clips.empty())
        {
            const KR::AnimationClip& clip = m_Model.clips[m_ClipIndex];
            ImGui::Separator();
            ImGui::Text("Clip: %s (%.3f s)", clip.name.c_str(), clip.duration);
            ImGui::Checkbox("Play", &m_Playing);
            ImGui::SliderFloat("Speed", &m_Speed, 0.0f, 3.0f);
            ImGui::SliderFloat("Time", &m_Time, 0.0f, clip.duration); // 止めているときに、時刻を動かして、姿勢を確かめる
        }
        ImGui::End();
    }

    void OnShutdown() override
    {
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
    bool m_ShowBones = true;
    bool m_ShowMesh = true;
    bool m_Rotate = true;
    bool m_BindPose = false;

    size_t m_ClipIndex = 0;   // 再生する動きの番号
    float m_Time = 0.0f;      // 動きの、今の時刻(秒)
    float m_Speed = 1.0f;     // 再生の速さ(1 = 等倍)
    bool m_Playing = true;
};

std::unique_ptr<KR::Application> KR::CreateApplication()
{
    return std::make_unique<SkeletonApp>();
}
