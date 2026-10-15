---
title: Utility(版・起動の引数)
project: KREngine
kind: 使い方
status: 最新
updated: 2026-10-15
replaces:
---

# Utility

## 概要
小さな補助の関数。**エンジンの版**を調べる `GetVersion()`・`GetVersionString()` と、**起動したときの引数**を読む `GetCommandLineArguments()`。

版は、`メジャー.マイナー.パッチ`(例 `0.13.0`)。

- **パッチ**:バグ修正だけ(使い方は変わらない)。
- **マイナー**:機能の追加(今までの使い方は、そのまま動く)。
- **メジャー**:互換性が壊れる変更(ゲームのコードを直す必要がある)。0 の間は、開発中(マイナーでも、互換性が変わることがある)。

決まりは `docs/Versioning.md`・変更の記録は `CHANGELOG.md`。

ヘッダー:`KR/Core/Version.h`・`KR/Core/CommandLine.h`

## サンプルコード
```cpp
void OnInit() override
{
    KR_LOG_INFO("エンジンの版: {}", KR::GetVersionString());     // 例 "0.13.0"

    const KR::Version v = KR::GetVersion();
    if (v.major == 0 && v.minor < 13) { KR_LOG_WARN("エンジンが古い"); }

    // 起動の引数:先頭は exe のパス。日本語のパスも読める
    const std::vector<std::filesystem::path> args = KR::GetCommandLineArguments();
    for (size_t i = 1; i < args.size(); ++i)
    {
        KR_LOG_INFO("引数 {}: {}", i, args[i].string());
    }
}
```

## 関数

### KR::GetVersionString()　版の文字
```cpp
std::string GetVersionString();
```
`"0.13.0"` の形の文字列。起動時のログの最初の行にも出る。

### KR::GetVersion()　版の数字
```cpp
Version GetVersion();
```
このエンジンをビルドしたときの版(`CMakeLists.txt` の `project(KREngine VERSION ...)` が元)。

### KR::GetCommandLineArguments()　起動の引数
```cpp
std::vector<std::filesystem::path> GetCommandLineArguments();
```
起動したときの、コマンドラインの引数。先頭は、exe のパス。日本語などを含むパスも、正しく読める。`OnInit` などで、いつでも呼べる。

## 列挙型・構造体

### Version　エンジンの版
| メンバー | 説明 |
|---|---|
| `major` | メジャー(互換性が壊れる変更) |
| `minor` | マイナー(機能の追加) |
| `patch` | パッチ(バグ修正) |

## アクセサー一覧
なし。
