#include "KR/Behavior/Observe.h"

#include "KR/Behavior/GraphAsset.h"
#include "KR/Behavior/LiveGraph.h"
#include "KR/Behavior/Runner.h"
#include "Behavior/ObserveState.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <fstream>

namespace KR::Behavior
{
    namespace
    {
        constexpr const char* ObserveFileName = "observe.json";
        constexpr float WriteInterval = 0.1f; // 何秒おきに書くか(ツールも同じくらいの間隔で読む = 遅れは最大で約 0.2 秒)
        constexpr float StaleSeconds = 1.0f;  // これより古い観測ファイルは「ゲームとつながっていない」(ゲームが閉じた・落ちた)

        std::string ToUtf8(const std::filesystem::path& path)
        {
            const std::u8string utf8 = path.u8string();
            return std::string(utf8.begin(), utf8.end());
        }

        std::filesystem::path GetObserveFile(const std::filesystem::path& enemiesFolder)
        {
            return GetLiveGraphFolder(enemiesFolder) / ObserveFileName;
        }

#ifdef KR_DEBUG
        // 最後に写し取った、見ている敵の状態(敵そのものは持たない = 敵が消えても安全)
        struct Snapshot
        {
            std::string graphName;
            NodeId nodeId = 0;
            std::string stateType;
            float timeInState = 0.0f;
        };

        Snapshot g_Snapshot;
        bool g_HasSnapshot = false;      // 写した物があるか
        bool g_ObservedThisFrame = false; // このフレームに ObserveRunner が呼ばれたか
        bool g_EverObserved = false;      // 一度でも呼ばれたか(呼ばないゲーム = すれすれ飛行などは、観測ファイルを書かない)
        float g_Elapsed = 0.0f;           // 前に書いてからの秒

        // 別名で書いてから置き換える(書きかけを読ませない)。置き換えに失敗したら(ツールが読んでいる最中など)、何もしない = 次の回に書く
        void WriteObserveFile(const std::filesystem::path& enemiesFolder, bool observing, bool paused)
        {
            const std::filesystem::path file = GetObserveFile(enemiesFolder);
            std::error_code error;
            std::filesystem::create_directories(file.parent_path(), error);

            nlohmann::json json;
            json["version"] = 1;
            json["observing"] = observing;
            json["paused"] = paused;
            if (observing)
            {
                json["graph"] = g_Snapshot.graphName;
                json["node"] = g_Snapshot.nodeId;
                json["state"] = g_Snapshot.stateType;
                json["time"] = g_Snapshot.timeInState;
            }

            std::filesystem::path temp = file;
            temp += ".tmp";
            {
                std::ofstream stream(temp, std::ios::binary | std::ios::trunc);
                if (!stream)
                {
                    return;
                }
                stream << json.dump();
            }
            std::filesystem::rename(temp, file, error);
        }
#endif
    }

    void ObserveRunner(const Runner& runner, const GraphAsset& asset)
    {
#ifdef KR_DEBUG
        const StateView state = runner.GetCurrentState();
        g_Snapshot.graphName = ToUtf8(asset.GetFile().stem()); // 例 Walker(ツールが開いているグラフの名前と比べる)
        g_Snapshot.nodeId = state.GetNodeId();
        g_Snapshot.stateType = state.GetType();
        g_Snapshot.timeInState = state.GetTimeInState();
        g_HasSnapshot = true;
        g_ObservedThisFrame = true;
        g_EverObserved = true;
#else
        (void)runner;
        (void)asset;
#endif
    }

    namespace ObserveState
    {
        void EndFrame(float deltaTime, bool paused, const std::filesystem::path& enemiesFolder)
        {
#ifdef KR_DEBUG
            // 止まっていないのに呼ばれなかった = 見ている敵なし(敵が消えた・選んでいない)。止まっている間は、最後の状態のまま
            if (!paused && !g_ObservedThisFrame)
            {
                g_HasSnapshot = false;
            }
            g_ObservedThisFrame = false;

            g_Elapsed += deltaTime;
            if (!g_EverObserved || g_Elapsed < WriteInterval)
            {
                return;
            }
            g_Elapsed = 0.0f;
            // 変わっていなくても書く(ツールにとって「ゲームが生きている」印)
            WriteObserveFile(enemiesFolder, g_HasSnapshot, paused);
#else
            (void)deltaTime;
            (void)paused;
            (void)enemiesFolder;
#endif
        }

        void Shutdown(const std::filesystem::path& enemiesFolder)
        {
#ifdef KR_DEBUG
            if (!g_EverObserved)
            {
                return; // 書いていない
            }
            std::error_code error;
            std::filesystem::remove(GetObserveFile(enemiesFolder), error);
#else
            (void)enemiesFolder;
#endif
        }
    }

    Observation ReadObservation(const std::filesystem::path& enemiesFolder)
    {
        Observation result;
        const std::filesystem::path file = GetObserveFile(enemiesFolder);

        // 古い = ゲームが書くのをやめた(閉じた・落ちた)
        std::error_code error;
        const auto written = std::filesystem::last_write_time(file, error);
        if (error)
        {
            return result; // 無い
        }
        const auto age = std::filesystem::file_time_type::clock::now() - written;
        if (std::chrono::duration<float>(age).count() > StaleSeconds)
        {
            return result;
        }

        std::ifstream stream(file, std::ios::binary);
        if (!stream)
        {
            return result; // ゲームが置き換えている最中など。次の回に読む
        }
        const nlohmann::json json = nlohmann::json::parse(stream, nullptr, false); // 例外を投げない形(壊れていたら discarded)
        if (json.is_discarded() || !json.is_object())
        {
            return result;
        }
        result.connected = true;
        result.observing = json.value("observing", false);
        result.paused = json.value("paused", false);
        if (result.observing)
        {
            result.graphName = json.value("graph", std::string());
            result.nodeId = json.value("node", NodeId{ 0 });
            result.stateType = json.value("state", std::string());
            result.timeInState = json.value("time", 0.0f);
        }
        return result;
    }
}
