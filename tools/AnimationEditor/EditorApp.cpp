// KR エンジンのツール:アニメーションの調整ツール(KRAnimationEditor)
// ゲームのアセットのフォルダーを開いて、モデル(glTF)の骨・動き・判定の形を、見て、調整して、保存する。
// 保存した判定の形(<モデル名>.hitboxes.json)は、ゲームが AnimatedModel で読み込むだけ(ゲームに、調整の画面は、要らない)
//
//   起動:KRAnimationEditor.exe [--assets <アセットのフォルダー>] [--model <Models からの相対パス>]
//   引数なしなら、前回の続き(KRAnimationEditor.txt に、記憶している)
//   マウス:左ドラッグ = 回す / 中ドラッグ = 平行移動 / ホイール = 近づく・離れる(ImGui の窓の上では、動かさない)

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <KR/KR.h>

#include "FolderDialog.h"

#include <imgui.h>
#include <ImGuizmo.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <vector>

using namespace DirectX;

namespace
{
    // path ⇔ UTF-8 の文字列(設定のファイルや ImGui の文字欄は、UTF-8)
    std::string ToUtf8(const std::filesystem::path& path)
    {
        const std::u8string text = path.u8string();
        return std::string(reinterpret_cast<const char*>(text.data()), text.size());
    }

    std::filesystem::path FromUtf8(const std::string& text)
    {
        return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(text.data()), text.size()));
    }

    // exe のあるフォルダー(設定のファイルの置き場所)
    std::filesystem::path GetExeDirectory()
    {
        wchar_t exePath[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        return std::filesystem::path(exePath).parent_path();
    }

    constexpr size_t MaxRecentFolders = 8;

    // 形の中心(骨の中の座標)を、世界で worldDelta だけ動かしたときの、新しい中心(骨の中の座標)
    // 骨の中 → 世界の行列(骨の今の行列 × モデル全体の行列)で、世界の位置にして、動かして、逆行列で、骨の中に戻す
    XMFLOAT3 MoveCenterByWorldDelta(const XMFLOAT3& localCenter, const XMMATRIX& boneToWorld, const XMFLOAT3& worldDelta)
    {
        const XMVECTOR world = XMVectorAdd(XMVector3Transform(XMLoadFloat3(&localCenter), boneToWorld), XMLoadFloat3(&worldDelta));
        XMFLOAT3 result;
        XMStoreFloat3(&result, XMVector3Transform(world, XMMatrixInverse(nullptr, boneToWorld)));
        return result;
    }

    // コピーした形を、骨 targetJoint に貼る形にする:骨・名前だけ変えて、あとは、そのまま(骨の中の座標なので、別の骨でも、その骨の中で同じ位置)
    // 同じ骨に貼るときは、重なって見えなくならないよう、骨の中の X に shift だけずらす
    KR::Hitbox MakePastedHitbox(const KR::Hitbox& source, int targetJoint, const std::string& targetName, float shift)
    {
        KR::Hitbox pasted = source;
        pasted.jointIndex = targetJoint;
        pasted.name = targetName;
        pasted.center.x += shift;
        return pasted;
    }

    // 大きさのギズモ:世界の大きさが oldScale → newScale に変わったとき、骨の中の大きさ(半分の大きさ)の、新しい値
    // 軸ごとに、比(new / old)を掛ける。小さくしすぎて、形が消えないよう、最小値で止める
    constexpr float MinShapeSize = 0.01f;

    XMFLOAT3 ScaleHalfSize(const XMFLOAT3& half, const XMFLOAT3& oldScale, const XMFLOAT3& newScale)
    {
        auto scaled = [](float value, float oldValue, float newValue)
        {
            return oldValue > 1e-9f ? std::max(value * newValue / oldValue, MinShapeSize) : value;
        };
        return { scaled(half.x, oldScale.x, newScale.x), scaled(half.y, oldScale.y, newScale.y), scaled(half.z, oldScale.z, newScale.z) };
    }

    // 球:半径は 1 つだけ。3 軸のうち、一番大きく変わった軸の比を使う(中心のつまみ = 3 軸いっしょ・1 軸の矢印 = その軸だけ変わる)
    float ScaleRadius(float radius, float oldScale, const XMFLOAT3& newScale)
    {
        if (oldScale <= 1e-9f)
        {
            return radius;
        }
        float ratio = 1.0f;
        for (const float value : { newScale.x, newScale.y, newScale.z })
        {
            const float r = value / oldScale;
            if (std::abs(r - 1.0f) > std::abs(ratio - 1.0f))
            {
                ratio = r;
            }
        }
        return std::max(radius * ratio, MinShapeSize);
    }

    // クリックとドラッグの見分け:押してから離すまでに、押した点から 4 ピクセル以上離れたら、ドラッグ(途中で戻ってきても、ドラッグ)
    class ClickDetector
    {
    public:
        static constexpr float DragThreshold = 4.0f;

        void Press(float x, float y) { m_StartX = x; m_StartY = y; m_Dragged = false; }

        void Move(float x, float y)
        {
            const float dx = x - m_StartX;
            const float dy = y - m_StartY;
            if (dx * dx + dy * dy >= DragThreshold * DragThreshold)
            {
                m_Dragged = true;
            }
        }

        // ボタンを離した。ドラッグでなければ、クリック(true)
        bool Release(float x, float y)
        {
            Move(x, y);
            return !m_Dragged;
        }

    private:
        float m_StartX = 0.0f;
        float m_StartY = 0.0f;
        bool m_Dragged = false;
    };
}

class EditorApp : public KR::Application
{
public:
    EditorApp() : Application(KR::ApplicationDesc{ .title = "KR Animation Editor (KR Engine " + KR::GetVersionString() + ")" }) {}

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

