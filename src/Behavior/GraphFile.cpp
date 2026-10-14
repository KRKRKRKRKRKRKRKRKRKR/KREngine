#include "KR/Behavior/GraphFile.h"

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
        constexpr int FileVersion = 1; // ファイルの形式の版。形式を変えたら、上げる

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

        json ToJson(const Node& node)
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
                root["nodes"].push_back(ToJson(node));
            }
            root["links"] = json::array();
            for (const Link& link : graph.links)
            {
                root["links"].push_back({ { "from", link.from }, { "to", link.to }, { "priority", link.priority } });
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
            return node;
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
            if (version != FileVersion)
            {
                throw std::runtime_error("版が違う: " + std::to_string(version));
            }

            Graph graph;
            graph.version = version;
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

            // 線は、無くてもよい(つながっていない = その状態のまま)。存在しないノードへの線は、警告して捨てる
            if (root.contains("links"))
            {
                for (const json& item : root.at("links"))
                {
                    const Link link{ item.at("from").get<int>(), item.at("to").get<int>(), item.value("priority", 0) };
                    if (graph.FindNode(link.from) == nullptr || graph.FindNode(link.to) == nullptr)
                    {
                        KR_LOG_WARN("Behavior: 存在しないノードへの線を捨てました: {} → {}({})", link.from, link.to, ToUtf8(file.filename()));
                        continue;
                    }
                    graph.links.push_back(link);
                }
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
                    graph.startNode, firstState->id, ToUtf8(file.filename()));
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

        KR_LOG_INFO("Behavior: グラフを読みました: {}(ノード {}・線 {})", ToUtf8(file.filename()), out.nodes.size(), out.links.size());
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
