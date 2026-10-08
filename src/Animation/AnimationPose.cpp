#include "KR/Animation/AnimationPose.h"

using namespace DirectX;

namespace KR
{
    namespace
    {
        // time をはさむ 2 つのキーを探す。first = 手前のキー・second = 次のキー・t = 2 つの間のどこか(0 〜 1)
        // time が最初より前なら最初のキー・最後より後なら最後のキーのまま(first == second)
        template <typename Key>
        void FindKeys(const std::vector<Key>& keys, float time, size_t& first, size_t& second, float& t)
        {
            if (time <= keys.front().time)
            {
                first = second = 0;
                t = 0.0f;
                return;
            }
            if (time >= keys.back().time)
            {
                first = second = keys.size() - 1;
                t = 0.0f;
                return;
            }
            size_t next = 1;
            while (keys[next].time < time)
            {
                ++next;
            }
            first = next - 1;
            second = next;
            t = (time - keys[first].time) / (keys[second].time - keys[first].time);
        }

        // 位置・大きさのキー列から、時刻 time の値を求める(キーが無ければ fallback)
        XMVECTOR SampleVec3(const std::vector<Vec3Key>& keys, Interpolation interpolation, float time, const XMFLOAT3& fallback)
        {
            if (keys.empty())
            {
                return XMLoadFloat3(&fallback);
            }
            size_t first, second;
            float t;
            FindKeys(keys, time, first, second, t);
            const XMVECTOR a = XMLoadFloat3(&keys[first].value);
            if (interpolation == Interpolation::Step || first == second)
            {
                return a;
            }
            return XMVectorLerp(a, XMLoadFloat3(&keys[second].value), t); // 直線
        }

        // 向きのキー列から、時刻 time の値を求める。向きの補間は、球面(slerp)
        XMVECTOR SampleQuat(const std::vector<QuatKey>& keys, Interpolation interpolation, float time, const XMFLOAT4& fallback)
        {
            if (keys.empty())
            {
                return XMLoadFloat4(&fallback);
            }
            size_t first, second;
            float t;
            FindKeys(keys, time, first, second, t);
            const XMVECTOR a = XMLoadFloat4(&keys[first].value);
            if (interpolation == Interpolation::Step || first == second)
            {
                return a;
            }
            return XMQuaternionSlerp(a, XMLoadFloat4(&keys[second].value), t);
        }

        // 骨 index の世界の行列を求める(親が後ろに並んでいてもよいように、親を先に求める。求めたら done に印をつける)
        const XMMATRIX& ComputeWorld(size_t index, const Skeleton& skeleton, const std::vector<XMMATRIX>& local,
                                     std::vector<XMMATRIX>& world, std::vector<bool>& done)
        {
            if (!done[index])
            {
                const int parent = skeleton.joints[index].parent;
                // 根は、親のノード(骨ではない)の行列を掛ける
                const XMMATRIX parentWorld = (parent < 0) ? XMLoadFloat4x4(&skeleton.joints[index].parentWorld)
                                                           : ComputeWorld(static_cast<size_t>(parent), skeleton, local, world, done);
                world[index] = local[index] * parentWorld;
                done[index] = true;
            }
            return world[index];
        }
    }

    std::vector<XMFLOAT4X4> EvaluatePose(const Skeleton& skeleton, const AnimationClip* clip, float time)
    {
        const size_t jointCount = skeleton.joints.size();

        // ①②:各骨の、親から見た行列(大きさ → 向き → 位置の順に掛ける。行ベクトルの形)
        std::vector<XMMATRIX> local(jointCount);
        for (size_t i = 0; i < jointCount; ++i)
        {
            const Joint& joint = skeleton.joints[i];
            XMVECTOR translation = XMLoadFloat3(&joint.translation);
            XMVECTOR rotation = XMLoadFloat4(&joint.rotation);
            XMVECTOR scale = XMLoadFloat3(&joint.scale);

            if (clip)
            {
                for (const JointTrack& track : clip->tracks)
                {
                    if (track.jointIndex == static_cast<int>(i))
                    {
                        translation = SampleVec3(track.translations, track.translationInterpolation, time, joint.translation);
                        rotation = XMQuaternionNormalize(SampleQuat(track.rotations, track.rotationInterpolation, time, joint.rotation));
                        scale = SampleVec3(track.scales, track.scaleInterpolation, time, joint.scale);
                        break;
                    }
                }
            }
            local[i] = XMMatrixScalingFromVector(scale) * XMMatrixRotationQuaternion(rotation) * XMMatrixTranslationFromVector(translation);
        }

        // ③:親の行列を掛けて、世界の行列にする
        std::vector<XMMATRIX> world(jointCount);
        std::vector<bool> done(jointCount, false);
        std::vector<XMFLOAT4X4> result(jointCount);
        for (size_t i = 0; i < jointCount; ++i)
        {
            XMStoreFloat4x4(&result[i], ComputeWorld(i, skeleton, local, world, done));
        }
        return result;
    }
}
