#pragma once

#include <filesystem>

// 観測(Observe.h)の、エンジン内部の窓口(Application だけが呼ぶ。InputState と同じ形)
namespace KR::Behavior::ObserveState
{
    // 毎フレーム、メインループの最後に呼ぶ。約 0.1 秒おきに、観測ファイルを書く(Debug だけ)
    //   paused = ゲームが止まっている(OnUpdate を呼ばなかった)。止まっている間は、最後に写した状態のまま「止まっている」と書く
    //   止まっていないのに、このフレームに ObserveRunner が呼ばれなかったら「見ている敵なし」
    void EndFrame(float deltaTime, bool paused, const std::filesystem::path& enemiesFolder);

    // ゲームの終了時:観測ファイルを消す(ツールは、すぐ「つながっていません」になる)
    void Shutdown(const std::filesystem::path& enemiesFolder);
}
