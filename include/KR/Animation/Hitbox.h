#pragma once

#include "KR/Animation/Skeleton.h"
#include "KR/Physics/Collision.h"

#include <DirectXMath.h>

#include <string>
#include <vector>

namespace KR
{
    enum class HitShape
    {
        Box,    // 箱(OBB。骨の中で、向きを持てる)
        Sphere, // 球
    };

    // 骨にくっつけた、判定の形 1 つ。位置・向き・大きさは、**骨の中の座標**で持つ(骨が動いても、この数字は変わらない)
    struct Hitbox
    {
        std::string name;
        int jointIndex = -1; // くっつける骨(Skeleton::joints の番号)
        HitShape shape = HitShape::Box;

        DirectX::XMFLOAT3 center = { 0.0f, 0.0f, 0.0f };          // 骨の中での、中心の位置(骨の根もとが原点)
        DirectX::XMFLOAT4 rotation = { 0.0f, 0.0f, 0.0f, 1.0f };  // 骨の中での、箱の向き(クォータニオン)。球では使わない
        DirectX::XMFLOAT3 halfSize = { 0.5f, 0.5f, 0.5f };        // 箱の、半分の大きさ(骨の中の軸の向き。メートル)
        float radius = 0.5f;                                      // 球の半径(メートル)
    };

    // ある時刻の、世界の座標での形(判定に使う)。shape が Box なら box・Sphere なら sphere が、有効
    struct WorldHitbox
    {
        HitShape shape = HitShape::Box;
        OBB box;
        Sphere sphere;
    };

    // 判定の形を、世界の座標にする。結果は hitboxes と同じ数・同じ並び
    //   pose      : 骨の今の世界の行列(EvaluatePose の結果)
    //   modelWorld: モデル全体の置き方(DrawSkeleton の world と同じ。メッシュを描く行列)
    // 世界の行列 = (骨の中の向き・位置)× 骨の今の行列 × モデル全体の行列。拡大・縮小がかかっても、箱の軸は長さ 1 のまま・大きさに、拡大が反映される。
    // 球の半径には、いちばん大きい拡大が掛かる(一様な拡大だけ、正しい)。jointIndex が範囲の外なら、骨の動きなし(モデル全体の行列だけ)で置く
    std::vector<WorldHitbox> EvaluateHitboxes(const std::vector<Hitbox>& hitboxes, const std::vector<DirectX::XMFLOAT4X4>& pose,
                                              const DirectX::XMMATRIX& modelWorld);

    // 形の種類(箱・球)の、4 つの組み合わせを振り分けて、重なっているか調べる
    bool Intersects(const WorldHitbox& a, const WorldHitbox& b);

    // とりあえずの判定の形を、骨ごとに作る(あとで、調整する前提)
    //   子の骨がある骨 : 子の骨までを覆う箱(中心 = 子の位置の半分・各軸の半分の大きさ = max(距離の半分, thickness))
    //   子の骨が無い骨(先端): 半径 thickness の球
    std::vector<Hitbox> GenerateDefaultHitboxes(const Skeleton& skeleton, float thickness);
}
