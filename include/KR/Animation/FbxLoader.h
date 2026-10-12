#pragma once

#include "KR/Animation/GltfLoader.h"

#include <filesystem>

namespace KR
{
    // FBX(.fbx)を読む。relativePath は Assets/Models からの相対パス(例: "Test/Rigged.fbx")
    // 座標は、このエンジン(左手系・Y が上・メートル)に直して返す。失敗したらログにエラーを出して、空のモデルを返す(落ちない)
    // 中で使うライブラリ(ufbx)の型は、この公開ヘッダーに出さない(GltfLoader と同じ作法)
    //
    // 今は、メッシュ(三角形・法線・UV)・骨(名前・親・基準姿勢・逆バインド)・重み(mesh.skin。1 頂点 4 本まで)・動き(clips。1 秒 30 回の Linear に焼き付け)
    // 座標:モデルの座標 = FBX の世界の座標(Blender の置き場所を、そのまま持つ)。骨の基準姿勢は、バインド時の世界の行列から作る
    ModelData LoadFbx(const std::filesystem::path& relativePath);

    // 確認用:骨の重みの処理(5 本以上を、大きい順の 4 本に絞る・合計 1・骨に無いクラスターを捨てる・重みなし → 骨 0・集計)を、作った偽のデータで検査する。
    // 5 本以上の骨が付いた FBX が、手元に無くても、この処理が正しいか、確かめられる。全部合っていれば true(結果は、ログに出す)
    bool RunFbxSkinSelfTest();
}
