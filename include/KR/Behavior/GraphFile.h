#pragma once

#include "KR/Behavior/Graph.h"

#include <filesystem>

namespace KR::Behavior
{
    // グラフを、JSON ファイル(版 1)に保存する。file は、フルパス
    // (例 GetAssetDirectory("Enemies") / "Walker.json")。フォルダーが無ければ作る
    // 一時ファイルに書いてから置き換える(書いている途中のファイルを、読まれない)
    // 失敗したら、エラーを出して false
    bool SaveGraph(const Graph& graph, const std::filesystem::path& file);
}
