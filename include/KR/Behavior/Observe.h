#pragma once

#include "KR/Behavior/Graph.h"

#include <filesystem>
#include <optional>
#include <string>

// 動いている敵の「今の状態」を、F1 のツール(KRBehaviorEditor)のグラフの上で光らせる(リアルタイムの観測。Debug だけ)
//
// 使い方(ゲーム):見てほしい敵を 1 体、**毎フレーム** OnUpdate の中で知らせる(Runner::Update の後)
//   KR::Behavior::ObserveRunner(enemy.runner, enemy.asset);
// ・呼ばなかったフレームは「見ている敵なし」。敵が倒されて消えたら、呼ぶのをやめるだけでよい(エンジンは、敵を持たない = その場で写し取る)
// ・Release では、何もしない(#ifdef を書かなくてよい)
// ・エンジンが、約 0.1 秒おきに「観測ファイル」(LiveGraph.h と同じ一時フォルダーの observe.json)に書き、ツールが読む
namespace KR::Behavior
{
    class Runner;
    class GraphAsset;

    // 見てほしい敵を知らせる(毎フレーム)。グラフの名前は asset のファイル名(拡張子なし。例 Walker)
    void ObserveRunner(const Runner& runner, const GraphAsset& asset);

    // ---- ここから下は、ツール用(ゲームは呼ばなくてよい)----

    // 観測ファイルの中身
    struct Observation
    {
        bool connected = false;  // ゲームとつながっているか(観測ファイルが、約 1 秒以内に書かれた)
        bool observing = false;  // ゲームが、敵を見せているか(このフレームに ObserveRunner が呼ばれた)
        bool paused = false;     // ゲームが止まっているか(F1 のツールの「ゲームを止める」)
        std::string graphName;   // 見ている敵のグラフの名前(例 Walker)
        NodeId nodeId = 0;       // 今の状態のノード
        std::string stateType;   // 今の状態の種類(例 Patrol)
        float timeInState = 0.0f; // 今の状態に入ってからの秒
    };

    // その Enemies フォルダーの観測ファイルを読む。無い・古い(約 1 秒より前)・読めない → connected = false
    Observation ReadObservation(const std::filesystem::path& enemiesFolder);
}