        const KR::OBB box = KR::MakeOBB({ 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, XMMatrixIdentity());
        check("球×箱 面に近い(当たり)", KR::Intersects(KR::Sphere{ { 1.4f, 0.0f, 0.0f }, 0.5f }, box), true);
        check("球×箱 面から離れる", KR::Intersects(KR::Sphere{ { 1.6f, 0.0f, 0.0f }, 0.5f }, box), false);
        check("球×箱 角に近い(当たり)", KR::Intersects(KR::Sphere{ { 1.5f, 1.5f, 1.5f }, 0.9f }, box), true);
        check("球×箱 角から離れる", KR::Intersects(KR::Sphere{ { 1.5f, 1.5f, 1.5f }, 0.8f }, box), false);
        check("球×箱 箱の中に入っている", KR::Intersects(KR::Sphere{ { 0.2f, 0.1f, 0.0f }, 0.1f }, box), true);
        check("箱×球 引数の順を入れ替えても同じ", KR::Intersects(box, KR::Sphere{ { 1.6f, 0.0f, 0.0f }, 0.5f }), false);

        const KR::OBB rotated = KR::MakeOBB({ 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, XMMatrixRotationY(XM_PIDIV4));
        check("回した箱 角の先(当たり)", KR::Intersects(KR::Sphere{ { 1.7f, 0.0f, 0.0f }, 0.5f }, rotated), true);
        check("軸にそろえた箱 同じ点は外れ", KR::Intersects(KR::Sphere{ { 1.7f, 0.0f, 0.0f }, 0.5f }, box), false);

        // 画面の点 → レイ:カメラは (0, 0, -10) から、原点を見る(視野 60°・画面 1600 × 900)
        {
            KR::Camera camera;
            camera.SetPosition({ 0.0f, 0.0f, -10.0f });
            camera.LookAt({ 0.0f, 0.0f, 0.0f });
            auto nearEnough = [](float a, float b) { return std::abs(a - b) < 1e-3f; };

            const KR::Ray center = camera.ScreenPointToRay(800.0f, 450.0f, 1600.0f, 900.0f);
            check("レイ 画面の中心 → 正面", nearEnough(center.direction.x, 0.0f) && nearEnough(center.direction.y, 0.0f) && nearEnough(center.direction.z, 1.0f), true);
            check("レイ 始点は、カメラの前(near の面)", nearEnough(center.origin.z, -9.9f), true);

            // 縦の視野の半分の角度 30° の tan = 0.57735・横は、画面の縦横比 16:9 を掛ける
            const KR::Ray right = camera.ScreenPointToRay(1600.0f, 450.0f, 1600.0f, 900.0f);
            check("レイ 右端 → 右へ傾く(視野の端)", nearEnough(right.direction.x / right.direction.z, 0.57735f * 16.0f / 9.0f), true);
            const KR::Ray left = camera.ScreenPointToRay(0.0f, 450.0f, 1600.0f, 900.0f);
            check("レイ 左端 → 左へ傾く", nearEnough(left.direction.x / left.direction.z, -0.57735f * 16.0f / 9.0f), true);
            const KR::Ray top = camera.ScreenPointToRay(800.0f, 0.0f, 1600.0f, 900.0f);
            check("レイ 上端 → 上へ傾く(画面の y は、下向きに増える)", nearEnough(top.direction.y / top.direction.z, 0.57735f), true);
        }

        // レイ × 球・箱:原点の球(半径 1)・箱(半分 1)に、(0,0,-5) から +Z へ撃つ
        {
            auto nearEnough = [](float a, float b) { return std::abs(a - b) < 1e-4f; };
            auto hitAt = [&](const std::optional<float>& t, float expected)
            {
                return t.has_value() && nearEnough(*t, expected);
            };
            const KR::Ray forward{ { 0.0f, 0.0f, -5.0f }, { 0.0f, 0.0f, 1.0f } };
            const KR::Ray backward{ { 0.0f, 0.0f, -5.0f }, { 0.0f, 0.0f, -1.0f } };
            const KR::Ray aside{ { 0.0f, 2.0f, -5.0f }, { 0.0f, 0.0f, 1.0f } };

            check("レイ×球 真ん中に当たる(t = 4)", hitAt(KR::Raycast(forward, sphere), 4.0f), true);
            check("レイ×球 横にそれて外れる", KR::Raycast(aside, sphere).has_value(), false);
            check("レイ×球 後ろ向きは外れ", KR::Raycast(backward, sphere).has_value(), false);
            check("レイ×箱 真ん中に当たる(t = 4)", hitAt(KR::Raycast(forward, box), 4.0f), true);
            check("レイ×箱 横にそれて外れる", KR::Raycast(aside, box).has_value(), false);
            check("レイ×箱 後ろ向きは外れ", KR::Raycast(backward, box).has_value(), false);
            // 45° 回した箱:正面の角が √2 ≒ 1.4142 まで出る → t = 5 − 1.4142
            check("レイ×回した箱 角に当たる(t = 3.5858)", hitAt(KR::Raycast(forward, rotated), 5.0f - std::sqrt(2.0f)), true);
            // x = 1.2 のレイ:軸にそろえた箱(幅 ±1)は外れ・45° 回した箱(角が ±1.414)は、斜めの面に当たる
            const KR::Ray offset{ { 1.2f, 0.0f, -5.0f }, { 0.0f, 0.0f, 1.0f } };
            check("レイ×箱 x=1.2 軸にそろえた箱は外れ", KR::Raycast(offset, box).has_value(), false);
            check("レイ×回した箱 x=1.2 は当たる(面は |x|+|z| = √2 → t = 5 − (√2 − 1.2))", hitAt(KR::Raycast(offset, rotated), 5.0f - (std::sqrt(2.0f) - 1.2f)), true);
        }

        // レイ × 線分の最短距離:レイは (0,0,-5) から +Z へ
        {
            auto distanceIs = [](float actual, float expected) { return std::abs(actual - expected) < 1e-4f; };
            const KR::Ray ray{ { 0.0f, 0.0f, -5.0f }, { 0.0f, 0.0f, 1.0f } };
            check("レイ×線分 平行に離れる(距離 1)", distanceIs(KR::RayToSegmentDistance(ray, { 1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f }), 1.0f), true);
            check("レイ×線分 交わる(距離 0)", distanceIs(KR::RayToSegmentDistance(ray, { -1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }), 0.0f), true);
            check("レイ×線分 線分の端が一番近い(距離 3)", distanceIs(KR::RayToSegmentDistance(ray, { 0.0f, 3.0f, 0.0f }, { 0.0f, 5.0f, 0.0f }), 3.0f), true);
            check("レイ×線分 後ろにある(始点が一番近い・距離 3)", distanceIs(KR::RayToSegmentDistance(ray, { 0.0f, 0.0f, -8.0f }, { 0.0f, 1.0f, -8.0f }), 3.0f), true);
            float rayT = 0.0f;
            KR::RayToSegmentDistance(ray, { -1.0f, 2.0f, 0.0f }, { 1.0f, 2.0f, 0.0f }, &rayT);
            check("レイ×線分 レイ上の距離 t(t = 5)", distanceIs(rayT, 5.0f), true);
        }

        // ギズモで動かした量 → 骨の中の座標の変化
        {
            auto nearEnough = [](float a, float b) { return std::abs(a - b) < 1e-4f; };
            // 骨が Y 軸に 90° 回って、(1, 2, 3) にある。形の中心は、骨の中の (1, 0, 0)
            const XMMATRIX rotation = XMMatrixRotationY(XM_PIDIV2);
            const XMMATRIX boneToWorld = rotation * XMMatrixTranslation(1.0f, 2.0f, 3.0f);
            const XMFLOAT3 local{ 1.0f, 0.0f, 0.0f };

            const XMFLOAT3 none = MoveCenterByWorldDelta(local, boneToWorld, { 0.0f, 0.0f, 0.0f });
            check("ギズモ 動かさない → 中心は変わらない", nearEnough(none.x, 1.0f) && nearEnough(none.y, 0.0f) && nearEnough(none.z, 0.0f), true);

            // 世界で (0.5, 0, 0) 動かす → 骨の中では、回転の逆(−90°)をかけた向きに、同じ長さだけ動く
            const XMFLOAT3 delta{ 0.5f, 0.0f, 0.0f };
            const XMFLOAT3 moved = MoveCenterByWorldDelta(local, boneToWorld, delta);
            XMFLOAT3 expectedChange;
            XMStoreFloat3(&expectedChange, XMVector3TransformNormal(XMLoadFloat3(&delta), XMMatrixInverse(nullptr, rotation)));
            check("ギズモ 世界の +X 0.5 → 骨の中の変化 = 逆回転した向き",
                  nearEnough(moved.x - local.x, expectedChange.x) && nearEnough(moved.y - local.y, expectedChange.y) && nearEnough(moved.z - local.z, expectedChange.z), true);
            const float length = std::sqrt((moved.x - local.x) * (moved.x - local.x) + (moved.y - local.y) * (moved.y - local.y) + (moved.z - local.z) * (moved.z - local.z));
            check("ギズモ 動いた長さは、世界でも骨の中でも 0.5", nearEnough(length, 0.5f), true);
            // 90° 回した骨では、世界の +X は、骨の中の Z の向き(X は、変わらない)
            check("ギズモ 回した骨では、骨の中の X は変わらない", nearEnough(moved.x, 1.0f), true);
        }

        // 左右反転:左の骨(x = +1・Y 軸に 90° 回っている)と、右の骨(x = −1・Y 軸に −90°)
        {
            auto nearEnough = [](float a, float b) { return std::abs(a - b) < 1e-4f; };
            XMFLOAT4X4 leftBone;
            XMFLOAT4X4 rightBone;
            XMStoreFloat4x4(&leftBone, XMMatrixRotationY(XM_PIDIV2) * XMMatrixTranslation(1.0f, 0.0f, 0.0f));
            XMStoreFloat4x4(&rightBone, XMMatrixRotationY(-XM_PIDIV2) * XMMatrixTranslation(-1.0f, 0.0f, 0.0f));

            KR::Hitbox box;
            box.name = "Arm.L";
            box.jointIndex = 0;
            box.shape = KR::HitShape::Box;
            box.center = { 0.2f, 0.5f, 0.1f };
            XMStoreFloat4(&box.rotation, XMQuaternionRotationRollPitchYaw(0.3f, 0.5f, 0.2f)); // 斜めに回した箱(軸が、世界の軸に、そろわない)
            box.halfSize = { 0.1f, 0.2f, 0.3f };

            // 世界の形(骨の姿勢 = 左右の骨の行列・モデル全体の行列は、単位)
            const std::vector<XMFLOAT4X4> pose = { leftBone, rightBone };
            auto worldOf = [&](const KR::Hitbox& hitbox) { return KR::EvaluateHitboxes({ hitbox }, pose, XMMatrixIdentity())[0]; };

            // 箱 b の 8 つの角のそれぞれが、箱 a の角を X 反転した点のどれかに重なるか(角の集合が、鏡映しで一致するか)
            auto isMirrorImage = [&](const KR::OBB& a, const KR::OBB& b)
            {
                for (const XMFLOAT3& corner : KR::GetCorners(a))
                {
                    bool found = false;
                    for (const XMFLOAT3& other : KR::GetCorners(b))
                    {
                        if (nearEnough(-corner.x, other.x) && nearEnough(corner.y, other.y) && nearEnough(corner.z, other.z))
                        {
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                    {
                        return false;
                    }
                }
                return true;
            };

            const KR::Hitbox mirrored = KR::MirrorHitbox(box, leftBone, 1, rightBone);
            check("左右反転 箱:骨の番号が、対の骨になる", mirrored.jointIndex == 1, true);
            check("左右反転 箱:世界の形が、X = 0 の鏡映し(角が、すべて一致)", isMirrorImage(worldOf(box).box, worldOf(mirrored).box), true);
            const XMVECTOR q = XMLoadFloat4(&mirrored.rotation);
            check("左右反転 箱:向きは、普通の回転(クォータニオンの長さ 1・軸の行列式 +1)",
                  nearEnough(XMVectorGetX(XMVector4Length(q)), 1.0f) && XMVectorGetX(XMMatrixDeterminant(XMMatrixRotationQuaternion(q))) > 0.0f, true);
            check("左右反転 箱:半分の大きさは、変わらない",
                  nearEnough(mirrored.halfSize.x, 0.1f) && nearEnough(mirrored.halfSize.y, 0.2f) && nearEnough(mirrored.halfSize.z, 0.3f), true);

            const KR::Hitbox back = KR::MirrorHitbox(mirrored, rightBone, 0, leftBone);
            const KR::WorldHitbox original = worldOf(box);
            const KR::WorldHitbox twice = worldOf(back);
            check("左右反転 箱:2 回反転で、もとの場所に戻る(中心)",
                  nearEnough(original.box.center.x, twice.box.center.x) && nearEnough(original.box.center.y, twice.box.center.y) && nearEnough(original.box.center.z, twice.box.center.z), true);
            check("左右反転 箱:2 回反転で、もとの形に戻る(角が、すべて一致)", isMirrorImage(original.box, [&] { KR::OBB m = twice.box; m.center.x = -m.center.x; for (auto& a : m.axes) { a.x = -a.x; } return m; }()), true);
            check("左右反転 箱:2 回反転で、骨の中の座標も戻る", nearEnough(back.center.x, box.center.x) && nearEnough(back.center.y, box.center.y) && nearEnough(back.center.z, box.center.z), true);

            // 球:中心だけが鏡映し・半径は同じ。左の骨の x = 1 の先 → 世界 x = 1 + ... を、右の骨の中で
            KR::Hitbox sphere;
            sphere.jointIndex = 0;
            sphere.shape = KR::HitShape::Sphere;
            sphere.center = { 0.0f, 0.0f, 0.5f };
            sphere.radius = 0.25f;
            const KR::WorldHitbox sphereWorld = worldOf(sphere);
            const KR::WorldHitbox mirroredSphere = worldOf(KR::MirrorHitbox(sphere, leftBone, 1, rightBone));
            check("左右反転 球:中心が X = 0 の鏡映し・半径は同じ",
                  nearEnough(-sphereWorld.sphere.center.x, mirroredSphere.sphere.center.x) && nearEnough(sphereWorld.sphere.center.y, mirroredSphere.sphere.center.y)
                      && nearEnough(sphereWorld.sphere.center.z, mirroredSphere.sphere.center.z) && nearEnough(mirroredSphere.sphere.radius, 0.25f), true);
        }

        // 対になる骨の名前
        {
            auto pairIs = [&](const char* name, const char* expected)
            {
                const std::optional<std::string> result = KR::MirrorBoneName(name);
                check(name, result.has_value() && *result == expected, true);
            };
            pairIs("Arm.L", "Arm.R");
            pairIs("Leg_R", "Leg_L");
            pairIs("Arm.L.001", "Arm.R.001");
            pairIs("mixamorig:LeftHand", "mixamorig:RightHand");
            pairIs("RightFoot", "LeftFoot");
            pairIs("左腕", "右腕");
            check("対の名前 中央の骨(Spine)は、見つからない", KR::MirrorBoneName("Spine").has_value(), false);
            check("対の名前 Hand_Bone(L や R で終わらない)は、見つからない", KR::MirrorBoneName("Hand_Bone").has_value(), false);

            KR::Skeleton skeleton;
            for (const char* name : { "Spine", "Arm.L", "Arm.R", "Head.L" })
            {
                KR::Joint joint;
                joint.name = name;
                skeleton.joints.push_back(joint);
            }
            check("対の骨 Arm.L → Arm.R(番号 2)", KR::FindMirrorJoint(skeleton, 1) == std::optional<int>(2), true);
            check("対の骨 Arm.R → Arm.L(番号 1)", KR::FindMirrorJoint(skeleton, 2) == std::optional<int>(1), true);
            check("対の骨 Head.L は、Head.R が骨に無いので、見つからない", KR::FindMirrorJoint(skeleton, 3).has_value(), false);
            check("対の骨 Spine は、見つからない", KR::FindMirrorJoint(skeleton, 0).has_value(), false);
        }

        // コピー・貼り付け
        {
            auto nearEnough = [](float a, float b) { return std::abs(a - b) < 1e-5f; };
            KR::Hitbox source;
            source.name = "Arm";
            source.jointIndex = 1;
            source.shape = KR::HitShape::Sphere;
            source.center = { 0.5f, 1.0f, 2.0f };
            source.radius = 0.3f;

            const KR::Hitbox other = MakePastedHitbox(source, 2, "Hand", 0.0f);
            check("貼り付け 別の骨:骨・名前が変わり、形・位置・大きさは同じ",
                  other.jointIndex == 2 && other.name == "Hand" && other.shape == KR::HitShape::Sphere && nearEnough(other.center.x, 0.5f)
                      && nearEnough(other.center.y, 1.0f) && nearEnough(other.center.z, 2.0f) && nearEnough(other.radius, 0.3f), true);
            const KR::Hitbox same = MakePastedHitbox(source, 1, "Arm", 0.1f);
            check("貼り付け 同じ骨:X だけ 0.1 ずれる", same.jointIndex == 1 && nearEnough(same.center.x, 0.6f) && nearEnough(same.center.y, 1.0f), true);
            check("貼り付け もとの形は、変わらない", nearEnough(source.center.x, 0.5f) && source.jointIndex == 1 && source.name == "Arm", true);
        }

        // 大きさのギズモ:世界の大きさの変化 → 骨の中の大きさ
        {
            auto nearEnough = [](float a, float b) { return std::abs(a - b) < 1e-4f; };
            // 骨の中の半分の大きさ (1, 2, 3)・世界では 2 倍(モデルが 2 倍に拡大されている)→ 世界 X を 2 → 4 にすると、骨の中の X も 2 倍
            const XMFLOAT3 half = ScaleHalfSize({ 1.0f, 2.0f, 3.0f }, { 2.0f, 4.0f, 6.0f }, { 4.0f, 4.0f, 6.0f });
            check("大きさ 箱:X だけ 2 倍(他は不変)", nearEnough(half.x, 2.0f) && nearEnough(half.y, 2.0f) && nearEnough(half.z, 3.0f), true);
            const XMFLOAT3 tiny = ScaleHalfSize({ 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, -1.0f, 0.001f });
            check("大きさ 箱:小さくしすぎても、最小値(0.01)で止まる", nearEnough(tiny.x, MinShapeSize) && nearEnough(tiny.y, MinShapeSize) && nearEnough(tiny.z, MinShapeSize), true);
            check("大きさ 球:3 軸いっしょに 1.5 倍 → 半径 1.5 倍", nearEnough(ScaleRadius(1.0f, 1.0f, { 1.5f, 1.5f, 1.5f }), 1.5f), true);
            check("大きさ 球:1 軸だけ 0.5 倍 → 半径 0.5 倍", nearEnough(ScaleRadius(2.0f, 1.0f, { 1.0f, 0.5f, 1.0f }), 1.0f), true);
            check("大きさ 球:小さくしすぎても、最小値で止まる", nearEnough(ScaleRadius(1.0f, 1.0f, { 0.0f, 1.0f, 1.0f }), MinShapeSize), true);
        }

        // クリックとドラッグの見分け
        {
            ClickDetector detector;
            detector.Press(100.0f, 100.0f);
            check("クリック 動かない(クリック)", detector.Release(100.0f, 100.0f), true);
            detector.Press(100.0f, 100.0f);
            detector.Move(102.0f, 102.0f);
            check("クリック 3 ピクセル弱の動き(クリック)", detector.Release(102.0f, 102.0f), true);
            detector.Press(100.0f, 100.0f);
            check("クリック 4 ピクセルの動き(ドラッグ)", detector.Release(104.0f, 100.0f), false);
            detector.Press(100.0f, 100.0f);
            detector.Move(130.0f, 100.0f);
            check("クリック 遠くへ行って戻る(ドラッグ)", detector.Release(100.0f, 100.0f), false);
            detector.Press(50.0f, 50.0f);
            check("クリック 次の押下で、ドラッグの記憶が消える(クリック)", detector.Release(50.0f, 50.0f), true);
        }

        if (failures == 0)
        {
            KR_LOG_INFO("  判定の確認: OK(球×球・球×箱・回した箱・画面の点からのレイ・レイ×球・箱・レイ×線分・ギズモの移動量・大きさ・左右反転・対の骨の名前・コピー貼り付け・クリックの見分け)");
        }
    }

    // FBX のメッシュの確認(F3):同じモデルの .glb と .fbx を読んで、形が、そろっているか調べる(Models からの、拡張子なしの相対パス)
    //   三角形の数・大きさ(幅・高さ・奥行き。メートル)・表裏(三角形の向きと、法線の向きが、そろっている三角形の割合)
    // 置き場所(位置)は、比べない(FBX は、Blender の置き場所を、そのまま持つため。骨を読む F4 で、扱いを決める)
    static void CheckFbxMesh(const std::filesystem::path& modelPath)
    {
        const KR::ModelData gltf = KR::LoadGltf(std::filesystem::path(modelPath).concat(".glb"));
        const KR::ModelData fbx = KR::LoadFbx(std::filesystem::path(modelPath).concat(".fbx"));
        if (gltf.IsEmpty() || fbx.IsEmpty())
        {
            KR_LOG_WARN("  FBX の確認: {} の .glb か .fbx が、読めないので、飛ばします", modelPath.string());
            return;
        }

        struct Shape
        {
            XMFLOAT3 size;
            float outwardRatio; // 三角形の向き(外積)と、頂点の法線が、同じ向きの三角形の割合(1 に近いほど、表裏が合っている)
        };
        auto measure = [](const KR::MeshData& mesh)
        {
            XMFLOAT3 minimum = mesh.vertices[0].position;
            XMFLOAT3 maximum = minimum;
            for (const KR::Vertex& vertex : mesh.vertices)
            {
                minimum = { std::min(minimum.x, vertex.position.x), std::min(minimum.y, vertex.position.y), std::min(minimum.z, vertex.position.z) };
                maximum = { std::max(maximum.x, vertex.position.x), std::max(maximum.y, vertex.position.y), std::max(maximum.z, vertex.position.z) };
            }
            size_t same = 0;
            const size_t triangles = mesh.indices.size() / 3;
            for (size_t t = 0; t < triangles; ++t)
            {
                const KR::Vertex& a = mesh.vertices[mesh.indices[t * 3]];
                const KR::Vertex& b = mesh.vertices[mesh.indices[t * 3 + 1]];
                const KR::Vertex& c = mesh.vertices[mesh.indices[t * 3 + 2]];
                const XMVECTOR normal = XMVector3Cross(XMVectorSubtract(XMLoadFloat3(&b.position), XMLoadFloat3(&a.position)),
                                                       XMVectorSubtract(XMLoadFloat3(&c.position), XMLoadFloat3(&a.position)));
                same += (XMVectorGetX(XMVector3Dot(normal, XMLoadFloat3(&a.normal))) > 0.0f) ? 1 : 0;
            }
            return Shape{ { maximum.x - minimum.x, maximum.y - minimum.y, maximum.z - minimum.z }, static_cast<float>(same) / static_cast<float>(std::max<size_t>(triangles, 1)) };
        };

        const Shape a = measure(gltf.mesh);
        const Shape b = measure(fbx.mesh);
        auto close = [](float x, float y) { return std::abs(x - y) < 0.01f; };
        const bool trianglesMatch = gltf.mesh.indices.size() == fbx.mesh.indices.size();
        const bool sizeMatch = close(a.size.x, b.size.x) && close(a.size.y, b.size.y) && close(a.size.z, b.size.z);
        // 表裏:glTF の割合と、FBX の割合が、近ければ OK(どちらも、同じ決まりの表裏)。向きが逆なら、割合が、1 − ほぼ
        const bool windingMatch = std::abs(a.outwardRatio - b.outwardRatio) < 0.05f;

        if (trianglesMatch && sizeMatch && windingMatch)
        {
            KR_LOG_INFO("  FBX メッシュの確認: OK(glTF と一致:三角形 {}・大きさ {:.2f} × {:.2f} × {:.2f} m・表裏の割合 glTF {:.2f} / FBX {:.2f})",
                        fbx.mesh.indices.size() / 3, b.size.x, b.size.y, b.size.z, a.outwardRatio, b.outwardRatio);
        }
        else
        {
            KR_LOG_ERROR("  FBX メッシュの確認 NG: 三角形 glTF {} / FBX {}・大きさ glTF ({:.3f}, {:.3f}, {:.3f}) / FBX ({:.3f}, {:.3f}, {:.3f})・表裏の割合 glTF {:.2f} / FBX {:.2f}",
                         gltf.mesh.indices.size() / 3, fbx.mesh.indices.size() / 3, a.size.x, a.size.y, a.size.z, b.size.x, b.size.y, b.size.z, a.outwardRatio, b.outwardRatio);
        }

        // 骨(F4):数・名前・親が、glTF と同じか。基準姿勢が、逆バインド行列と合っているか。骨どうしの相対位置が、glTF と同じか
        const KR::Skeleton& gs = gltf.skeleton;
        const KR::Skeleton& fs = fbx.skeleton;
        bool skeletonOk = gs.joints.size() == fs.joints.size();
        std::string detail;
        const std::vector<XMFLOAT4X4> gPose = KR::EvaluatePose(gs, nullptr, 0.0f);
        const std::vector<XMFLOAT4X4> fPose = KR::EvaluatePose(fs, nullptr, 0.0f);
        auto indexOf = [](const KR::Skeleton& s, const std::string& name) -> int
        {
            for (size_t i = 0; i < s.joints.size(); ++i)
            {
                if (s.joints[i].name == name)
                {
                    return static_cast<int>(i);
                }
            }
            return -1;
        };
        auto rootOf = [](const KR::Skeleton& s) -> int
        {
            for (size_t i = 0; i < s.joints.size(); ++i)
            {
                if (s.joints[i].parent < 0)
                {
                    return static_cast<int>(i);
                }
            }
            return -1;
        };
        const int gRoot = rootOf(gs);
        const int fRoot = rootOf(fs);
        if (skeletonOk && gRoot >= 0 && fRoot >= 0)
        {
            const XMFLOAT4X4& gr = gPose[static_cast<size_t>(gRoot)];
            const XMFLOAT4X4& fr = fPose[static_cast<size_t>(fRoot)];
            for (const KR::Joint& joint : gs.joints)
            {
                const int j = indexOf(fs, joint.name);
                if (j < 0)
                {
                    skeletonOk = false;
                    detail += " 骨「" + joint.name + "」が FBX に無い";
                    continue;
                }
                const KR::Joint& other = fs.joints[static_cast<size_t>(j)];
                const std::string gParent = joint.parent >= 0 ? gs.joints[static_cast<size_t>(joint.parent)].name : "";
                const std::string fParent = other.parent >= 0 ? fs.joints[static_cast<size_t>(other.parent)].name : "";
                if (gParent != fParent)
                {
                    skeletonOk = false;
                    detail += " 「" + joint.name + "」の親が違う";
                }
                // 根の骨からの相対位置(置き場所の違いを、打ち消す)
                const XMFLOAT4X4& gw = gPose[static_cast<size_t>(&joint - gs.joints.data())];
                const XMFLOAT4X4& fw = fPose[static_cast<size_t>(j)];
                if (!close(gw._41 - gr._41, fw._41 - fr._41) || !close(gw._42 - gr._42, fw._42 - fr._42) || !close(gw._43 - gr._43, fw._43 - fr._43))
                {
                    skeletonOk = false;
                    detail += " 「" + joint.name + "」の相対位置が違う";
                }
            }
        }
        // 基準姿勢 × 逆バインド = 単位行列(骨の世界の行列と、逆バインドが、矛盾していない)
        float worstBind = 0.0f;
        for (size_t i = 0; i < fs.joints.size(); ++i)
        {
            const XMMATRIX product = XMLoadFloat4x4(&fPose[i]) * XMLoadFloat4x4(&fs.joints[i].inverseBind);
            XMFLOAT4X4 p;
            XMStoreFloat4x4(&p, product);
            const float* e = &p._11;
            for (int k = 0; k < 16; ++k)
            {
                worstBind = std::max(worstBind, std::abs(e[k] - ((k % 5 == 0) ? 1.0f : 0.0f)));
            }
        }
        if (worstBind > 0.01f)
        {
            skeletonOk = false;
            detail += " 基準姿勢 × 逆バインドが、単位行列と合わない(最大のずれ " + std::to_string(worstBind) + ")";
        }

        if (skeletonOk)
        {
            const XMFLOAT4X4& fr = fPose[static_cast<size_t>(fRoot)];
            const XMFLOAT4X4& gr = gPose[static_cast<size_t>(gRoot)];
            KR_LOG_INFO("  FBX 骨の確認: OK(骨 {} 本・名前と親が glTF と一致・骨どうしの相対位置が一致・基準姿勢 × 逆バインド = 単位行列(最大のずれ {:.5f}))。根の骨の置き場所:glTF ({:.2f}, {:.2f}, {:.2f}) / FBX ({:.2f}, {:.2f}, {:.2f})",
                        fs.joints.size(), worstBind, gr._41, gr._42, gr._43, fr._41, fr._42, fr._43);
        }
        else
        {
            KR_LOG_ERROR("  FBX 骨の確認 NG: 骨の数 glTF {} / FBX {}。{}", gs.joints.size(), fs.joints.size(), detail);
        }

        // 骨の重み(F5):頂点と同じ数・重みの合計が 1・骨の番号が範囲の中・骨ごとの重みの合計が、glTF と同じか
        {
            const KR::MeshData& gm = gltf.mesh;
            const KR::MeshData& fm = fbx.mesh;
            bool skinOk = fm.skin.size() == fm.vertices.size() && gm.skin.size() == gm.vertices.size() && !fm.skin.empty();
            std::string skinDetail;
            auto totals = [](const KR::MeshData& mesh, const KR::Skeleton& skeleton, float& worstSum, int& maxJoint)
            {
                std::map<std::string, float> perBone;
                worstSum = 0.0f;
                maxJoint = -1;
                for (const KR::SkinVertex& v : mesh.skin)
                {
                    float sum = 0.0f;
                    for (int k = 0; k < 4; ++k)
                    {
                        sum += v.weights[k];
                        if (v.weights[k] > 0.0f)
                        {
                            maxJoint = std::max(maxJoint, static_cast<int>(v.joints[k]));
                            if (v.joints[k] < skeleton.joints.size())
                            {
                                perBone[skeleton.joints[v.joints[k]].name] += v.weights[k];
                            }
                        }
                    }
                    worstSum = std::max(worstSum, std::abs(sum - 1.0f));
                }
                return perBone;
            };
            float gSum = 0.0f, fSum = 0.0f;
            int gMax = -1, fMax = -1;
            const std::map<std::string, float> gTotals = totals(gm, gs, gSum, gMax);
            const std::map<std::string, float> fTotals = totals(fm, fs, fSum, fMax);
            if (fSum > 1e-3f)
            {
                skinOk = false;
                skinDetail += " 重みの合計が 1 でない頂点がある(最大のずれ " + std::to_string(fSum) + ")";
            }
            if (fMax >= static_cast<int>(fs.joints.size()))
            {
                skinOk = false;
                skinDetail += " 骨の番号が範囲の外";
            }
            for (const auto& [name, weight] : gTotals)
            {
                const auto other = fTotals.find(name);
                const float fw = other == fTotals.end() ? 0.0f : other->second;
                if (std::abs(weight - fw) > 0.5f) // 頂点 1536 個の合計。丸めの誤差だけ許す
                {
                    skinOk = false;
                    skinDetail += " 骨「" + name + "」の重みの合計が違う(glTF " + std::to_string(weight) + " / FBX " + std::to_string(fw) + ")";
                }
            }
            if (skinOk)
            {
                std::string text;
                for (const auto& [name, weight] : fTotals)
                {
                    text += (text.empty() ? "" : "・") + name + " " + std::to_string(static_cast<int>(weight + 0.5f));
                }
                KR_LOG_INFO("  FBX 重みの確認: OK(頂点 {} 個・重みの合計 1(最大のずれ {:.6f})・骨ごとの重みの合計が glTF と一致:{})", fm.skin.size(), fSum, text);
            }
            else
            {
                KR_LOG_ERROR("  FBX 重みの確認 NG:{}", skinDetail);
            }
        }

        // 動き(F6):数・長さ・動く骨の数が、glTF と同じか。いくつかの時刻の姿勢(骨の位置)が、glTF と近いか
        // (FBX は、1 秒 30 回の Linear に焼き付けた物。glTF は、元のキー。補間のしかたの差だけ、許す)
        {
            bool clipOk = gltf.clips.size() == fbx.clips.size() && !fbx.clips.empty();
            std::string clipDetail;
            float worstDiff = 0.0f;
            if (clipOk)
            {
                const KR::AnimationClip& gc = gltf.clips[0];
                const KR::AnimationClip& fc = fbx.clips[0];
                if (std::abs(gc.duration - fc.duration) > 0.05f)
                {
                    clipOk = false;
                    clipDetail += " 長さ glTF " + std::to_string(gc.duration) + " / FBX " + std::to_string(fc.duration);
                }
                if (gc.tracks.size() != fc.tracks.size())
                {
                    clipOk = false;
                    clipDetail += " 動く骨の数 glTF " + std::to_string(gc.tracks.size()) + " / FBX " + std::to_string(fc.tracks.size());
                }
                // 置き場所の違い(根の骨の、基準姿勢の位置の差)を、打ち消して、骨ごとの世界の位置を、比べる
                const std::vector<XMFLOAT4X4> gBase = KR::EvaluatePose(gs, nullptr, 0.0f);
                const std::vector<XMFLOAT4X4> fBase = KR::EvaluatePose(fs, nullptr, 0.0f);
                const XMFLOAT3 offset = { fBase[static_cast<size_t>(fRoot)]._41 - gBase[static_cast<size_t>(gRoot)]._41,
                                          fBase[static_cast<size_t>(fRoot)]._42 - gBase[static_cast<size_t>(gRoot)]._42,
                                          fBase[static_cast<size_t>(fRoot)]._43 - gBase[static_cast<size_t>(gRoot)]._43 };
                for (const float time : { 0.0f, 0.3f, 0.7f, 1.0f, 1.3f })
                {
                    const std::vector<XMFLOAT4X4> gp = KR::EvaluatePose(gs, &gc, time);
                    const std::vector<XMFLOAT4X4> fp = KR::EvaluatePose(fs, &fc, time);
                    for (size_t i = 0; i < gs.joints.size(); ++i)
                    {
                        const int j = indexOf(fs, gs.joints[i].name);
                        if (j < 0)
                        {
                            continue;
                        }
                        const XMFLOAT4X4& a2 = gp[i];
                        const XMFLOAT4X4& b2 = fp[static_cast<size_t>(j)];
                        const float diff = std::max({ std::abs(a2._41 - (b2._41 - offset.x)), std::abs(a2._42 - (b2._42 - offset.y)), std::abs(a2._43 - (b2._43 - offset.z)) });
                        worstDiff = std::max(worstDiff, diff);
                    }
                }
                if (worstDiff > 0.15f)
                {
                    clipOk = false;
                    clipDetail += " 骨の位置が、glTF と合わない(最大のずれ " + std::to_string(worstDiff) + " m)";
                }
            }
            else
            {
                clipDetail += " 動きの数 glTF " + std::to_string(gltf.clips.size()) + " / FBX " + std::to_string(fbx.clips.size());
            }
            if (clipOk)
            {
                KR_LOG_INFO("  FBX 動きの確認: OK(動き {} 個「{}」・長さ {:.3f} 秒・動く骨 {} 本・5 つの時刻の骨の位置が glTF と一致(最大のずれ {:.4f} m))",
                            fbx.clips.size(), fbx.clips[0].name, fbx.clips[0].duration, fbx.clips[0].tracks.size(), worstDiff);
            }
            else
            {
                KR_LOG_ERROR("  FBX 動きの確認 NG:{}", clipDetail);
            }
        }

        // 振り分け(F7b):LoadModel が、拡張子で、glTF・FBX を選ぶ(大文字の拡張子でも)。LoadGltf / LoadFbx を直接呼んだ結果と、同じ形か
        {
            const KR::ModelData viaGlb = KR::LoadModel(std::filesystem::path(modelPath).concat(".glb"));
            const KR::ModelData viaFbx = KR::LoadModel(std::filesystem::path(modelPath).concat(".fbx"));
            const KR::ModelData viaUpper = KR::LoadModel(std::filesystem::path(modelPath).concat(".FBX"));
            const bool dispatchOk = viaGlb.mesh.vertices.size() == gltf.mesh.vertices.size() && viaGlb.skeleton.joints.size() == gltf.skeleton.joints.size()
                                 && viaFbx.mesh.vertices.size() == fbx.mesh.vertices.size() && viaFbx.skeleton.joints.size() == fbx.skeleton.joints.size()
                                 && viaUpper.mesh.vertices.size() == fbx.mesh.vertices.size() && !viaGlb.IsEmpty() && !viaFbx.IsEmpty();
            if (dispatchOk)
            {
                KR_LOG_INFO("  LoadModel の確認: OK(.glb → LoadGltf・.fbx / .FBX → LoadFbx と同じ結果:頂点 {} / {}・骨 {} / {})",
                            viaGlb.mesh.vertices.size(), viaFbx.mesh.vertices.size(), viaGlb.skeleton.joints.size(), viaFbx.skeleton.joints.size());
            }
            else
            {
                KR_LOG_ERROR("  LoadModel の確認 NG: .glb の頂点 {}(LoadGltf {})・.fbx の頂点 {}(LoadFbx {})・.FBX の頂点 {}", viaGlb.mesh.vertices.size(),
                             gltf.mesh.vertices.size(), viaFbx.mesh.vertices.size(), fbx.mesh.vertices.size(), viaUpper.mesh.vertices.size());
            }
        }
    }

    void OnInit() override
    {
        RunCollisionSelfTest();
        CheckFbxMesh("Test/BossObstacle/BossObstacle"); // F3 の確認:FBX のメッシュが、同じモデルの glTF と、そろっているか
        m_BoneRenderer.Create(GetRenderer());

        // 開くフォルダー・モデルの決め方:コマンドラインの引数 → 前回の記憶 → 既定(ビルドで決まった、アセットのフォルダー)
        LoadSettings();
        std::filesystem::path folder = m_LastFolder;
        std::filesystem::path model = m_LastModel;

        const std::vector<std::filesystem::path> arguments = KR::GetCommandLineArguments();
        for (size_t i = 1; i + 1 < arguments.size(); ++i)
        {
            if (arguments[i] == L"--assets")
            {
                folder = arguments[i + 1];
                model.clear(); // フォルダーを指定したら、前回のモデルは、使わない(--model で指定する)
            }
        }
        for (size_t i = 1; i + 1 < arguments.size(); ++i)
        {
            if (arguments[i] == L"--model")
            {
                model = arguments[i + 1];
            }
        }

        if (folder.empty() || !std::filesystem::is_directory(folder / "Models"))
        {
            folder = KR::GetAssetDirectory("Models").parent_path(); // 既定
        }
        ApplyFolder(folder);

        // 指定されたモデルがあれば、それを。無ければ、一覧の最初のモデルを
        if (!model.empty())
        {
            ApplyModel(model);
        }
        else if (!m_Models.empty())
        {
            ApplyModel(FromUtf8(m_Models.front()));
        }
    }

    void OnUpdate(float deltaTime) override
    {
        if (m_Rotate)
        {
            m_Angle = std::fmod(m_Angle + deltaTime * 0.5f, 2.0f * std::numbers::pi_v<float>);
        }
        UpdateCamera();
        UpdateClick();
        m_Model.Update(deltaTime); // 時刻を進めて、姿勢を求める
    }

    // 左ボタンのクリック(ドラッグでない)を検出する。ImGui の上で押したものは、数えない
    void UpdateClick()
    {
        const ImGuiIO& io = ImGui::GetIO();
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            m_ClickTracking = !io.WantCaptureMouse && !m_GizmoBusy;
            m_ClickDetector.Press(io.MousePos.x, io.MousePos.y);
        }
        if (!m_ClickTracking)
        {
            return;
        }
        m_ClickDetector.Move(io.MousePos.x, io.MousePos.y);
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            m_ClickTracking = false;
            if (m_ClickDetector.Release(io.MousePos.x, io.MousePos.y))
            {
                PickHitbox(io.MousePos.x, io.MousePos.y);
            }
        }
    }

    // モデル全体の置き方(形の中心を原点に動かして、Y 軸で回す)。描くときも、選ぶときも、同じ行列を使う
    XMMATRIX GetModelWorld() const
    {
        return XMMatrixTranslation(-m_Center.x, -m_Center.y, -m_Center.z) * XMMatrixRotationY(m_Angle);
    }

    // 画面の点 (x, y) をクリックしたとき、一番手前に当たる判定の形を選ぶ(骨の選択と連動:その骨の、その形になる)
    void PickHitbox(float x, float y)
    {
        if (!m_Model.IsLoaded() || !m_ShowHitboxes)
        {
            return; // 見えていない形は、選べない
        }
        const ImVec2 size = ImGui::GetIO().DisplaySize;
        const KR::Ray ray = m_Camera.ScreenPointToRay(x, y, size.x, size.y);
        const std::vector<KR::WorldHitbox> shapes = m_Model.GetWorldHitboxes(GetModelWorld());

        size_t nearest = NoHitbox;
        float nearestT = std::numeric_limits<float>::max();
        for (size_t i = 0; i < shapes.size(); ++i)
        {
            const std::optional<float> t = (shapes[i].shape == KR::HitShape::Box) ? KR::Raycast(ray, shapes[i].box) : KR::Raycast(ray, shapes[i].sphere);
            if (t && *t < nearestT)
            {
                nearest = i;
                nearestT = *t;
            }
        }
        if (nearest == NoHitbox)
        {
            PickBone(ray, x, y); // 形に当たらなければ、骨を探す(形と骨が重なるときは、形を優先)
            return;
        }

        // 骨の選択に直す:その形の骨を選び、その骨の何番目の形か(m_SubHitbox)を決める
        const std::vector<KR::Hitbox>& hitboxes = m_Model.GetHitboxes();
        const int joint = hitboxes[nearest].jointIndex;
        if (joint < 0 || joint >= static_cast<int>(m_Model.GetSkeleton().joints.size()))
        {
            KR_LOG_WARN("  クリック: 形 {} は、骨に付いていないので、選べません", nearest);
            return;
        }
        int sub = 0;
        for (size_t i = 0; i < nearest; ++i)
        {
            if (hitboxes[i].jointIndex == joint)
            {
                ++sub;
            }
        }
        m_SelectedBone = joint;
        m_SubHitbox = sub;
        KR_LOG_INFO("  クリック({:.0f}, {:.0f}): 形 {} を選びました(骨 {}「{}」の {} 番目・t = {:.2f})", x, y, nearest, joint,
                    m_Model.GetSkeleton().joints[static_cast<size_t>(joint)].name, sub, nearestT);
    }

    // クリックの光線に、一番近い骨を選ぶ(骨の線分への最短距離が、しきい値より近い物のうち、最も近い物)
    // 骨は、描いているとおり、「親の関節 → 子の関節」の線分で、親の関節の骨になる。先端(子が無い)の骨は、親から来た向きの先に、3 割の長さ
    void PickBone(const KR::Ray& ray, float x, float y)
    {
        const KR::Skeleton& skeleton = m_Model.GetSkeleton();
        const std::vector<XMFLOAT4X4>& pose = m_Model.GetPose();
        const size_t count = std::min(skeleton.joints.size(), pose.size());
        const XMMATRIX world = GetModelWorld();

        std::vector<XMVECTOR> position(count);
        std::vector<bool> hasChild(count, false);
        for (size_t i = 0; i < count; ++i)
        {
            position[i] = XMVector3Transform(XMLoadFloat4x4(&pose[i]).r[3], world);
            const int parent = skeleton.joints[i].parent;
            if (parent >= 0 && static_cast<size_t>(parent) < count)
            {
                hasChild[static_cast<size_t>(parent)] = true;
            }
        }

        const float threshold = std::max(m_ModelRadius, 0.1f) * 0.06f; // 線から、この距離(メートル)以内なら、骨に触れたとみなす
        int bestJoint = -1;
        float bestDistance = threshold;
        auto consider = [&](int joint, FXMVECTOR head, FXMVECTOR tail)
        {
            XMFLOAT3 a;
            XMFLOAT3 b;
            XMStoreFloat3(&a, head);
            XMStoreFloat3(&b, tail);
            const float distance = KR::RayToSegmentDistance(ray, a, b);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestJoint = joint;
            }
        };
        for (size_t i = 0; i < count; ++i)
        {
            const int parent = skeleton.joints[i].parent;
            if (parent < 0 || static_cast<size_t>(parent) >= count)
            {
                continue;
            }
            const size_t p = static_cast<size_t>(parent);
            consider(parent, position[p], position[i]);
            if (!hasChild[i])
            {
                consider(static_cast<int>(i), position[i], XMVectorAdd(position[i], XMVectorScale(XMVectorSubtract(position[i], position[p]), 0.3f)));
            }
        }

        if (bestJoint < 0)
        {
            KR_LOG_INFO("  クリック({:.0f}, {:.0f}): 判定の形にも骨にも当たらず", x, y);
            return;
        }
        m_SelectedBone = bestJoint;
        m_SubHitbox = 0;
        KR_LOG_INFO("  クリック({:.0f}, {:.0f}): 骨 {}「{}」を選びました(線までの距離 {:.3f})", x, y, bestJoint,
                    skeleton.joints[static_cast<size_t>(bestJoint)].name, bestDistance);
    }

