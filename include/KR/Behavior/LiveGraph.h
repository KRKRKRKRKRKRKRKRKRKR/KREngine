#pragma once

#include "KR/Behavior/Graph.h"

#include <filesystem>

// リアルタイム編集の「ライブファイル」(ツールが、保存する前のグラフを、ゲームへ渡すための一時ファイル)
// **ツールとエンジン用。ゲームは呼ばなくてよい**(GraphAsset が、Debug のとき、自動でライブファイルも見る)
//
// 場所:%TEMP%\KREngineLive\<印>\<名前>.json
//   印 = Enemies フォルダーの場所から作る 16 桁の数字(ツールとゲームが、同じフォルダーから、同じ場所を計算できる。別のゲームと混ざらない)
//   ゲームの Assets フォルダーには、何も出ない(Git に入らない)
namespace KR::Behavior
{
    // グラフのファイル(本物。例 <Assets>/Enemies/Walker.json)に対応する、ライブファイルの場所
    std::filesystem::path GetLiveGraphFile(const std::filesystem::path& graphFile);

    // Enemies フォルダーに対応する、ライブファイルのフォルダー
    std::filesystem::path GetLiveGraphFolder(const std::filesystem::path& enemiesFolder);

    // ライブファイルを書く(いったん別名で書いてから置き換える = 書きかけを読ませない)。失敗したら false
    bool WriteLiveGraph(const Graph& graph, const std::filesystem::path& graphFile);

    // ライブファイルを消す(無くてもよい)。消すと、ゲームは本物のファイル(保存の内容)に戻る
    void DeleteLiveGraphFile(const std::filesystem::path& graphFile);

    // その Enemies フォルダーのライブファイルを、全部消す(ツールの起動時・閉じたとき・ゲームが、ツールが閉じたのに気づいたとき)
    void DeleteAllLiveGraphFiles(const std::filesystem::path& enemiesFolder);
}
