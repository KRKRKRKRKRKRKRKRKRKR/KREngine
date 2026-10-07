#pragma once

#include <filesystem>

namespace KR
{
    // ゲームのデータ(モデル・コース・設定など)のフォルダーの場所を返す。name は、ゲームのアセットの下のフォルダー名(例:"Courses")
    //   Debug  : 編集中の元フォルダー(ゲームの Assets/<name>。CMake の KR_GAME_ASSETS_DIR)。保存した変更が、コピーなしで、すぐ読める
    //   Release: exe の横のフォルダー(<exe のあるフォルダー>/<name>)。ビルド後に、元フォルダーから、自動でコピーされる
    // 「Debug は元のフォルダー、Release は exe の横」という決まりを、ここ 1 か所にまとめてある(別々に書くと、直し忘れる)
    std::filesystem::path GetAssetDirectory(const std::filesystem::path& name);

    // エンジンのシェーダーのフォルダーの場所を返す(シェーダーは、ゲームではなく、エンジンの物)
    //   Debug  : エンジンの shaders/ の元フォルダー(KR_SHADER_SOURCE_DIR)。保存した変更を、F10 で、再読み込みできる
    //   Release: exe の横の Shaders/(ビルド後に、エンジンの shaders/ から、自動でコピーされる)
    std::filesystem::path GetEngineShaderDirectory();
}
