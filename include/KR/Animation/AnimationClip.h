#pragma once

#include <DirectXMath.h>

#include <string>
#include <vector>

namespace KR
{
    // キーとキーの間の、埋め方
    enum class Interpolation
    {
        Linear, // 直線でつなぐ(向きは、球面で補間する slerp)
        Step,   // 次のキーまで、前の値のまま(階段)
    };

    struct Vec3Key
    {
        float time = 0.0f; // 秒
        DirectX::XMFLOAT3 value = {};
    };

    struct QuatKey
    {
        float time = 0.0f; // 秒
        DirectX::XMFLOAT4 value = { 0.0f, 0.0f, 0.0f, 1.0f }; // クォータニオン(x, y, z, w)
    };

    // 1本の骨の、動き。位置・向き・大きさは、それぞれ別のキー列(無ければ空 = 基準姿勢のまま)
    struct JointTrack
    {
        int jointIndex = -1; // Skeleton::joints の番号

        Interpolation translationInterpolation = Interpolation::Linear;
        std::vector<Vec3Key> translations;

        Interpolation rotationInterpolation = Interpolation::Linear;
        std::vector<QuatKey> rotations;

        Interpolation scaleInterpolation = Interpolation::Linear;
        std::vector<Vec3Key> scales;
    };

    // 1つの動き(走る・羽ばたく、など)
    struct AnimationClip
    {
        std::string name;
        float duration = 0.0f; // 長さ(秒)。全部のキーの、いちばん遅い時刻
        std::vector<JointTrack> tracks;
    };
}
