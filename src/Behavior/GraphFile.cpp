#include "KR/Behavior/GraphFile.h"

#include "GraphEdit.h"
#include "KR/Behavior/NodeTypes.h"
#include "KR/Core/Log.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <stdexcept>

using nlohmann::json;

namespace KR::Behavior
{
    namespace
    {
        constexpr int FileVersion = 2;        // いま書く、ファイルの形式の版。形式を変えたら、上げる
        constexpr int OldestReadable = 1;     // 読める、いちばん古い版(版 1 = 状態 → 条件 → 状態の線。読み込むときに、版 2 に変換する)

        // ログ用。path::string() は、日本語の Windows で CP932 になり、ログ(UTF-8)が文字化けする(2Dアクションゲーム のフォルダーなど)
        std::string ToUtf8(const std::filesystem::path& path)
        {
            const std::u8string utf8 = path.u8string();
            return std::string(utf8.begin(), utf8.end());
        }

        // float は、いちばん短い表記(0.2 は 0.2)で書く。そのまま double にすると 0.20000000298… と書かれてしまう
        json ToJson(float value)
        {
            char buffer[32];
            const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
            return std::strtod(std::string(buffer, result.ptr).c_str(), nullptr);
        }

        template <typename T>
        json ToJson(const T& value)
        {
            return json(value);
        }

        json ToJson(const ParamValue& value)
        {
            return std::visit([](const auto& v) { return ToJson(v); }, value);
        }

        json ToJson(const Node& node, const NodeTypeInfo* info)
        {
            json item;
            item["id"] = node.id;
            item["kind"] = (node.kind == NodeKind::State) ? "state" : "condition";
            item["type"] = node.type;
            item["pos"] = { ToJson(node.posX), ToJson(node.posY) };
            item["params"] = json::object();
            for (const auto& [name, value] : node.params)
            {
                item["params"][name] = ToJson(value);
            }
            if (node.kind == NodeKind::State)
            {
                // 遷移の行:並びの順 = 調べる順。0 = つながっていない
                item["transitions"] = json::array();
                for (const Transition& transition : node.transitions)
                {
                    item["transitions"].push_back({ { "condition", transition.condition }, { "to", transition.to } });
                }
            }
            else if (info != nullptr && info->IsLogic())
            {
                item["inputs"] = node.inputs; // かつ・または・でない の入力(条件の id。0 = つながっていない)
            }
            return item;
        }
    }

    bool SaveGraph(const Graph& graph, const std::filesystem::path& file)
    {
        try
        {
            json root;
            root["version"] = FileVersion;
            root["graph"] = graph.graphType;
            root["start"] = graph.startNode;
            root["nextId"] = graph.nextNodeId;
            root["nodes"] = json::array();
            for (const Node& node : graph.nodes)
            {
                root["nodes"].push_back(ToJson(node, FindNodeType(node.type)));
            }

            std::error_code error;
            if (file.has_parent_path())
            {
                std::filesystem::create_directories(file.parent_path(), error);
            }

            std::filesystem::path temp = file;
            temp += ".tmp";
            {
                std::ofstream stream(temp, std::ios::binary);
                if (!stream.is_open())
                {
                    KR_LOG_ERROR("Behavior: 保存できません(ファイルを開けない): {}", ToUtf8(file));
                    return false;
                }
                stream << root.dump(2) << '\n';
                if (!stream.good())
                {
                    KR_LOG_ERROR("Behavior: 保存できません(書き込みに失敗): {}", ToUtf8(file));
                    return false;
                }
            }
            std::filesystem::rename(temp, file, error); // 既にあれば、置き換わる(Windows)
            if (error)
            {
                std::filesystem::remove(temp, error);
                KR_LOG_ERROR("Behavior: 保存できません(置き換えに失敗): {}", ToUtf8(file));
                return false;
            }
        }
        catch (const std::exception& e)
        {
            KR_LOG_ERROR("Behavior: 保存できません: {}({})", ToUtf8(file), e.what());
            return false;
        }

        KR_LOG_INFO("Behavior: グラフを保存しました: {}", ToUtf8(file.filename()));
        return true;
    }

    namespace
    {
        // 値の型は、登録(spec)の既定値の型で決まる。ファイルの値が型に合わなければ、既定値のまま
        ParamValue ParamFromJson(const json& value, const ParamSpec& spec)
        {
            if (std::holds_alternative<float>(spec.defaultValue) && value.is_number())
            {
                return value.get<float>();
            }
            if (std::holds_alternative<int>(spec.defaultValue) && value.is_number())
            {
                return value.get<int>();
            }
            if (std::holds_alternative<bool>(spec.defaultValue) && value.is_boolean())
            {
                return value.get<bool>();
            }
            if (std::holds_alternative<std::string>(spec.defaultValue) && value.is_string())
            {
                return value.get<std::string>();
            }
            return spec.defaultValue;
        }