    KR::Camera m_Camera; // 今のフレームのカメラ(クリックの光線に使う)

    ClickDetector m_ClickDetector;
    bool m_ClickTracking = false;

    void OnRender() override
    {
        if (!m_Model.IsLoaded())
        {
            return;
        }

        // 形の中心を原点に動かしてから、Y 軸で回す
        const XMMATRIX world = GetModelWorld();

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
        ImGuizmo::BeginFrame(); // ギズモの 1 フレームの始まり

        // Blender に合わせる:T = 移動 / S = 大きさ(スケール)。文字を入力している間は、効かせない
        if (!ImGui::GetIO().WantTextInput)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_T, false))
            {
                m_GizmoOperation = ImGuizmo::TRANSLATE;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_S, false))
            {
                m_GizmoOperation = ImGuizmo::SCALE;
            }
        }
        ImGui::Begin("アニメーション調整ツール###AnimationEditor");
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

        DrawProjectSection();

        if (m_Model.IsLoaded())
        {
            DrawViewSection();
            DrawPlaybackSection();
            DrawHitboxSection();
        }
        else
        {
            ImGui::Separator();
            ImGui::TextWrapped(m_Models.empty() ? "Models の下に .glb が見つかりません。別のフォルダーを選んでください。" : "モデルを読み込めていません(ログを見てください)。");
        }
        ImGui::End();

        DrawGizmo();
        DrawUnsavedPopup();
    }

    // 選んでいる判定の形の中心に、移動のギズモ(ImGuizmo)を出す(A3-1:表示だけ。ドラッグで動かすのは A3-2)
    // ImGuizmo は、行列を OpenGL 形式(列が連続)の float[16] で受け取る。DirectXMath の行列(行が連続・行ベクトル)は、メモリの並びが同じなので、そのまま渡せる
    // ただし、ImGuizmo は右手系を前提にしていて、このエンジンは左手系。軸の反転(AllowAxisFlip)が、視線の向きの判断を誤るので、切る
    void DrawGizmo()
    {
        m_GizmoBusy = false;
        if (!m_Model.IsLoaded() || !m_ShowHitboxes || m_SelectedHitbox == NoHitbox)
        {
            return;
        }
        const std::vector<KR::WorldHitbox> shapes = m_Model.GetWorldHitboxes(GetModelWorld());
        if (m_SelectedHitbox >= shapes.size())
        {
            return;
        }
        const KR::WorldHitbox& shape = shapes[m_SelectedHitbox];
        const XMFLOAT3 center = (shape.shape == KR::HitShape::Box) ? shape.box.center : shape.sphere.center;

        const ImVec2 size = ImGui::GetIO().DisplaySize;
        XMFLOAT4X4 view;
        XMFLOAT4X4 projection;
        XMStoreFloat4x4(&view, m_Camera.GetViewMatrix());
        XMStoreFloat4x4(&projection, m_Camera.GetProjectionMatrix(size.x / size.y));
        // ギズモに渡す行列。移動:位置だけ / 大きさ:軸(箱の向き)× 今の大きさ(世界)+ 位置(球は、軸を世界の軸にして、半径を掛ける)
        const bool isScale = (m_GizmoOperation == ImGuizmo::SCALE);
        const bool isBox = (shape.shape == KR::HitShape::Box);
        const XMFLOAT3 oldScale = isBox ? shape.box.halfSize : XMFLOAT3{ shape.sphere.radius, shape.sphere.radius, shape.sphere.radius };
        XMFLOAT4X4 matrix;
        XMStoreFloat4x4(&matrix, XMMatrixTranslation(center.x, center.y, center.z));
        if (isScale)
        {
            const XMFLOAT3 axes[3] = { isBox ? shape.box.axes[0] : XMFLOAT3{ 1, 0, 0 }, isBox ? shape.box.axes[1] : XMFLOAT3{ 0, 1, 0 }, isBox ? shape.box.axes[2] : XMFLOAT3{ 0, 0, 1 } };
            const float scales[3] = { oldScale.x, oldScale.y, oldScale.z };
            float* rows[3] = { &matrix._11, &matrix._21, &matrix._31 };
            for (int i = 0; i < 3; ++i)
            {
                rows[i][0] = axes[i].x * scales[i];
                rows[i][1] = axes[i].y * scales[i];
                rows[i][2] = axes[i].z * scales[i];
            }
        }

        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList()); // ImGui の窓より後ろ・3D の絵より手前に描く
        ImGuizmo::SetRect(0.0f, 0.0f, size.x, size.y);
        ImGuizmo::AllowAxisFlip(false);
        const bool changed = ImGuizmo::Manipulate(&view._11, &projection._11, m_GizmoOperation, isScale ? ImGuizmo::LOCAL : ImGuizmo::WORLD, &matrix._11);
        m_GizmoBusy = ImGuizmo::IsOver() || ImGuizmo::IsUsing(); // 次のフレームの、カメラ・クリックの判断に使う

        if (changed && isScale)
        {
            // 行の長さ = 新しい大きさ(世界)。今の大きさとの比を、骨の中の大きさに掛ける(骨・モデルの拡大があっても、合う)
            const XMFLOAT3 newScale{ std::sqrt(matrix._11 * matrix._11 + matrix._12 * matrix._12 + matrix._13 * matrix._13),
                                     std::sqrt(matrix._21 * matrix._21 + matrix._22 * matrix._22 + matrix._23 * matrix._23),
                                     std::sqrt(matrix._31 * matrix._31 + matrix._32 * matrix._32 + matrix._33 * matrix._33) };
            KR::Hitbox& hitbox = m_Model.GetHitboxes()[m_SelectedHitbox];
            if (isBox)
            {
                hitbox.halfSize = ScaleHalfSize(hitbox.halfSize, oldScale, newScale);
            }
            else
            {
                hitbox.radius = ScaleRadius(hitbox.radius, oldScale.x, newScale);
            }
            m_Dirty = true;
        }
        else if (changed)
        {
            // ギズモが動かした世界の位置 → 骨の中の座標に直して、center にする(形は、骨の中の座標で持つ)
            std::vector<KR::Hitbox>& hitboxes = m_Model.GetHitboxes();
            KR::Hitbox& hitbox = hitboxes[m_SelectedHitbox];
            const std::vector<XMFLOAT4X4>& pose = m_Model.GetPose();
            if (hitbox.jointIndex < 0 || static_cast<size_t>(hitbox.jointIndex) >= pose.size())
            {
                return;
            }
            const XMMATRIX boneToWorld = XMLoadFloat4x4(&pose[static_cast<size_t>(hitbox.jointIndex)]) * GetModelWorld();
            const XMFLOAT3 delta{ matrix._41 - center.x, matrix._42 - center.y, matrix._43 - center.z };
            const XMFLOAT3 before = hitbox.center;
            hitbox.center = MoveCenterByWorldDelta(hitbox.center, boneToWorld, delta);
            m_Dirty = true;
            KR_LOG_TRACE("  ギズモ: 世界で ({:.3f}, {:.3f}, {:.3f}) 動かした → center ({:.3f}, {:.3f}, {:.3f}) → ({:.3f}, {:.3f}, {:.3f})",
                         delta.x, delta.y, delta.z, before.x, before.y, before.z, hitbox.center.x, hitbox.center.y, hitbox.center.z);
        }
    }

    std::optional<KR::Hitbox> m_Clipboard; // コピーした判定の形(貼り付けるまで、覚えている)
    int m_MirrorTarget = 0; // 左右反転の貼り付け先の骨(対の骨が、名前から決まらないときに、ユーザーが選ぶ)
    ImGuizmo::OPERATION m_GizmoOperation = ImGuizmo::TRANSLATE; // 移動 / 大きさ(ラジオボタン・T / S キー。Blender に合わせた)
    bool m_GizmoBusy = false; // ギズモの上にマウスがある・ドラッグ中(カメラを回さない・クリックで選ばない)

    void OnShutdown() override
    {
        m_BoneRenderer.Destroy(GetRenderer());
        m_Model.Unload(GetRenderer());
    }

