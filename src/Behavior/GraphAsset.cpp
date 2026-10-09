#include "KR/Behavior/GraphAsset.h"

#include "KR/Behavior/GraphFile.h"
#include "KR/Core/Log.h"

namespace KR::Behavior
{
    std::optional<GraphAsset::Stamp> GraphAsset::ReadStamp() const
    {
        std::error_code error;
        const auto time = std::filesystem::last_write_time(file_, error);
        if (error)
        {
            return std::nullopt;
        }
        const auto size = std::filesystem::file_size(file_, error);
        if (error)
        {
            return std::nullopt;
        }
        return Stamp{ time, size };
    }

    bool GraphAsset::Load(const std::filesystem::path& file)
    {
        file_ = file;
        elapsed_ = 0.0f;
        lastStamp_ = ReadStamp();

        Graph loaded;
        if (LoadGraphFile(file_, loaded) == GraphLoadResult::Loaded)
        {
            graph_ = std::make_shared<const Graph>(std::move(loaded));
            ++version_;
            return true;
        }
        // 読めなかった:待機 1 つの既定のグラフ(ファイルが、あとで直ったら、ReloadIfChanged が読み直す)
        KR_LOG_ERROR("Behavior: グラフを読めないので、待機だけのグラフを使います: {}", file_.string());
        graph_ = MakeDefaultGraph();
        ++version_;
        return false;
    }

    bool GraphAsset::ReloadIfChanged(float deltaTime)
    {
        elapsed_ += deltaTime;
        if (elapsed_ < interval_)
        {
            return false;
        }
        elapsed_ = 0.0f;
        return Reload();
    }

    bool GraphAsset::ReloadNow()
    {
        elapsed_ = 0.0f;
        return Reload();
    }

    bool GraphAsset::Reload()
    {
        if (file_.empty())
        {
            return false;
        }
        const std::optional<Stamp> stamp = ReadStamp();
        if (stamp == lastStamp_)
        {
            return false; // 変わっていない(ファイルが無いままも、含む)
        }
        lastStamp_ = stamp; // 読めても読めなくても、この印は「見た」ことにする(壊れたファイルを、0.5 秒ごとに、読み直さない)
        if (!stamp.has_value())
        {
            KR_LOG_WARN("Behavior: グラフのファイルが、なくなりました(今のグラフのまま使います): {}", file_.string());
            return false;
        }

        Graph loaded;
        if (LoadGraphFile(file_, loaded) != GraphLoadResult::Loaded)
        {
            KR_LOG_WARN("Behavior: 書き換わったグラフを読めないので、前のグラフのまま使います: {}", file_.string());
            return false; // 壊れていたら、前のまま
        }
        graph_ = std::make_shared<const Graph>(std::move(loaded));
        ++version_;
        KR_LOG_INFO("Behavior: グラフを読み直しました(版 {}): {}", version_, file_.filename().string());
        return true;
    }
}
