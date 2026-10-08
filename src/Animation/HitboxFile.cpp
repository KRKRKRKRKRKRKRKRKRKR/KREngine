#include "KR/Animation/HitboxFile.h"

#include "KR/Core/Assets.h"
#include "KR/Core/Log.h"

#include <nlohmann/json.hpp>

#include <fstream>

using namespace DirectX;
using nlohmann::json;

namespace KR
{
    namespace
    {
        constexpr int FileVersion = 1; // ファイルの形式の版。形式を変えたら、上げる

        std::filesystem::path GetFullPath(const std::filesystem::path& relativePath)
        {
            return GetAssetDirectory("Models") / relativePath;
        }

        json ToJson(const Hitbox& hitbox, const Skeleton& skeleton)
        {
            json item;
            item["name"] = hitbox.name;
            // 骨は、名前(主)と番号(予備)の両方を書く
            const bool validJoint = hitbox.jointIndex >= 0 && static_cast<size_t>(hitbox.jointIndex) < skeleton.joints.size();
            item["joint"] = validJoint ? skeleton.joints[static_cast<size_t>(hitbox.jointIndex)].name : std::string();
            item["jointIndex"] = hitbox.jointIndex;
            item["shape"] = (hitbox.shape == HitShape::Box) ? "box" : "sphere";
            item["center"] = { hitbox.center.x, hitbox.center.y, hitbox.center.z };
            item["rotation"] = { hitbox.rotation.x, hitbox.rotation.y, hitbox.rotation.z, hitbox.rotation.w };
            item["halfSize"] = { hitbox.halfSize.x, hitbox.halfSize.y, hitbox.halfSize.z };
            item["radius"] = hitbox.radius;
            return item;
        }

        // 骨を探す:名前で → 無ければ番号で(範囲内なら)。見つからなければ -1
        int FindJoint(const json& item, const Skeleton& skeleton)
        {
            const std::string name = item.value("joint", std::string());
            if (!name.empty())
            {
                for (size_t i = 0; i < skeleton.joints.size(); ++i)
                {
                    if (skeleton.joints[i].name == name)
                    {
                        return static_cast<int>(i);
                    }
                }
            }
            const int index = item.value("jointIndex", -1);
            if (index >= 0 && static_cast<size_t>(index) < skeleton.joints.size())
            {
                return index;
            }
            return -1;
        }
    }

    std::filesystem::path GetHitboxFilePath(const std::filesystem::path& modelRelativePath)
    {
        std::filesystem::path result = modelRelativePath;
        result.replace_extension(".hitboxes.json"); // 例 Boss.glb → Boss.hitboxes.json
        return result;
    }

    bool SaveHitboxes(const std::filesystem::path& relativePath, const std::vector<Hitbox>& hitboxes, const Skeleton& skeleton)
    {
        const std::filesystem::path file = GetFullPath(relativePath);
        try
        {
            json root;
            root["version"] = FileVersion;
            root["hitboxes"] = json::array();
            for (const Hitbox& hitbox : hitboxes)
            {
                root["hitboxes"].push_back(ToJson(hitbox, skeleton));
            }

            std::error_code error;
            std::filesystem::create_directories(file.parent_path(), error);
            std::ofstream stream(file, std::ios::binary);
            if (!stream.is_open())
            {
                KR_LOG_ERROR("Hitbox: 保存できません(ファイルを開けない): {}", file.string());
                return false;
            }
            stream << root.dump(2) << '\n';
            if (!stream.good())
            {
                KR_LOG_ERROR("Hitbox: 保存できません(書き込みに失敗): {}", file.string());
                return false;
            }
        }
        catch (const std::exception& e)
        {
            KR_LOG_ERROR("Hitbox: 保存できません: {}({})", file.string(), e.what());
            return false;
        }

        KR_LOG_INFO("Hitbox: {} 個を保存しました: {}", hitboxes.size(), file.filename().string());
        return true;
    }

    HitboxLoadResult LoadHitboxes(const std::filesystem::path& relativePath, const Skeleton& skeleton, std::vector<Hitbox>& out)
    {
        out.clear();
        const std::filesystem::path file = GetFullPath(relativePath);

        std::ifstream stream(file, std::ios::binary);
        if (!stream.is_open())
        {
            return HitboxLoadResult::NotFound; // ファイルが無いのは、エラーではない
        }

        try
        {
            const json root = json::parse(stream);
            const int version = root.value("version", 0);
            if (version != FileVersion)
            {
                KR_LOG_ERROR("Hitbox: {} の形式の版が違います(ファイル {} / このエンジン {})。読みません", file.filename().string(), version, FileVersion);
                return HitboxLoadResult::Invalid;
            }

            for (const json& item : root.at("hitboxes"))
            {
                Hitbox hitbox;
                hitbox.name = item.value("name", std::string());
                hitbox.jointIndex = FindJoint(item, skeleton);
                if (hitbox.jointIndex < 0)
                {
                    KR_LOG_WARN("Hitbox: 骨「{}」が見つかりません。この形を飛ばします({})", item.value("joint", std::string()), hitbox.name);
                    continue;
                }

                hitbox.shape = (item.value("shape", std::string("box")) == "sphere") ? HitShape::Sphere : HitShape::Box;
                const std::vector<float> center = item.at("center").get<std::vector<float>>();
                const std::vector<float> rotation = item.at("rotation").get<std::vector<float>>();
                const std::vector<float> halfSize = item.at("halfSize").get<std::vector<float>>();
                if (center.size() != 3 || rotation.size() != 4 || halfSize.size() != 3)
                {
                    KR_LOG_ERROR("Hitbox: {} の「{}」の、数字の数が違います", file.filename().string(), hitbox.name);
                    out.clear();
                    return HitboxLoadResult::Invalid;
                }
                hitbox.center = { center[0], center[1], center[2] };
                hitbox.rotation = { rotation[0], rotation[1], rotation[2], rotation[3] };
                hitbox.halfSize = { halfSize[0], halfSize[1], halfSize[2] };
                hitbox.radius = item.value("radius", 0.5f);
                out.push_back(hitbox);
            }
        }
        catch (const std::exception& e)
        {
            KR_LOG_ERROR("Hitbox: {} を読めません({})", file.filename().string(), e.what());
            out.clear();
            return HitboxLoadResult::Invalid;
        }

        KR_LOG_INFO("Hitbox: {} 個を読みました: {}", out.size(), file.filename().string());
        return HitboxLoadResult::Loaded;
    }
}
