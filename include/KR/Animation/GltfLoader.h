#pragma once

#include "KR/Animation/AnimationClip.h"
#include "KR/Animation/Skeleton.h"
#include "KR/Renderer/Mesh.h"

#include <filesystem>
#include <vector>

namespace KR
{
    // モデル 1 ファイル(glTF でも FBX でも)から読んだ、形・骨・動き(CPU 側)。LoadGltf / LoadFbx が返す(もとの名前は GltfModel。FBX にも使うので、改名した)
    struct ModelData
    {
        MeshData mesh;
        Skeleton skeleton; // 骨が無いモデルは空
        std::vector<AnimationClip> clips;

        bool IsEmpty() const { return mesh.vertices.empty(); }
    };

    // glTF(.glb / .gltf)を読む。relativePath は Assets/Models からの相対パス(例: "Test/Rigged.glb")
    // 座標は、このエンジン(左手系)に直して返す。失敗したらログにエラーを出して、空の ModelData を返す
    // メモ: 骨の番号と重み(1 組目)は、mesh.skin に入る(骨つきなら)。メッシュのノードの変換は、使わない
    ModelData LoadGltf(const std::filesystem::path& relativePath);
}
