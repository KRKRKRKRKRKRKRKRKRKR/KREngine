#pragma once

#include "KR/Physics/Ray.h"

#include <DirectXMath.h>

namespace KR
{
    // どこから・どこを・どれくらいの範囲で見るか(長さの単位はメートル)
    // Game が持ち、毎フレーム Renderer::SetCamera で渡す
    class Camera
    {
    public:
        void SetPosition(const DirectX::XMFLOAT3& position) { m_Position = position; }
        void LookAt(const DirectX::XMFLOAT3& target) { m_Target = target; }
        void SetFieldOfView(float degrees) { m_FieldOfViewDegrees = degrees; }

        // ロール(視線の向きを軸にした、カメラの傾き。度)。0 で水平。向きは、動かして確認する(逆なら、符号を反対にする)
        void SetRollDegrees(float degrees) { m_RollDegrees = degrees; }
        void SetClipPlanes(float nearZ, float farZ) { m_NearZ = nearZ; m_FarZ = farZ; }

        const DirectX::XMFLOAT3& GetPosition() const { return m_Position; }
        const DirectX::XMFLOAT3& GetTarget() const { return m_Target; }

        // ビュー行列: ワールド座標 → カメラから見た座標(カメラが原点、カメラの正面が +Z)
        DirectX::XMMATRIX GetViewMatrix() const;

        // プロジェクション行列: カメラから見た座標 → 画面の座標(遠近感と縦横比の補正)
        // 縦横比はウィンドウの大きさで決まるので、Renderer から受け取る
        DirectX::XMMATRIX GetProjectionMatrix(float aspectRatio) const;

        // 画面の点(x, y。左上が (0, 0)・単位は画素)から、世界へ伸びる光線を作る(マウスで、3D の物を選ぶのに使う)
        // width / height は、描いている画面(ビューポート)の大きさ(画素)。始点は、カメラの前の面(near)の上の点・向きは、長さ 1
        Ray ScreenPointToRay(float x, float y, float width, float height) const;

    private:
        DirectX::XMFLOAT3 m_Position = { 0.0f, 2.0f, -5.0f }; // 2メートル上、5メートル手前
        DirectX::XMFLOAT3 m_Target = { 0.0f, 0.0f, 0.0f };    // 原点を見る
        DirectX::XMFLOAT3 m_Up = { 0.0f, 1.0f, 0.0f };        // +Y が上(カメラが横に傾かないように)
        float m_FieldOfViewDegrees = 60.0f;                   // 上下の見える角度
        float m_RollDegrees = 0.0f;                           // ロール(視線を軸にした傾き)
        float m_NearZ = 0.1f;                                 // これより近い物は映らない(10センチ)
        float m_FarZ = 1000.0f;                               // これより遠い物は映らない(1キロメートル)
    };
}
