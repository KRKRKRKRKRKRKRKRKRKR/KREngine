#include "KR/Behavior/GraphFile.h"

#include "KR/Core/Log.h"

#include <nlohmann/json.hpp>

#include <charconv>
#include <cstdlib>
#include <fstream>

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
}
