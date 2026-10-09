#pragma once

#include "KR/Animation/GltfLoader.h"

#include <filesystem>

namespace KR
{
    // FBX(.fbx)を読む。relativePath は Assets/Models からの相対パス(例: "Test/Rigged.fbx")
    // 座標は、このエンジン(左手系・Y が上・メートル)に直して返す。失敗したらログにエラーを出して、空のモデルを返す(落ちない)
    // 中で使うライブラリ(ufbx)の型は、この公開ヘッダーに出さない(GltfLoader と同じ作法)
    //
    // 今は、メッシュだけ(F3):三角形・法線・UV。骨(Skeleton)・重み(skin)・動き(clips)は、空(F4〜F6 で読む)
    GltfModel LoadFbx(const std::filesystem::path& relativePath);
}
