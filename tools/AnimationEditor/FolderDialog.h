#pragma once

#include <filesystem>
#include <optional>

// Windows の「フォルダーを選ぶ」ダイアログを出す(ツールの中だけで使う。エンジン本体は、Windows の画面に依存しない)
// initial:最初に開くフォルダー(空でもよい)。選んだら、そのフォルダー。キャンセルしたら、nullopt
std::optional<std::filesystem::path> PickFolder(const std::filesystem::path& initial);
