#include "KR/Animation/HitboxMirror.h"

#include <cmath>

using namespace DirectX;

namespace KR
{
    namespace
    {
        // 左右の言葉の組。最初の物と 2 番目の物を、入れ替える(大文字・小文字・先頭だけ大文字、の 3 通り)
        struct WordPair
        {
            const char* a;
            const char* b;
        };
        constexpr WordPair WordPairs[] = {
            { "Left", "Right" }, { "left", "right" }, { "LEFT", "RIGHT" }, { "左", "右" },
        };

        // text の中の word を探して、replacement に置き換える(最初の 1 か所)。見つからなければ false
        bool ReplaceFirst(std::string& text, const std::string& word, const std::string& replacement)
        {
            const size_t position = text.find(word);
            if (position == std::string::npos)
            {
                return false;
            }
            text.replace(position, word.size(), replacement);
            return true;
        }

        // 終わりの「.001」のような連番(Blender が、名前の重なりを避けるために付ける)の、始まりの位置。無ければ name.size()
        size_t NumberSuffixStart(const std::string& name)
        {
            size_t end = name.size();
            while (end > 0 && name[end - 1] >= '0' && name[end - 1] <= '9')
            {
                --end;
            }
            if (end < name.size() && end > 0 && name[end - 1] == '.')
            {
                return end - 1;
            }
            return name.size();
        }
    }

    std::optional<std::string> MirrorBoneName(const std::string& name)
    {
        // ① 終わりの .L / .R / _L / _R(連番の前)
        const size_t bodyEnd = NumberSuffixStart(name);
        if (bodyEnd >= 2 && (name[bodyEnd - 2] == '.' || name[bodyEnd - 2] == '_'))
        {
            const char side = name[bodyEnd - 1];
            const char swapped = side == 'L' ? 'R' : side == 'R' ? 'L' : side == 'l' ? 'r' : side == 'r' ? 'l' : '\0';
            if (swapped != '\0')
            {
                std::string result = name;
                result[bodyEnd - 1] = swapped;
                return result;
            }
        }

        // ② Left / Right・③ 左 / 右(どちらの向きでも、入れ替える)
        for (const WordPair& pair : WordPairs)
        {
            std::string result = name;
            if (ReplaceFirst(result, pair.a, pair.b) || ReplaceFirst(result, pair.b, pair.a))
            {
                return result;
            }
        }
        return std::nullopt;
    }

    std::optional<int> FindMirrorJoint(const Skeleton& skeleton, int joint)
    {
        if (joint < 0 || static_cast<size_t>(joint) >= skeleton.joints.size())
        {
            return std::nullopt;
        }
        const std::optional<std::string> name = MirrorBoneName(skeleton.joints[static_cast<size_t>(joint)].name);
        if (!name)
        {
            return std::nullopt;
        }
        for (size_t i = 0; i < skeleton.joints.size(); ++i)
        {
            if (skeleton.joints[i].name == *name)
            {
                return static_cast<int>(i);
            }
        }
        return std::nullopt;
    }

    Hitbox MirrorHitbox(const Hitbox& source, const XMFLOAT4X4& sourceBoneWorld, int targetJoint, const XMFLOAT4X4& targetBoneWorld)
    {
        // ① 形 → 元の骨の中の行列 → モデルの座標(行ベクトル:向き → 位置 → 骨 の順に掛ける)
        const XMMATRIX local = XMMatrixRotationQuaternion(XMQuaternionNormalize(XMLoadFloat4(&source.rotation)))
                             * XMMatrixTranslation(source.center.x, source.center.y, source.center.z);
        const XMMATRIX inModel = local * XMLoadFloat4x4(&sourceBoneWorld);

        // ② X を反転(行列の右から、X を −1 倍する)→ ③ 対の骨の中の座標に戻す
        const XMMATRIX mirrored = inModel * XMMatrixScaling(-1.0f, 1.0f, 1.0f);
        XMMATRIX inTarget = mirrored * XMMatrixInverse(nullptr, XMLoadFloat4x4(&targetBoneWorld));

        Hitbox result = source;
        result.jointIndex = targetJoint;
        XMStoreFloat3(&result.center, inTarget.r[3]);

        if (source.shape == HitShape::Box)
        {
            // 1〜3 行目 = 箱の軸(長さは、骨の拡大の差。1 なら、そのまま)
            float length[3];
            for (int k = 0; k < 3; ++k)
            {
                length[k] = XMVectorGetX(XMVector3Length(inTarget.r[k]));
                if (length[k] > 1e-8f)
                {
                    inTarget.r[k] = XMVectorScale(inTarget.r[k], 1.0f / length[k]);
                }
                inTarget.r[k] = XMVectorSetW(inTarget.r[k], 0.0f);
            }
            inTarget.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

            // 反転で、座標の右手・左手が入れ替わった(3 本の軸の行列式が、負)→ X の軸の向きを逆にして、普通の回転にする
            if (XMVectorGetX(XMMatrixDeterminant(inTarget)) < 0.0f)
            {
                inTarget.r[0] = XMVectorNegate(inTarget.r[0]);
                inTarget.r[0] = XMVectorSetW(inTarget.r[0], 0.0f);
            }
            XMStoreFloat4(&result.rotation, XMQuaternionNormalize(XMQuaternionRotationMatrix(inTarget)));
            result.halfSize = { source.halfSize.x * length[0], source.halfSize.y * length[1], source.halfSize.z * length[2] };
        }
        return result;
    }
}
