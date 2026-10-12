#include "KR/Animation/AnimatedModel.h"

#include "KR/Animation/AnimationPose.h"
#include "KR/Animation/LoadModel.h"
#include "KR/Animation/HitboxFile.h"
#include "KR/Animation/SkinMatrices.h"
#include "KR/Core/Log.h"

#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace KR
{
    float AnimatedModel::Bounds::Radius() const
    {
        const float dx = max.x - min.x;
        const float dy = max.y - min.y;
        const float dz = max.z - min.z;
        return 0.5f * std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    bool AnimatedModel::Load(Renderer& renderer, const std::filesystem::path& relativePath)
    {
        Unload(renderer); // 読み込み済みなら、先に片づける

        ModelData model = LoadModel(relativePath); // 拡張子で、glTF か FBX を選ぶ
        if (model.IsEmpty())
        {
            return false; // エラーは、LoadModel(の中の、読み込み)が出している
        }

        // メッシュを囲む箱(CreateMesh のあとは、CPU 側の頂点は、捨ててよいので、先に求める)
        m_Bounds.min = m_Bounds.max = model.mesh.vertices[0].position;
        for (const Vertex& vertex : model.mesh.vertices)
        {
            m_Bounds.min = { std::min(m_Bounds.min.x, vertex.position.x), std::min(m_Bounds.min.y, vertex.position.y), std::min(m_Bounds.min.z, vertex.position.z) };
            m_Bounds.max = { std::max(m_Bounds.max.x, vertex.position.x), std::max(m_Bounds.max.y, vertex.position.y), std::max(m_Bounds.max.z, vertex.position.z) };
        }

        m_Mesh = renderer.CreateMesh(model.mesh);
        if (!m_Mesh.IsValid())
        {
            return false; // エラーは、CreateMesh が出している
        }

        m_ModelPath = relativePath;
        m_Skinned = !model.mesh.skin.empty();
        m_Skeleton = std::move(model.skeleton);
        m_Clips = std::move(model.clips);

        ReloadHitboxes(); // ファイルがあれば読む・無ければ、骨ごとの既定の形

        // 最初の動きを、ループで再生しはじめる(動きが無ければ、基準姿勢のまま)
        m_CurrentClip = m_Clips.empty() ? -1 : 0;
        m_Time = 0.0f;
        m_Speed = 1.0f;
        m_Loop = true;
        m_Paused = false;
        m_Finished = false;
        UpdatePose();

        KR_LOG_INFO("AnimatedModel: {} を読みました(動き {} 個・判定の形 {} 個)", relativePath.filename().string(), m_Clips.size(), m_Hitboxes.size());
        return true;
    }

    void AnimatedModel::Unload(Renderer& renderer)
    {
        if (m_Mesh.IsValid())
        {
            renderer.DestroyMesh(m_Mesh);
        }
        m_Mesh = {};
        m_Skinned = false;
        m_Skeleton = {};
        m_Clips.clear();
        m_Hitboxes.clear();
        m_Pose.clear();
        m_SkinMatrices.clear();
        m_CurrentClip = -1;
        m_Time = 0.0f;
        m_Finished = false;
    }

    bool AnimatedModel::Play(size_t clipIndex, bool loop)
    {
        if (clipIndex >= m_Clips.size())
        {
            KR_LOG_WARN("AnimatedModel::Play: 動きの番号 {} がありません(動きは {} 個)", clipIndex, m_Clips.size());
            return false;
        }
        m_CurrentClip = static_cast<int>(clipIndex);
        m_Loop = loop;
        m_Time = (m_Speed < 0.0f) ? m_Clips[clipIndex].duration : 0.0f; // 逆再生なら、最後から
        m_Finished = false;
        UpdatePose();
        return true;
    }

    bool AnimatedModel::Play(const std::string& clipName, bool loop)
    {
        for (size_t i = 0; i < m_Clips.size(); ++i)
        {
            if (m_Clips[i].name == clipName)
            {
                return Play(i, loop);
            }
        }
        KR_LOG_WARN("AnimatedModel::Play: 動き「{}」がありません", clipName);
        return false;
    }

    void AnimatedModel::Stop()
    {
        m_CurrentClip = -1;
        m_Time = 0.0f;
        m_Finished = false;
        UpdatePose();
    }

    float AnimatedModel::GetDuration() const
    {
        return m_CurrentClip >= 0 ? m_Clips[static_cast<size_t>(m_CurrentClip)].duration : 0.0f;
    }

    void AnimatedModel::SetTime(float time)
    {
        const float duration = GetDuration();
        m_Time = std::clamp(time, 0.0f, duration);
        m_Finished = false;
        UpdatePose();
    }

    void AnimatedModel::Update(float deltaTime)
    {
        if (!IsLoaded())
        {
            return;
        }

        if (m_CurrentClip >= 0 && !m_Paused && !m_Finished)
        {
            const float duration = GetDuration();
            m_Time += deltaTime * m_Speed;
            if (duration > 0.0f)
            {
                if (m_Loop)
                {
                    // 長さをこえたら、最初に戻る(逆再生なら、最後に戻る)
                    m_Time = std::fmod(m_Time, duration);
                    if (m_Time < 0.0f)
                    {
                        m_Time += duration;
                    }
                }
                else if (m_Time >= duration || m_Time <= 0.0f)
                {
                    // ループしない:端で止まる
                    m_Time = std::clamp(m_Time, 0.0f, duration);
                    m_Finished = true;
                }
            }
        }
        UpdatePose();
    }

    void AnimatedModel::UpdatePose()
    {
        const AnimationClip* clip = (m_CurrentClip >= 0) ? &m_Clips[static_cast<size_t>(m_CurrentClip)] : nullptr;
        m_Pose = EvaluatePose(m_Skeleton, clip, m_Time);
        if (m_Skinned)
        {
            m_SkinMatrices = ComputeSkinMatrices(m_Skeleton, m_Pose);
        }
    }

    void AnimatedModel::Draw(Renderer& renderer, const XMMATRIX& world, TextureHandle texture) const
    {
        if (!IsLoaded())
        {
            return;
        }
        if (m_Skinned)
        {
            renderer.DrawSkinnedMesh(m_Mesh, world, m_SkinMatrices, texture);
        }
        else
        {
            renderer.DrawMesh(m_Mesh, world, texture);
        }
    }

    std::vector<WorldHitbox> AnimatedModel::GetWorldHitboxes(const XMMATRIX& world) const
    {
        return EvaluateHitboxes(m_Hitboxes, m_Pose, world);
    }

    bool AnimatedModel::SaveHitboxes() const
    {
        return KR::SaveHitboxes(GetHitboxFilePath(m_ModelPath), m_Hitboxes, m_Skeleton);
    }

    bool AnimatedModel::ReloadHitboxes()
    {
        std::vector<Hitbox> loaded;
        const HitboxLoadResult result = LoadHitboxes(GetHitboxFilePath(m_ModelPath), m_Skeleton, loaded);
        if (result == HitboxLoadResult::Loaded)
        {
            m_Hitboxes = std::move(loaded);
            return true;
        }

        // ファイルが無い・壊れている:骨ごとの既定の形(太さは、モデルの大きさの 12%)
        m_Hitboxes = GenerateDefaultHitboxes(m_Skeleton, m_Bounds.Radius() * 0.12f);
        return false;
    }
}
