#include "KR/Behavior/GraphAsset.h"

#include "KR/Behavior/GraphFile.h"
#include "KR/Behavior/LiveGraph.h"
#include "KR/Core/Log.h"

namespace KR::Behavior
{
    namespace
    {
        // ログ用(日本語のフォルダーで、文字化けしないように)
        std::string ToUtf8(const std::filesystem::path& path)
        {
            const std::u8string utf8 = path.u8string();
            return std::string(utf8.begin(), utf8.end());
        }
    }

    std::optional<GraphAsset::Stamp> GraphAsset::ReadStamp(const std::filesystem::path& file)
    {
        if (file.empty())
        {
            return std::nullopt;
        }
        std::error_code error;
        const auto time = std::filesystem::last_write_time(file, error);
        if (error)
        {
            return std::nullopt;
        }
        const auto size = std::filesystem::file_size(file, error);
        if (error)
        {
            return std::nullopt;
        }
        return Stamp{ time, size };
    }

    bool GraphAsset::Load(const std::filesystem::path& file)
    {
        file_ = file;
#ifdef KR_DEBUG
        // Debug だけ:ツールが、保存する前のグラフを書く「ライブファイル」も見る(リアルタイム編集。LiveGraph.h)
        liveFile_ = GetLiveGraphFile(file);
#endif
        elapsed_ = 0.0f;
        lastStamp_ = ReadStamp(file_);
        lastLiveStamp_ = ReadStamp(liveFile_);

        // ライブファイルがあれば(ツールで編集中に、ゲームを起動した)、そちらを使う
        const bool useLive = lastLiveStamp_.has_value();
        Graph loaded;
        if (LoadGraphFile(useLive ? liveFile_ : file_, loaded) == GraphLoadResult::Loaded)
        {
            graph_ = std::make_shared<const Graph>(std::move(loaded));
            ++version_;
            if (useLive)
            {
                KR_LOG_INFO("Behavior: ツールで編集中のグラフ(ライブファイル)を使います: {}", ToUtf8(file_.filename()));
            }
            return true;
        }
        // 読めなかった:待機 1 つの既定のグラフ(ファイルが、あとで直ったら、ReloadIfChanged が読み直す)
        KR_LOG_ERROR("Behavior: グラフを読めないので、待機だけのグラフを使います: {}", ToUtf8(file_));
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
        // 本物とライブの、両方の印を見る。どちらも変わっていなければ、何もしない
        const std::optional<Stamp> stamp = ReadStamp(file_);
        const std::optional<Stamp> liveStamp = ReadStamp(liveFile_);
        if (stamp == lastStamp_ && liveStamp == lastLiveStamp_)
        {
            return false; // 変わっていない(ファイルが無いままも、含む)
        }
        const bool liveRemoved = lastLiveStamp_.has_value() && !liveStamp.has_value(); // ツールが、保存した・閉じた・変更を捨てた
        lastStamp_ = stamp; // 読めても読めなくても、この印は「見た」ことにする(壊れたファイルを、0.5 秒ごとに、読み直さない)
        lastLiveStamp_ = liveStamp;

        // ライブがあれば、ライブを読む。無ければ、本物を読む(ライブが消えたときは、本物が変わっていなくても、本物に戻す)
        const bool useLive = liveStamp.has_value();
        if (!useLive && !stamp.has_value())
        {
            KR_LOG_WARN("Behavior: グラフのファイルが、なくなりました(今のグラフのまま使います): {}", ToUtf8(file_));
            return false;
        }

        Graph loaded;
        if (LoadGraphFile(useLive ? liveFile_ : file_, loaded) != GraphLoadResult::Loaded)
        {
            KR_LOG_WARN("Behavior: 書き換わったグラフを読めないので、前のグラフのまま使います: {}{}", ToUtf8(file_.filename()), useLive ? "(ライブファイル)" : "");
            return false; // 壊れていたら、前のまま
        }
        graph_ = std::make_shared<const Graph>(std::move(loaded));
        ++version_;
        if (useLive)
        {
            KR_LOG_INFO("Behavior: ツールで編集中のグラフ(ライブファイル)を読み直しました(版 {}): {}", version_, ToUtf8(file_.filename()));
        }
        else
        {
            KR_LOG_INFO("Behavior: グラフを読み直しました(版 {}{}): {}", version_, liveRemoved ? "。ライブファイルが消えたので、保存の内容に戻す" : "", ToUtf8(file_.filename()));
        }
        return true;
    }
}