private:
    // ---- 開く(フォルダー・モデル)----

    // アセットのフォルダーを決める:KR::SetAssetRoot で、エンジンの読み書きの場所を切り替え、Models/ の下の .glb を、一覧にする
    // Models/ が無いフォルダーは、断る(false)
    bool ApplyFolder(const std::filesystem::path& folder)
    {
        std::error_code error;
        if (!std::filesystem::is_directory(folder / "Models", error))
        {
            KR_LOG_WARN("Editor: {} に、Models フォルダーがありません", ToUtf8(folder));
            m_StatusMessage = "Models フォルダーがありません: " + ToUtf8(folder);
            return false;
        }

        KR::SetAssetRoot(folder);
        m_Folder = folder;
        std::snprintf(m_FolderText, sizeof(m_FolderText), "%s", ToUtf8(folder).c_str());

        // Models/ の下の .glb / .gltf を、全部探す(Models からの相対パス。区切りは /)
        m_Models.clear();
        for (const auto& entry : std::filesystem::recursive_directory_iterator(folder / "Models", std::filesystem::directory_options::skip_permission_denied, error))
        {
            if (!entry.is_regular_file())
            {
                continue;
            }
            std::filesystem::path extension = entry.path().extension();
            std::wstring lower = extension.wstring();
            std::transform(lower.begin(), lower.end(), lower.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
            if (lower == L".glb" || lower == L".gltf")
            {
                m_Models.push_back(ToUtf8(std::filesystem::relative(entry.path(), folder / "Models").generic_u8string()));
            }
        }
        std::sort(m_Models.begin(), m_Models.end());
        m_SelectedModel = 0;

        // 最近使ったフォルダーの先頭に
        std::erase(m_Recent, ToUtf8(folder));
        m_Recent.insert(m_Recent.begin(), ToUtf8(folder));
        if (m_Recent.size() > MaxRecentFolders)
        {
            m_Recent.resize(MaxRecentFolders);
        }

        m_StatusMessage = m_Models.empty() ? "Models の下に .glb が見つかりません" : "";
        KR_LOG_INFO("Editor: アセットのフォルダー {}(モデル {} 個)", ToUtf8(folder), m_Models.size());
        return true;
    }

    // モデルを読む(Models からの相対パス)。読めたら、カメラ・選択・「未保存」を、初期化する
    bool ApplyModel(const std::filesystem::path& relative)
    {
        m_Dirty = false;
        m_CurrentModel.clear();
        if (!m_Model.Load(GetRenderer(), relative))
        {
            m_StatusMessage = "読み込めません: " + ToUtf8(relative);
            return false;
        }
        m_CurrentModel = ToUtf8(relative.generic_u8string());
        m_StatusMessage = "";

        // 一覧の中で、今のモデルを選んでおく
        for (size_t i = 0; i < m_Models.size(); ++i)
        {
            if (m_Models[i] == m_CurrentModel)
            {
                m_SelectedModel = i;
            }
        }

        const KR::AnimatedModel::Bounds& bounds = m_Model.GetBounds();
        m_Center = bounds.Center();
        m_ModelRadius = bounds.Radius();
        m_TestBox.halfSize = { m_ModelRadius * 0.15f, m_ModelRadius * 0.15f, m_ModelRadius * 0.15f };
        m_TestCenter = { m_ModelRadius * 0.8f, 0.0f, 0.0f }; // 最初は、モデルの横に置く(スライダーで、動かして、骨の形に、ぶつける)
        m_SelectedBone = 0;
        m_SubHitbox = 0;
        m_SelectedHitbox = NoHitbox;
        m_BindPose = false;
        m_LastClip = 0;
        ResetCamera();
        SaveSettings();
        return true;
    }

    // 未保存の変更があれば、確認の窓を出してから(DrawUnsavedPopup)。無ければ、すぐ
    void RequestFolder(const std::filesystem::path& folder)
    {
        if (m_Dirty)
        {
            m_Pending = Pending::Folder;
            m_PendingPath = folder;
            return;
        }
        if (ApplyFolder(folder))
        {
            OpenFirstModelOrUnload();
        }
    }

    void RequestModel(const std::filesystem::path& relative)
    {
        if (m_Dirty)
        {
            m_Pending = Pending::Model;
            m_PendingPath = relative;
            return;
        }
        ApplyModel(relative);
    }

    // フォルダーを替えたあと:新しいフォルダーの、最初のモデルを開く(無ければ、今のモデルを閉じる)
    void OpenFirstModelOrUnload()
    {
        if (!m_Models.empty())
        {
            ApplyModel(FromUtf8(m_Models.front()));
        }
        else
        {
            m_Model.Unload(GetRenderer());
            m_CurrentModel.clear();
            m_Dirty = false;
        }
        SaveSettings();
    }

    void ApplyPending()
    {
        const Pending kind = m_Pending;
        const std::filesystem::path path = m_PendingPath;
        m_Pending = Pending::None;
        if (kind == Pending::Folder)
        {
            if (ApplyFolder(path))
            {
                OpenFirstModelOrUnload();
            }
        }
        else if (kind == Pending::Model)
        {
            ApplyModel(path);
        }
    }

    // ---- 設定の記憶(exe の隣の KRAnimationEditor.txt。1 行 1 項目。UTF-8)----

    void LoadSettings()
    {
        std::ifstream stream(GetExeDirectory() / "KRAnimationEditor.txt", std::ios::binary);
        std::string line;
        while (std::getline(stream, line))
        {
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }
            if (line.rfind("assets=", 0) == 0)      { m_LastFolder = FromUtf8(line.substr(7)); }
            else if (line.rfind("model=", 0) == 0)  { m_LastModel = FromUtf8(line.substr(6)); }
            else if (line.rfind("recent=", 0) == 0) { m_Recent.push_back(line.substr(7)); }
        }
    }

    void SaveSettings() const
    {
        std::ofstream stream(GetExeDirectory() / "KRAnimationEditor.txt", std::ios::binary);
        if (!stream.is_open())
        {
            return; // 記憶できなくても、ツールは、使える
        }
        stream << "assets=" << ToUtf8(m_Folder) << '\n';
        stream << "model=" << m_CurrentModel << '\n';
        for (const std::string& recent : m_Recent)
        {
            stream << "recent=" << recent << '\n';
        }
    }

    // ---- カメラ(マウス)----

    void ResetCamera()
    {
        m_Yaw = 0.0f;
        m_Pitch = 0.15f;
        m_Distance = std::max(m_ModelRadius, 0.1f) * 3.0f;
        m_Target = { 0.0f, 0.0f, 0.0f };
    }

    void UpdateCamera()
    {
        // ImGui の窓の上・ImGui を操作している間は、カメラを動かさない
        const ImGuiIO& io = ImGui::GetIO();
        if (!io.WantCaptureMouse && !m_GizmoBusy)
        {
            if (io.MouseDown[0]) // 左ドラッグ:回す
            {
                m_Yaw -= io.MouseDelta.x * 0.01f;
                m_Pitch = std::clamp(m_Pitch + io.MouseDelta.y * 0.01f, -1.5f, 1.5f);
            }
            if (io.MouseDown[2]) // 中ドラッグ:平行移動(カメラの右・上の向きに、距離に比例した量)
            {
                const float scale = m_Distance * 0.002f;
                m_Target.x -= std::cos(m_Yaw) * io.MouseDelta.x * scale;
                m_Target.z -= std::sin(m_Yaw) * io.MouseDelta.x * scale;
                m_Target.y += io.MouseDelta.y * scale;
            }
            if (io.MouseWheel != 0.0f) // ホイール:近づく・離れる
            {
                m_Distance = std::clamp(m_Distance * std::pow(0.9f, io.MouseWheel), m_ModelRadius * 0.2f + 0.01f, m_ModelRadius * 20.0f + 1.0f);
            }
        }

        // 注目点のまわりを、回る位置にカメラを置く(yaw = 0 で、手前(−Z)から見る)
        m_Camera.SetPosition({ m_Target.x + m_Distance * std::sin(m_Yaw) * std::cos(m_Pitch),
                               m_Target.y + m_Distance * std::sin(m_Pitch),
                               m_Target.z - m_Distance * std::cos(m_Yaw) * std::cos(m_Pitch) });
        m_Camera.LookAt(m_Target);
        GetRenderer().SetCamera(m_Camera);
    }

    // ---- ImGui の各区画 ----

    void DrawProjectSection()
    {
        ImGui::SeparatorText("プロジェクト");

        // アセットのフォルダー:文字欄(Enter で開く)・最近(一覧を出す)・参照(フォルダーを選ぶ窓)
        ImGui::SetNextItemWidth(-190.0f);
        const bool entered = ImGui::InputText("##folder", m_FolderText, sizeof(m_FolderText), ImGuiInputTextFlags_EnterReturnsTrue);
        if (entered)
        {
            RequestFolder(FromUtf8(m_FolderText));
        }
        ImGui::SameLine();
        if (ImGui::Button("最近..."))
        {
            ImGui::OpenPopup("recent_popup");
        }
        if (ImGui::BeginPopup("recent_popup"))
        {
            if (m_Recent.empty())
            {
                ImGui::TextDisabled("履歴はありません");
            }
            for (size_t i = 0; i < m_Recent.size(); ++i)
            {
                const std::string label = m_Recent[i] + "##recent" + std::to_string(i);
                if (ImGui::Selectable(label.c_str()))
                {
                    RequestFolder(FromUtf8(m_Recent[i]));
                }
            }
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("参照..."))
        {
            if (const std::optional<std::filesystem::path> picked = PickFolder(m_Folder))
            {
                RequestFolder(*picked);
            }
        }
        ImGui::SameLine();
        ImGui::Text("アセット");

        // モデルの一覧(Models の下の .glb)
        if (!m_Models.empty())
        {
            m_SelectedModel = std::min(m_SelectedModel, m_Models.size() - 1);
            const std::string preview = m_CurrentModel.empty() ? "選んでください..." : m_CurrentModel;
            if (ImGui::BeginCombo("モデル", preview.c_str()))
            {
                for (size_t i = 0; i < m_Models.size(); ++i)
                {
                    const std::string label = m_Models[i] + "##model" + std::to_string(i);
                    if (ImGui::Selectable(label.c_str(), m_Models[i] == m_CurrentModel))
                    {
                        RequestModel(FromUtf8(m_Models[i]));
                    }
                }
                ImGui::EndCombo();
            }
        }
        if (!m_StatusMessage.empty())
        {
            ImGui::TextColored({ 1.0f, 0.5f, 0.3f, 1.0f }, "%s", m_StatusMessage.c_str());
        }
        if (m_Dirty)
        {
            ImGui::TextColored({ 1.0f, 0.8f, 0.2f, 1.0f }, "* 判定の形が、保存されていません");
        }
    }

    void DrawViewSection()
    {
        ImGui::SeparatorText("表示");
        ImGui::Text("骨: %zu 本 / 動き: %zu 個", m_Model.GetSkeleton().joints.size(), m_Model.GetClipCount());
        ImGui::Checkbox("骨", &m_ShowBones);
        ImGui::Checkbox("骨を手前に表示", &m_BonesInFront); // 骨を、メッシュの手前に描く(メッシュの中の骨も見える)
        ImGui::Checkbox("メッシュ", &m_ShowMesh);
        ImGui::Checkbox("モデルを回す", &m_Rotate);
        if (ImGui::Button("カメラをリセット"))
        {
            ResetCamera();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(左ドラッグ:回す / 中ドラッグ:移動 / ホイール:ズーム)");

        // 選んだ骨:一覧(コンボボックス)から、名前で選ぶ。選んだ骨は橙色になり、その骨の判定の形(下)が、調整できる
        // 一覧は、骨の階層に合わせて、字下げする(子の骨ほど、右へ)
        const KR::Skeleton& skeleton = m_Model.GetSkeleton();
        const int jointCount = static_cast<int>(skeleton.joints.size());
        m_SelectedBone = std::clamp(m_SelectedBone, 0, std::max(jointCount - 1, 0));
        if (ImGui::BeginCombo("骨の選択", skeleton.joints[static_cast<size_t>(m_SelectedBone)].name.c_str()))
        {
            for (int i = 0; i < jointCount; ++i)
            {
                int depth = 0; // 根から、何段目か(親をたどって数える)
                for (int p = skeleton.joints[static_cast<size_t>(i)].parent; p >= 0 && depth < jointCount; p = skeleton.joints[static_cast<size_t>(p)].parent)
                {
                    ++depth;
                }
                const std::string label = std::string(static_cast<size_t>(depth) * 2, ' ') + skeleton.joints[static_cast<size_t>(i)].name + "##bone" + std::to_string(i);
                if (ImGui::Selectable(label.c_str(), i == m_SelectedBone))
                {
                    m_SelectedBone = i;
                    m_SubHitbox = 0;
                }
            }
            ImGui::EndCombo();
        }
    }

    void DrawPlaybackSection()
    {
        ImGui::SeparatorText("再生");
        if (m_Model.GetClipCount() == 0)
        {
            ImGui::Text("動き(アニメーション)がありません");
            return;
        }

        // 「基準姿勢」がオンの間は、動きを止めて、基準姿勢にしている。オフにすると、選んでいる動きを、最初から再生する
        if (m_Model.GetCurrentClip() >= 0)
        {
            m_LastClip = static_cast<size_t>(m_Model.GetCurrentClip());
        }
        m_LastClip = std::min(m_LastClip, m_Model.GetClipCount() - 1);

        // 動きの一覧(名前で選ぶ)
        if (ImGui::BeginCombo("動き", m_Model.GetClipName(m_LastClip).c_str()))
        {
            for (size_t i = 0; i < m_Model.GetClipCount(); ++i)
            {
                const std::string label = m_Model.GetClipName(i) + "##clip" + std::to_string(i);
                if (ImGui::Selectable(label.c_str(), i == m_LastClip))
                {
                    m_LastClip = i;
                    if (!m_BindPose)
                    {
                        m_Model.Play(m_LastClip, m_Model.IsLooping());
                    }
                }
            }
            ImGui::EndCombo();
        }
        ImGui::Text("長さ: %.3f 秒", m_Model.GetDuration());

        bool paused = m_Model.IsPaused();
        if (ImGui::Checkbox("一時停止", &paused))
        {
            m_Model.Pause(paused);
        }
        bool loop = m_Model.IsLooping();
        if (ImGui::Checkbox("ループ", &loop))
        {
            m_Model.SetLooping(loop);
            if (loop && !m_BindPose)
            {
                m_Model.Play(m_LastClip, true); // ループに戻したら、最初から
            }
        }
        ImGui::SameLine();
        ImGui::Text(m_Model.IsFinished() ? "(再生終了)" : "");
        float speed = m_Model.GetSpeed();
        if (ImGui::SliderFloat("速さ", &speed, 0.0f, 3.0f))
        {
            m_Model.SetSpeed(speed);
        }
        float time = m_Model.GetTime();
        if (ImGui::SliderFloat("時刻", &time, 0.0f, m_Model.GetDuration())) // 止めているときに、時刻を動かして、姿勢を確かめる
        {
            m_Model.SetTime(time);
        }

        // 基準姿勢(動かない姿勢)のオン・オフ。オンで、動きを止める。オフで、選んでいる動きを、最初から再生する
        if (ImGui::Checkbox("基準姿勢(アニメーションなし)", &m_BindPose))
        {
            if (m_BindPose)
            {
                m_Model.Stop();
            }
            else
            {
                m_Model.Play(m_LastClip, m_Model.IsLooping());
            }
        }
    }

    void DrawHitboxSection()
    {
        ImGui::SeparatorText("当たり判定");
        const KR::Skeleton& skeleton = m_Model.GetSkeleton();
        const int jointCount = static_cast<int>(skeleton.joints.size());

        ImGui::Checkbox("判定の形を表示", &m_ShowHitboxes);
        const float range = m_ModelRadius * 1.5f;
        ImGui::SliderFloat3("テストの箱の位置", &m_TestCenter.x, -range, range);
        std::vector<KR::Hitbox>& hitboxes = m_Model.GetHitboxes();
        ImGui::Text("当たり: %zu / %zu", m_HitIndices.size(), hitboxes.size());
        for (const size_t index : m_HitIndices)
        {
            // 触れている形が、どの骨の物か(名前で表示)
            const int joint = hitboxes[index].jointIndex;
            ImGui::BulletText("%s", joint >= 0 && joint < jointCount ? skeleton.joints[static_cast<size_t>(joint)].name.c_str() : "(骨なし)");
        }

        // 選んだ骨(表示の「骨の選択」)にくっついた、判定の形を調整する。形は、骨の中の座標で持つ(骨が動いても、この数字は変わらない)
        // 1 本の骨に、複数の形も付けられる(その場合だけ、「この骨の何番目の形」で選ぶ)
        std::vector<size_t> onBone;
        for (size_t i = 0; i < hitboxes.size(); ++i)
        {
            if (hitboxes[i].jointIndex == m_SelectedBone)
            {
                onBone.push_back(i);
            }
        }

        m_SelectedHitbox = NoHitbox;
        if (onBone.empty())
        {
            ImGui::Text("この骨には、判定の形がありません");
        }
        else
        {
            m_SubHitbox = std::clamp(m_SubHitbox, 0, static_cast<int>(onBone.size()) - 1);
            if (onBone.size() > 1)
            {
                ImGui::SliderInt("この骨の何番目の形", &m_SubHitbox, 0, static_cast<int>(onBone.size()) - 1);
            }
            m_SelectedHitbox = onBone[static_cast<size_t>(m_SubHitbox)];
            KR::Hitbox& hitbox = hitboxes[m_SelectedHitbox];

            // ギズモの操作:移動 / 大きさ
            int operation = (m_GizmoOperation == ImGuizmo::SCALE) ? 1 : 0;
            ImGui::RadioButton("移動 (T)", &operation, 0);
            ImGui::SameLine();
            ImGui::RadioButton("大きさ (S)", &operation, 1);
            m_GizmoOperation = (operation == 1) ? ImGuizmo::SCALE : ImGuizmo::TRANSLATE;

            // 変えたら「未保存」の印(m_Dirty)
            int shape = (hitbox.shape == KR::HitShape::Box) ? 0 : 1;
            if (ImGui::Combo("形の種類", &shape, "箱\0球\0"))
            {
                m_Dirty = true;
            }
            hitbox.shape = (shape == 0) ? KR::HitShape::Box : KR::HitShape::Sphere;
            m_Dirty |= ImGui::DragFloat3("中心(骨の中)", &hitbox.center.x, 0.05f);
            if (hitbox.shape == KR::HitShape::Box)
            {
                m_Dirty |= ImGui::DragFloat3("半分の大きさ", &hitbox.halfSize.x, 0.05f, 0.01f, 100.0f);
            }
            else
            {
                m_Dirty |= ImGui::DragFloat("半径", &hitbox.radius, 0.05f, 0.01f, 100.0f);
            }
        }

        // 形の追加・削除(追加・削除のあとは、形の一覧が変わるので、最後にまとめて行う)
        if (ImGui::Button("この骨に判定の形を追加"))
        {
            KR::Hitbox added;
            added.name = skeleton.joints[static_cast<size_t>(m_SelectedBone)].name;
            added.jointIndex = m_SelectedBone;
            const float size = m_ModelRadius * 0.1f;
            added.halfSize = { size, size, size };
            added.radius = size;
            hitboxes.push_back(added);
            m_SubHitbox = static_cast<int>(onBone.size()); // 追加した形を、選ぶ
            m_Dirty = true;
        }
        if (m_SelectedHitbox != NoHitbox)
        {
            ImGui::SameLine();
            if (ImGui::Button("この形を削除"))
            {
                hitboxes.erase(hitboxes.begin() + static_cast<std::ptrdiff_t>(m_SelectedHitbox));
                m_SelectedHitbox = NoHitbox;
                m_SubHitbox = 0;
                m_Dirty = true;
            }
        }

        // コピー・貼り付け(Ctrl + C / Ctrl + V でも)。貼り付けは、選んでいる骨に、形を 1 つ増やす
        const bool keys = !ImGui::GetIO().WantTextInput && ImGui::GetIO().KeyCtrl;
        const bool copyRequested = ImGui::Button("コピー") || (keys && ImGui::IsKeyPressed(ImGuiKey_C, false));
        ImGui::SameLine();
        const bool pasteRequested = ImGui::Button("貼り付け") || (keys && ImGui::IsKeyPressed(ImGuiKey_V, false));
        if (m_Clipboard)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("(コピー済み:%s)", m_Clipboard->name.c_str());
        }
        if (copyRequested && m_SelectedHitbox != NoHitbox)
        {
            m_Clipboard = hitboxes[m_SelectedHitbox];
            KR_LOG_INFO("  判定の形をコピー: {}(骨 {})", m_Clipboard->name, m_Clipboard->jointIndex);
        }
        if (pasteRequested && m_Clipboard)
        {
            const size_t before = hitboxes.size();
            const bool sameBone = (m_Clipboard->jointIndex == m_SelectedBone);
            hitboxes.push_back(MakePastedHitbox(*m_Clipboard, m_SelectedBone, skeleton.joints[static_cast<size_t>(m_SelectedBone)].name,
                                                sameBone ? m_ModelRadius * 0.05f : 0.0f));
            m_SubHitbox = static_cast<int>(onBone.size()); // 貼り付けた形を、選ぶ
            m_Dirty = true;
            KR_LOG_INFO("  判定の形を貼り付け: 骨 {}「{}」へ({}・形 {} → {} 個)", m_SelectedBone, hitboxes.back().name,
                        sameBone ? "同じ骨なので、ずらした" : "別の骨", before, hitboxes.size());
        }

        // 左右反転して貼り付け:コピーした形を、X = 0 の鏡映しにして、対の骨に貼る(対の骨は、名前で自動。決まらなければ、一覧から選ぶ)
        if (m_Clipboard)
        {
            const int jointTotal = static_cast<int>(skeleton.joints.size());
            const int sourceJoint = m_Clipboard->jointIndex;
            if (sourceJoint < 0 || sourceJoint >= jointTotal)
            {
                ImGui::TextDisabled("コピーした形は、骨に付いていないので、反転できません");
            }
            else
            {
                const std::optional<int> autoJoint = KR::FindMirrorJoint(skeleton, sourceJoint);
                if (autoJoint)
                {
                    m_MirrorTarget = *autoJoint;
                    ImGui::Text("反転先の骨:%s(名前から自動)", skeleton.joints[static_cast<size_t>(m_MirrorTarget)].name.c_str());
                }
                else
                {
                    m_MirrorTarget = std::clamp(m_MirrorTarget, 0, jointTotal - 1);
                    ImGui::TextDisabled("対の骨が、名前から決まりません。反転先の骨を選んでください");
                    if (ImGui::BeginCombo("反転先の骨", skeleton.joints[static_cast<size_t>(m_MirrorTarget)].name.c_str()))
                    {
                        for (int i = 0; i < jointTotal; ++i)
                        {
                            const std::string label = skeleton.joints[static_cast<size_t>(i)].name + "##mirror" + std::to_string(i);
                            if (ImGui::Selectable(label.c_str(), i == m_MirrorTarget))
                            {
                                m_MirrorTarget = i;
                            }
                        }
                        ImGui::EndCombo();
                    }
                }

                if (ImGui::Button("左右反転して貼り付け"))
                {
                    // 骨の行列は、2 つとも基準姿勢のもの(動きを再生していても、反転の結果は、変わらない)
                    const std::vector<XMFLOAT4X4> basePose = KR::EvaluatePose(skeleton, nullptr, 0.0f);
                    const size_t before = hitboxes.size();
                    KR::Hitbox mirrored = KR::MirrorHitbox(*m_Clipboard, basePose[static_cast<size_t>(sourceJoint)], m_MirrorTarget, basePose[static_cast<size_t>(m_MirrorTarget)]);
                    mirrored.name = skeleton.joints[static_cast<size_t>(m_MirrorTarget)].name;

                    int onTarget = 0; // 反転先の骨に、すでにある形の数(= 貼った形の「何番目」)
                    for (const KR::Hitbox& hitbox : hitboxes)
                    {
                        onTarget += (hitbox.jointIndex == m_MirrorTarget) ? 1 : 0;
                    }
                    hitboxes.push_back(mirrored);
                    m_SelectedBone = m_MirrorTarget; // 貼った形を、選ぶ
                    m_SubHitbox = onTarget;
                    m_Dirty = true;
                    KR_LOG_INFO("  判定の形を左右反転して貼り付け: 骨 {} → 骨 {}「{}」(形 {} → {} 個)", sourceJoint, m_MirrorTarget, mirrored.name, before, hitboxes.size());
                }
            }
        }

        // 保存・読み直し(<モデル名>.hitboxes.json。開いているアセットのフォルダーの、Models の下に書く)
        ImGui::Separator();
        if (ImGui::Button(m_Dirty ? "判定を保存 *" : "判定を保存"))
        {
            if (m_Model.SaveHitboxes())
            {
                m_Dirty = false;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("判定を読み直す"))
        {
            m_Model.ReloadHitboxes();
            m_Dirty = false;
        }
    }

    // 未保存のまま、別のモデル・フォルダーに切り替えようとしたときの、確認の窓
    void DrawUnsavedPopup()
    {
        constexpr const char* PopupName = "未保存の変更";
        if (m_Pending != Pending::None && !ImGui::IsPopupOpen(PopupName))
        {
            ImGui::OpenPopup(PopupName);
        }
        if (ImGui::BeginPopupModal(PopupName, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("いまのモデルの判定の形に、保存していない変更があります。");
            if (ImGui::Button("保存して続ける"))
            {
                if (m_Model.SaveHitboxes())
                {
                    m_Dirty = false;
                    ApplyPending();
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("捨てて続ける"))
            {
                m_Dirty = false;
                ApplyPending();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("やめる"))
            {
                m_Pending = Pending::None;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

private:
    KR::AnimatedModel m_Model;       // 開いているモデル(形・骨・動き・判定の形)
    KR::BoneRenderer m_BoneRenderer; // 骨を、八面体の形(Blender の骨に似せた形)で描く(デバッグ用)

    // 開いているもの
    std::filesystem::path m_Folder;        // アセットのフォルダー(KR::SetAssetRoot に渡した物)
    std::vector<std::string> m_Models;     // Models の下の .glb(相対パス。UTF-8。区切りは /)
    size_t m_SelectedModel = 0;
    std::string m_CurrentModel;            // 今読んでいるモデル(Models からの相対パス)
    char m_FolderText[1024] = {};          // フォルダーの文字欄
    std::vector<std::string> m_Recent;     // 最近使ったフォルダー(新しい順)
    std::filesystem::path m_LastFolder;    // 前回のフォルダー・モデル(記憶から読んだ物。起動のときだけ使う)
    std::filesystem::path m_LastModel;
    std::string m_StatusMessage;           // 画面に出す、エラーなどの文字

    // 未保存の確認
    bool m_Dirty = false;                  // 判定の形を変えて、まだ保存していない
    enum class Pending { None, Folder, Model };
    Pending m_Pending = Pending::None;     // 未保存のとき、確認のあとに行う、切り替え
    std::filesystem::path m_PendingPath;

    // カメラ(マウスで動かす)
    float m_Yaw = 0.0f;
    float m_Pitch = 0.15f;
    float m_Distance = 10.0f;
    XMFLOAT3 m_Target = {};

    XMFLOAT3 m_Center = {};          // 形の中心
    float m_ModelRadius = 1.0f;      // モデルの大きさ(スライダーの範囲に使う)
    float m_Angle = 0.0f;
    int m_SelectedBone = 0;          // 選んだ骨(関節の番号)。色が変わる。判定の形の調整は、この骨の形について行う
    bool m_ShowBones = true;
    bool m_BonesInFront = true;
    bool m_ShowMesh = true;
    bool m_Rotate = false;

    // 判定の形
    static constexpr size_t NoHitbox = static_cast<size_t>(-1); // 調整する形が無い(選んだ骨に、形が無い)
    size_t m_SelectedHitbox = NoHitbox;    // 調整している形(選んだ骨の形。毎フレーム、ImGui が決める)
    int m_SubHitbox = 0;                   // 選んだ骨に、形が複数あるときの、何番目か
    bool m_BindPose = false;               // 動きを止めて、基準姿勢にしているか
    size_t m_LastClip = 0;                 // 基準姿勢をオフにしたときに、再生する動き
    bool m_ShowHitboxes = true;
    XMFLOAT3 m_TestCenter = {};            // テストの箱の位置(世界)
    KR::OBB m_TestBox;                     // テストの箱(軸にそろえた箱)
    std::vector<size_t> m_HitIndices;      // 今、テストの箱に触れている形の番号
};

std::unique_ptr<KR::Application> KR::CreateApplication()
{
    return std::make_unique<EditorApp>();
}
