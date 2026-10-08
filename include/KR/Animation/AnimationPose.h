#pragma once

#include "KR/Animation/AnimationClip.h"
#include "KR/Animation/Skeleton.h"

#include <DirectXMath.h>

#include <vector>

namespace KR
{
    // ある時刻の姿勢:各骨の、世界の行列(モデルの座標。Skeleton::joints と同じ並び)を求める
    //   ① 各骨の、親から見た位置・向き・大きさを決める:動き(clip)にキーがあれば、時刻 time の値(キーの間は補間)、無ければ基準姿勢
    //   ② 位置・向き・大きさから、行列を作る
    //   ③ 親の行列を掛けて、世界の行列にする(根の骨は、親のノード = Joint::parentWorld を掛ける)
    // time は秒。clip の長さをこえたら、最後のキーの値のまま(ループは、呼ぶ側で time を折り返す)。clip が nullptr なら、基準姿勢
    std::vector<DirectX::XMFLOAT4X4> EvaluatePose(const Skeleton& skeleton, const AnimationClip* clip, float time);
}
