#pragma once

#include <filesystem>

namespace KR
{
    // データ(シェーダー・モデル・コースなど)のフォルダーの場所を返す。name は、Assets の下のフォルダー名(例:"Courses")
    //   Debug  : 編集中の元フォルダー(Assets/<name>)。保存した変更が、コピーなしで、すぐ読める
    //   Release: exe の横のフォルダー(<exe のあるフォルダー>/<name>)。ビルド後に、元フォルダーから、自動でコピーされる
    // 「Debug は元のフォルダー、Release は exe の横」という決まりを、ここ 1 か所にまとめてある(別々に書くと、直し忘れる)
    std::filesystem::path GetAssetDirectory(const std::filesystem::path& name);
}
