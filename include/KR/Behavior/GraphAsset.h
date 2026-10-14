#pragma once

#include "KR/Behavior/Graph.h"

#include <cstdint>
#include <filesystem>
#include <optional>

namespace KR::Behavior
{
    // グラフのファイル 1 つを持って、ファイルが書き換わったら、読み直す(ツールで保存したら、動いているゲームに、すぐ反映する)
    // 前例:シェーダーの F10。ただし、キーを押さなくても、更新時刻を、約 0.5 秒おきに見て、自動で読み直す
    //
    // 使い方(ゲーム):
    //   GraphAsset asset;  asset.Load(KR::GetAssetDirectory("Enemies") / "Walker.json");   // 起動時
    //   Runner runner(asset.GetGraph());
    //   毎フレーム:  asset.ReloadIfChanged(deltaTime);
    //                if (asset.GetVersion() != runnerVersion) { runner.SetGraph(asset.GetGraph()); runnerVersion = asset.GetVersion(); }
    //   (敵が複数いるなら、敵ごとに runnerVersion を持つ。Runner::SetGraph が、今の状態を引き継ぐ)
    //
    // リアルタイム編集(Debug だけ):ツールが、保存する前のグラフを「ライブファイル」(LiveGraph.h)に書く。
    // ライブファイルがあれば、本物のファイルより先に、そちらを読む。ライブファイルが消えたら(ツールで保存・閉じた)、本物に戻る。ゲームのコードは、そのまま
    class GraphAsset
    {
    public:
        // 読む。読めなかったら(ファイル無し・壊れている)、エラーを出して、待機 1 つの既定のグラフにして、false を返す
        // 読めなかったファイルも、あとで直った(書き換わった)ら、ReloadIfChanged が、読み直す
        bool Load(const std::filesystem::path& file);

        // deltaTime ぶん進めて、約 0.5 秒たつごとに、ファイルの更新時刻・大きさを見る。変わっていたら、読み直す
        // 読み直しが成功したら true(GetGraph が、新しいグラフになる・GetVersion が増える)
        // **壊れたファイルは、前のグラフのまま**(エラーを出す。同じ壊れたファイルを、繰り返し読まない = 書き換わるまで、待つ)
        bool ReloadIfChanged(float deltaTime);

        // 今すぐ、ファイルを見る(待たない)。成功したら true
        bool ReloadNow();

        // 今のグラフ(Load の前でも、null ではない = 待機 1 つの既定のグラフ)
        const GraphPtr& GetGraph() const { return graph_; }

        // 読み直しに成功するたびに、1 ずつ増える(Load の成功も、数える)。ゲームが、「自分の持っているグラフは古いか」を、比べるのに使う
        uint32_t GetVersion() const { return version_; }

        // 何秒おきに、ファイルを見るか(既定 0.5)
        void SetCheckInterval(float seconds) { interval_ = seconds; }

        const std::filesystem::path& GetFile() const { return file_; }

    private:
        struct Stamp
        {
            std::filesystem::file_time_type time;
            std::uintmax_t size = 0;
            bool operator==(const Stamp&) const = default;
        };

        static std::optional<Stamp> ReadStamp(const std::filesystem::path& file); // ファイルが無い(場所が空)なら nullopt
        bool Reload();

        std::filesystem::path file_;
        std::filesystem::path liveFile_; // ライブファイルの場所(Debug だけ。Release では空 = 見ない)
        GraphPtr graph_ = MakeDefaultGraph();
        uint32_t version_ = 0;
        std::optional<Stamp> lastStamp_;     // 最後に、読もうとしたときの、本物のファイルの印(読めても、読めなくても)
        std::optional<Stamp> lastLiveStamp_; // 同じく、ライブファイルの印
        float elapsed_ = 0.0f;
        float interval_ = 0.5f;
    };
}
