---
title: Log(ログ)
project: KREngine
kind: 使い方
status: 最新
updated: 2026-10-15
replaces:
---

# Log

## 概要
文字を、コンソールに出す(ログ)。`std::format` と同じ書き方(`{}` に値が入る)で、**マクロ**として使う。マクロにしているのは、呼び出し元のファイル名・行番号を、自動で付けるため。

| マクロ | 意味 | Release |
|---|---|---|
| `KR_LOG_TRACE(...)` | 細かい様子 | **消える** |
| `KR_LOG_INFO(...)` | ふつうのお知らせ | **消える** |
| `KR_LOG_WARN(...)` | 警告 | 出る |
| `KR_LOG_ERROR(...)` | エラー | 出る |
| `KR_LOG_FATAL(...)` | 致命的なエラー。**出力のあと、プログラムが止まる** | 出る |

Debug では、コンソールが開いて、ログが見える。Release では、コンソールが無い。

ヘッダー:`KR/Core/Log.h`

## サンプルコード
```cpp
KR_LOG_INFO("起動した");
KR_LOG_INFO("プレイヤーの位置: ({}, {}, {})", pos.x, pos.y, pos.z);   // {} に、値が入る
KR_LOG_WARN("画像が大きすぎる: {} 画素", width);
KR_LOG_ERROR("ファイルを読めなかった: {}", path.string());

if (!m_Boss.Load(GetRenderer(), "Boss/Boss.glb"))
{
    KR_LOG_FATAL("ボスを読めない。続けられない");                       // ここで止まる
}
```

## 関数

### KR_LOG_INFO() ほか　ログを出す
```cpp
KR_LOG_TRACE("文字 {}", 値);
KR_LOG_INFO("文字 {}", 値);
KR_LOG_WARN("文字 {}", 値);
KR_LOG_ERROR("文字 {}", 値);
KR_LOG_FATAL("文字 {}", 値);
```
第 1 引数は、`std::format` の書式文字列(`{}` が値の場所)。第 2 引数以降が、値。日本語を書ける。

### Log::SetMinLevel()　出すログの下限
```cpp
void SetMinLevel(LogLevel level);
```
この重さより軽いログは、出さない。たとえば `LogLevel::Warn` にすると、`TRACE` と `INFO` が出なくなる。

### Log::Init()　初期化
```cpp
void Init();
```
エンジンが、起動時に呼ぶ。**ゲームは呼ばない**。

### Log::Write() / Log::Print()　ログを出す(マクロの中身)
```cpp
void Write(LogLevel level, std::string_view message, const std::source_location& location);
```
マクロが呼ぶ関数。**普通は、マクロを使う**(`KR_LOG_INFO` など)。

## 列挙型・構造体

### LogLevel　ログの重さ
軽い順に、`Trace`・`Info`・`Warn`・`Error`・`Fatal`。

## アクセサー一覧
なし。