        // ノード 1 つ(版 1・版 2 で共通の部分:id・種類・位置・値)。遷移の行・入力は、あとで、つなぐ
        Node NodeFromJson(const json& item)
        {
            Node node;
            node.id = item.at("id").get<int>();
            node.type = item.at("type").get<std::string>();
            const NodeTypeInfo* info = FindNodeType(node.type);
            if (info == nullptr)
            {
                throw std::runtime_error("未知のノードの種類: " + node.type);
            }
            node.kind = info->kind;
            if (item.contains("pos") && item["pos"].is_array() && item["pos"].size() >= 2)
            {
                node.posX = item["pos"][0].get<float>();
                node.posY = item["pos"][1].get<float>();
            }
            const json params = item.value("params", json::object());
            for (const ParamSpec& spec : info->params)
            {
                const auto found = params.find(spec.name);
                node.params[spec.name] = (found != params.end()) ? ParamFromJson(*found, spec) : spec.defaultValue;
            }
            node.inputs.assign(static_cast<size_t>(info->minInputs), 0); // 論理の条件:入力のスロット(あとで、ファイルの値を入れる)
            return node;
        }

        // 版 1 の線 1 本:from → to。意味は「状態 → 条件 → 状態」。priority の小さい順に調べる
        struct OldLink
        {
            NodeId from = 0;
            NodeId to = 0;
            int priority = 0;
        };

        // 版 1 → 版 2:状態から出る線(状態 → 条件)ごとに、その状態に、遷移の行 { 条件, 条件の出力の先の状態 } を足す
        //   行の順 = 状態から出る線の priority の順(同じなら、ファイルの並び)。条件から出る線が複数なら、その順に、行を足す(版 1 の Runner は、最初の「状態」への線を使う = 先頭の行が、先に成り立つ)
        //   条件の出力が、つながっていない(または、状態でない)なら、行は { 条件, 0 }(to = 0 の行は、成り立っても、移らない = 次の行を調べる。版 1 の Runner と同じ)
        //   状態 → 状態・条件 → 条件の線は、版 1 の Runner が、使わなかった(エディタも、作れなかった)ので、捨てる
        void ConvertOldLinks(Graph& graph, std::vector<OldLink> links, const std::string& fileName)
        {
            std::stable_sort(links.begin(), links.end(), [](const OldLink& a, const OldLink& b) { return a.priority < b.priority; });
            for (const OldLink& stateToCondition : links)
            {
                Node* state = graph.FindNode(stateToCondition.from);
                const Node* condition = graph.FindNode(stateToCondition.to);
                if (state == nullptr || condition == nullptr || state->kind != NodeKind::State || condition->kind != NodeKind::Condition)
                {
                    if (state != nullptr && condition != nullptr && state->kind == condition->kind)
                    {
                        KR_LOG_WARN("Behavior: 版 1 の、使われない線(同じ種類どうし)を捨てました: {} → {}({})", stateToCondition.from, stateToCondition.to, fileName);
                    }
                    continue;
                }

                bool hasState = false;
                for (const OldLink& conditionToState : links) // 条件から出る線(priority の順)
                {
                    const Node* target = graph.FindNode(conditionToState.to);
                    if (conditionToState.from == condition->id && target != nullptr && target->kind == NodeKind::State)
                    {
                        state->transitions.push_back({ condition->id, target->id });
                        hasState = true;
                    }
                }
                if (!hasState)
                {
                    state->transitions.push_back({ condition->id, 0 });
                }
            }
        }

        // 版 2:遷移の行と、論理の入力を、つなぐ。存在しない id・種類が合わない・循環になる つながりは、警告して、外す(0 にする)
        void ApplyV2Connections(Graph& graph, const json& nodesJson, const std::string& fileName)
        {
            for (const json& item : nodesJson)
            {
                Node* node = graph.FindNode(item.at("id").get<int>());
                if (node == nullptr)
                {
                    continue;
                }

                if (node->kind == NodeKind::State && item.contains("transitions") && item["transitions"].is_array())
                {
                    for (const json& row : item["transitions"])
                    {
                        const int index = AddTransition(graph, node->id);
                        const int condition = row.value("condition", 0);
                        const int to = row.value("to", 0);
                        if (!SetTransitionCondition(graph, node->id, index, condition))
                        {
                            KR_LOG_WARN("Behavior: 遷移の行の条件({})を外しました(無い・条件でない)({})", condition, fileName);
                        }
                        if (!SetTransitionTarget(graph, node->id, index, to))
                        {
                            KR_LOG_WARN("Behavior: 遷移の行の次の状態({})を外しました(無い・状態でない)({})", to, fileName);
                        }
                    }
                }
            }

            // 論理の入力は、すべてのノードが、そろってから(循環の判定のため)
            for (const json& item : nodesJson)
            {
                const NodeId id = item.at("id").get<int>();
                const Node* node = graph.FindNode(id);
                if (node == nullptr || node->kind != NodeKind::Condition || !item.contains("inputs") || !item["inputs"].is_array())
                {
                    continue;
                }
                const NodeTypeInfo* info = FindNodeType(node->type);
                if (info == nullptr || !info->IsLogic())
                {
                    continue;
                }
                const json& inputs = item["inputs"];
                while (static_cast<int>(inputs.size()) > static_cast<int>(graph.FindNode(id)->inputs.size()) && AddInput(graph, id))
                {
                }
                if (static_cast<int>(inputs.size()) > info->maxInputs)
                {
                    KR_LOG_WARN("Behavior: 論理の条件({})の入力が、多すぎるので、{} 個までにしました({})", id, info->maxInputs, fileName);
                }
                for (size_t i = 0; i < inputs.size() && i < graph.FindNode(id)->inputs.size(); ++i)
                {
                    const int source = inputs[i].is_number_integer() ? inputs[i].get<int>() : 0;
                    if (!SetInput(graph, id, static_cast<int>(i), source))
                    {
                        KR_LOG_WARN("Behavior: 論理の条件({})の入力({})を外しました(無い・条件でない・循環)({})", id, source, fileName);
                    }
                }
            }
        }
    }

