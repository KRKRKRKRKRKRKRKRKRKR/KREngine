#pragma once

#include <DirectXMath.h>

#include "KR/Physics/Ray.h"

#include <array>
#include <optional>

namespace KR
{
    // 向きを持てる箱(OBB = Oriented Bounding Box)。当たり判定に使う
    // 回転しない箱も、軸を (1,0,0)(0,1,0)(0,0,1) にすれば、同じ形で持てる(同じ関数で判定できる)
    struct OBB
    {
        DirectX::XMFLOAT3 center = { 0.0f, 0.0f, 0.0f };   // 中心(世界の座標。メートル)
        DirectX::XMFLOAT3 axes[3] = {                      // 箱の 3 本の軸(右・上・前)。長さ 1・互いに直角
            { 1.0f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f },
        };
        DirectX::XMFLOAT3 halfSize = { 0.5f, 0.5f, 0.5f }; // 各軸の方向に、中心から面までの距離(半分の長さ。メートル)
    };

    // 回転行列から、OBB を作る。行列の 1 行目が右(X)、2 行目が上(Y)、3 行目が前(Z)の軸になる(DirectXMath の決まり)
    // 描画のワールド行列と同じ回転を渡せば、見た目と判定の向きがそろう。rotation には、移動・拡大を含めない
    OBB MakeOBB(const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT3& halfSize, DirectX::FXMMATRIX rotation);

    // 2 つの OBB が重なっているか(分離軸定理)
    // 15 本の軸(A の 3 軸・B の 3 軸・A の軸 × B の軸の外積 9 本)に、2 つの箱の影を落とし、
    // 1 本でも影にすき間があれば、当たっていない(そこで終わる)。全部の軸で影が重なっていれば、当たっている
    bool Intersects(const OBB& a, const OBB& b);

    // 球。中心と半径だけ(向きを持たない)。回しても形が変わらないので、動く骨の先端などに向く
    struct Sphere
    {
        DirectX::XMFLOAT3 center = { 0.0f, 0.0f, 0.0f }; // 中心(世界の座標。メートル)
        float radius = 0.5f;                             // 半径(メートル)
    };

    // 2 つの球が重なっているか:中心どうしの距離 ≦ 半径の和(ちょうど接するときも、当たり)
    bool Intersects(const Sphere& a, const Sphere& b);

    // 球と箱が重なっているか:箱の上で、球の中心に一番近い点を求め、その点までの距離 ≦ 半径(箱の中に入っていても、当たり)
    bool Intersects(const Sphere& sphere, const OBB& box);
    bool Intersects(const OBB& box, const Sphere& sphere);

    // レイが球に当たるか:当たれば、始点から当たった点までの距離 t(0 以上。direction の長さが 1 ならメートル)を返す。外れれば nullopt
    // 後ろ向き(球が始点の後ろ)は、外れ。始点が球の中なら、t = 0
    std::optional<float> Raycast(const Ray& ray, const Sphere& sphere);

    // レイが箱に当たるか:箱の軸ごとに、2 枚の面の間(スラブ)に入る t の範囲を求め、3 軸の範囲が重なれば当たり
    // 戻り値は Raycast(球)と同じ(始点が箱の中なら、t = 0)
    std::optional<float> Raycast(const Ray& ray, const OBB& box);

    // OBB の 8 つの角(中心 ± 右 × 半分 ± 上 × 半分 ± 前 × 半分)。箱を線で描くとき(Renderer::DrawWireBox)に使う
    // 番号の決め方: ビット 0 が右(1 なら +)、ビット 1 が上、ビット 2 が前
    std::array<DirectX::XMFLOAT3, 8> GetCorners(const OBB& box);
}
