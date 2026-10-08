#pragma once

#include "KR/Animation/AnimationClip.h"
#include "KR/Animation/Skeleton.h"
#include "KR/Renderer/Mesh.h"

#include <filesystem>
#include <vector>

namespace KR
{
    // glTF 1ファイルから読んだ、形・骨・動き(CPU 側)
    struct GltfModel
    {
        MeshData mesh;
        Skeleton skeleton; // 骨が無いモデルは空
        std::vector<AnimationClip> clips;

        bool IsEmpty() const { return mesh.vertices.empty(); }
    };

    // glTF(.glb / .gltf)を読む。relativePath は Assets/Models からの相対パス(例: "Test/Rigged.glb")
    // 座標は、このエンジン(左手系)に直して返す。失敗したらログにエラーを出して、空の GltfModel を返す
    // メモ: 今は、骨の番号と重みは、頂点に入れない(GPU スキニングの段階で足す)。メッシュのノードの変換も、使わない
    GltfModel LoadGltf(const std::filesystem::path& relativePath);
}
