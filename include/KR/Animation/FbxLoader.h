#pragma once

#include "KR/Animation/GltfLoader.h"

#include <filesystem>

namespace KR
{
    // FBX(.fbx)を読む。relativePath は Assets/Models からの相対パス(例: "Test/Rigged.fbx")
    // 座標は、このエンジン(左手系・Y が上・メートル)に直して返す。失敗したらログにエラーを出して、空のモデルを返す(落ちない)
    // 中で使うライブラリ(ufbx)の型は、この公開ヘッダーに出さない(GltfLoader と同じ作法)
    //
    // 今は、入口だけ(F2):ファイルを開いて、ufbx で解析できるかを確かめ、中身(ノード・メッシュ・動きの数)をログに出して、空のモデルを返す。
    // メッシュ・骨・動きを読むのは、これから(F3〜F6)
    GltfModel LoadFbx(const std::filesystem::path& relativePath);
}
