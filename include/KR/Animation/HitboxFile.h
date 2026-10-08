#pragma once

#include "KR/Animation/Hitbox.h"
#include "KR/Animation/Skeleton.h"

#include <filesystem>
#include <vector>

namespace KR
{
    // モデルのファイル(例 "Boss/Boss.glb")に対応する、判定の形のファイルの場所(例 "Boss/Boss.hitboxes.json")。Assets/Models からの相対パス
    std::filesystem::path GetHitboxFilePath(const std::filesystem::path& modelRelativePath);

    // 判定の形を、JSON ファイルに保存する。relativePath は Assets/Models からの相対パス(GetHitboxFilePath の結果など)
    // 骨は、名前と番号の両方を書く(読み込みは、名前で探す = 骨の並びが変わっても、壊れない)。失敗したら、エラーを出して false
    // Debug では、元の Assets フォルダーに書く(編集した結果が、残る)。Release では、exe の横の複製に書く(開発中の機能)
    bool SaveHitboxes(const std::filesystem::path& relativePath, const std::vector<Hitbox>& hitboxes, const Skeleton& skeleton);

    enum class HitboxLoadResult
    {
        Loaded,   // 読めた(out に入った)
        NotFound, // ファイルが無い(エラーではない。呼ぶ側が、既定の形を使う)
        Invalid,  // ファイルが壊れている・形式が違う(エラーを出した。out は空)
    };

    // 判定の形を、JSON ファイルから読む。骨は、名前で探す(無ければ番号。それも範囲外なら、警告して、その形を飛ばす)
    HitboxLoadResult LoadHitboxes(const std::filesystem::path& relativePath, const Skeleton& skeleton, std::vector<Hitbox>& out);
}