    GraphLoadResult LoadGraphFile(const std::filesystem::path& file, Graph& out)
    {
        out = Graph();

        std::ifstream stream(file, std::ios::binary);
        if (!stream.is_open())
        {
            return GraphLoadResult::NotFound;
        }

        try
        {
            const json root = json::parse(stream);
            const int version = root.at("version").get<int>();
            if (version < OldestReadable || version > FileVersion)
            {
                throw std::runtime_error("版が違う: " + std::to_string(version));
            }
            const std::string fileName = ToUtf8(file.filename());

            Graph graph; // graph.version は 2(版 1 のファイルも、読み込むときに、版 2 に変換する)
            graph.graphType = root.value("graph", std::string("fsm"));
            graph.startNode = root.value("start", 0);
            for (const json& item : root.at("nodes"))
            {
                Node node = NodeFromJson(item);
                if (graph.FindNode(node.id) != nullptr)
                {
                    throw std::runtime_error("ノードの id が重なっている: " + std::to_string(node.id));
                }
                graph.nodes.push_back(std::move(node));
            }

            // 次に発行する id:ファイルの値と、今ある id の最大 + 1 の、大きい方(古いファイルや手で直したファイルでも、重ならない)
            graph.nextNodeId = std::max(1, root.value("nextId", 1));
            for (const Node& node : graph.nodes)
            {
                graph.nextNodeId = std::max(graph.nextNodeId, node.id + 1);
            }

            if (version == 1)
            {
                // 版 1:線の一覧(無くてもよい = つながっていない)。存在しないノードへの線は、警告して捨てる
                std::vector<OldLink> links;
                if (root.contains("links"))
                {
                    for (const json& item : root.at("links"))
                    {
                        const OldLink link{ item.at("from").get<int>(), item.at("to").get<int>(), item.value("priority", 0) };
                        if (graph.FindNode(link.from) == nullptr || graph.FindNode(link.to) == nullptr)
                        {
                            KR_LOG_WARN("Behavior: 存在しないノードへの線を捨てました: {} → {}({})", link.from, link.to, fileName);
                            continue;
                        }
                        links.push_back(link);
                    }
                }
                ConvertOldLinks(graph, std::move(links), fileName);
                KR_LOG_INFO("Behavior: 版 1 のグラフを、版 2 に変換して読みました(保存すると、版 2 になります): {}", fileName);
            }
            else
            {
                ApplyV2Connections(graph, root.at("nodes"), fileName);
            }

            // 開始ノードが無い・状態でないときは、最初の状態にする。状態が 1 つも無ければ、壊れたファイル
            const Node* start = graph.FindNode(graph.startNode);
            if (start == nullptr || start->kind != NodeKind::State)
            {
                const auto firstState = std::find_if(graph.nodes.begin(), graph.nodes.end(),
                    [](const Node& node) { return node.kind == NodeKind::State; });
                if (firstState == graph.nodes.end())
                {
                    throw std::runtime_error("状態のノードが 1 つも無い");
                }
                KR_LOG_WARN("Behavior: 開始ノード({})が正しくないので、最初の状態({})から始めます({})",
                    graph.startNode, firstState->id, fileName);
                graph.startNode = firstState->id;
            }
            out = std::move(graph);
        }
        catch (const std::exception& e)
        {
            out = Graph();
            KR_LOG_ERROR("Behavior: グラフを読めません: {}({})", ToUtf8(file), e.what());
            return GraphLoadResult::Invalid;
        }

        size_t rows = 0;
        for (const Node& node : out.nodes)
        {
            rows += node.transitions.size();
        }
        KR_LOG_INFO("Behavior: グラフを読みました: {}(ノード {}・遷移の行 {})", ToUtf8(file.filename()), out.nodes.size(), rows);
        return GraphLoadResult::Loaded;
    }

    GraphPtr LoadGraph(const std::filesystem::path& file)
    {
        Graph graph;
        const GraphLoadResult result = LoadGraphFile(file, graph);
        if (result == GraphLoadResult::Loaded)
        {
            return std::make_shared<const Graph>(std::move(graph));
        }
        if (result == GraphLoadResult::NotFound)
        {
            KR_LOG_ERROR("Behavior: グラフのファイルがありません: {}", ToUtf8(file));
        }

        return MakeDefaultGraph();
    }
}
