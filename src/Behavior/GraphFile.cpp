#include "KR/Behavior/GraphFile.h"

#include "KR/Behavior/NodeTypes.h"
#include "KR/Core/Log.h"

#include <nlohmann/json.hpp>

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
                    KR_LOG_ERROR("Behavior: 保存できません(ファイルを開けない): {}", file.string());
                    return false;
                }
                stream << root.dump(2) << '\n';
                if (!stream.good())
                {
                    KR_LOG_ERROR("Behavior: 保存できません(書き込みに失敗): {}", file.string());
                    return false;
                }
            }
            std::filesystem::rename(temp, file, error); // 既にあれば、置き換わる(Windows)
            if (error)
            {
                std::filesystem::remove(temp, error);
                KR_LOG_ERROR("Behavior: 保存できません(置き換えに失敗): {}", file.string());
                return false;
            }
        }
        catch (const std::exception& e)
        {
            KR_LOG_ERROR("Behavior: 保存できません: {}({})", file.string(), e.what());
            return false;
        }

        KR_LOG_INFO("Behavior: グラフを保存しました: {}", file.filename().string());
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
                graph.nodes.push_back(NodeFromJson(item));
            }
            for (const json& item : root.at("links"))
            {
                graph.links.push_back({ item.at("from").get<int>(), item.at("to").get<int>(), item.value("priority", 0) });
            }
            out = std::move(graph);
        }
        catch (const std::exception& e)
        {
            out = Graph();
            KR_LOG_ERROR("Behavior: グラフを読めません: {}({})", file.string(), e.what());
            return GraphLoadResult::Invalid;
        }

        KR_LOG_INFO("Behavior: グラフを読みました: {}(ノード {}・線 {})", file.filename().string(), out.nodes.size(), out.links.size());
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
            KR_LOG_ERROR("Behavior: グラフのファイルがありません: {}", file.string());
        }

        // 安全な既定:待機 1 つ
        Graph fallback;
        Node wait;
        wait.id = 1;
        wait.kind = NodeKind::State;
        wait.type = "Wait";
        fallback.startNode = wait.id;
        fallback.nodes.push_back(wait);
        return std::make_shared<const Graph>(std::move(fallback));
    }
}
