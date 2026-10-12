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

namespace KR::Behavior
{
    enum class GraphLoadResult
    {
        Loaded,   // 読めた(out に入った)
        NotFound, // ファイルが無い
        Invalid,  // 壊れている・形式が違う(エラーを出した。out は空のグラフ)
    };

    // グラフを、JSON ファイルから読む。file は、フルパス
    GraphLoadResult LoadGraphFile(const std::filesystem::path& file, Graph& out);

    // ゲーム向けの窓口:読めたらそのグラフ。読めなければ、エラーを出して、安全な既定のグラフ(待機 1 つ)
    // いつも null でないグラフを返す
    GraphPtr LoadGraph(const std::filesystem::path& file);
}
