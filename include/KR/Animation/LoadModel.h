#pragma once

#include "KR/Animation/GltfLoader.h"

#include <filesystem>

namespace KR
{
    // モデルのファイルを、拡張子で振り分けて、読む。AnimatedModel::Load が使う(ゲームは、ファイルの形式を、気にしなくてよい)
    //   .glb / .gltf → LoadGltf
    //   .fbx         → LoadFbx
    // 拡張子の大文字・小文字は、区別しない。relativePath は Assets/Models からの相対パス。
    // 知らない拡張子・読めないファイルは、ログにエラーを出して、空の ModelData を返す(落ちない)
    ModelData LoadModel(const std::filesystem::path& relativePath);
}
