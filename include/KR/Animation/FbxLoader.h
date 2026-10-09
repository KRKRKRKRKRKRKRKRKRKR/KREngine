#pragma once

#include "KR/Animation/GltfLoader.h"

#include <filesystem>

namespace KR
{
    // FBX(.fbx)を読む。relativePath は Assets/Models からの相対パス(例: "Test/Rigged.fbx")
    // 座標は、このエンジン(左手系・Y が上・メートル)に直して返す。失敗したらログにエラーを出して、空のモデルを返す(落ちない)
    // 中で使うライブラリ(ufbx)の型は、この公開ヘッダーに出さない(GltfLoader と同じ作法)
    //
    // 今は、メッシュ(三角形・法線・UV)と、骨(名前・親・基準姿勢・逆バインド)。重み(skin)・動き(clips)は、空(F5・F6 で読む)
    // 座標:モデルの座標 = FBX の世界の座標(Blender の置き場所を、そのまま持つ)。骨の基準姿勢は、バインド時の世界の行列から作る
    GltfModel LoadFbx(const std::filesystem::path& relativePath);
}
