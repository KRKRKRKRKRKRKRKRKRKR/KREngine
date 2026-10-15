---
title: Audio(音)
project: KREngine
kind: 使い方
status: 最新
updated: 2026-10-15
replaces:
---

# Audio

## 概要
効果音・BGM を鳴らす窓口。WAV・OGG・MP3 を読める。`Application` が 1 つ持っているので、ゲームは `GetAudio()` で使う。自分で作らない。音のファイルは、ゲームの `Assets/Sounds/` に置く。

- **番号札**:読み込んだ音は `SoundHandle`、鳴っている音 1 回ぶんは `VoiceHandle`。同じ音を 3 回鳴らすと、`VoiceHandle` は 3 つ。鳴り終わると、エンジンが自動で片づける(札は、そのまま持っていても安全)。
- **音の機器が無い・準備に失敗したとき**は、警告を出して、音を出さずに動き続ける(ゲームは、分岐を書かなくてよい)。
- BGM は、継ぎ目に隙間が出にくい **OGG** を勧める(MP3 は、ループの継ぎ目にすき間が出やすい)。

ヘッダー:`KR/Audio/Audio.h`

## サンプルコード
効果音とループする BGM。

```cpp
void OnInit() override
{
    m_Jump  = GetAudio().Load("jump.wav");                 // Assets/Sounds/jump.wav
    m_Music = GetAudio().Load("stage1.ogg");
    m_Bgm   = GetAudio().Play(m_Music, true, 0.5f);        // BGM:ループする・音量 0.5
}
void OnUpdate(float dt) override
{
    if (KR::Input::IsKeyPressed(KR::Key::Space))
    {
        GetAudio().Play(m_Jump);                            // 効果音:重ねて鳴らせる
    }
    if (KR::Input::IsKeyPressed(KR::Key::M))
    {
        GetAudio().Stop(m_Bgm);                             // 止める
    }
}
void OnShutdown() override
{
    GetAudio().Unload(m_Jump);                              // 忘れても、終了時にエンジンが片づける
    GetAudio().Unload(m_Music);
}
```

## 関数

### Audio::Load()　音を読む
```cpp
SoundHandle Load(const std::filesystem::path& relativePath);
```
音のファイル(WAV・OGG・MP3)を、全部メモリに読む。読めなければ、エラーをログに出して、無効の札を返す。

| 引数 | 説明 |
|---|---|
| `relativePath` | ゲームの `Assets/Sounds/` からの場所(例 `"jump.wav"`) |

### Audio::Unload()　音を捨てる
```cpp
void Unload(SoundHandle sound);
```
読んだ音を捨てる。その音で鳴っている声は、先に止める。

### Audio::Play()　鳴らす
```cpp
VoiceHandle Play(SoundHandle sound, bool loop = false, float volume = 1.0f);
```
音を鳴らす。鳴らせなければ(無効の札・音の機器が無い)、無効の札を返す。

| 引数 | 説明 |
|---|---|
| `sound` | `Load()` で得た音の札 |
| `loop` | `true` で、`Stop()` するまで繰り返す |
| `volume` | 音量。0(無音)〜 1(そのまま) |

| 戻り値 | 説明 |
|---|---|
| `VoiceHandle` | 鳴っている音の札。`Stop()` に渡す |

### Audio::Stop()　止める
```cpp
void Stop(VoiceHandle voice);
```
鳴っている音を止める。もう鳴り終わった札・無効の札でも、何もしないだけ。

### Audio::SetMasterVolume()　全体の音量
```cpp
void SetMasterVolume(float volume);
```
全体の音量(0 = 無音、1 = そのまま)。鳴っている音にも、すぐ効く。

### Audio::Update()　鳴り終わった声を片づける
```cpp
void Update();
```
エンジンが、毎フレーム呼ぶ。**ゲームは呼ばなくてよい**。

## 列挙型・構造体

### SoundHandle　読み込んだ音の番号札
| メンバー | 説明 |
|---|---|
| `id` | 番号。0 は無効(まだ `Load` していない・読めなかった) |
| `IsValid()` | 有効か |

### VoiceHandle　鳴っている音の番号札
`SoundHandle` と同じ形。XAudio2 の言葉の「声(voice)」。

## アクセサー一覧
| 関数 | 説明 |
|---|---|
| `GetMasterVolume()` | 全体の音量を返す |
| `SoundHandle::IsValid()` | 音の札が有効か |
| `VoiceHandle::IsValid()` | 声の札が有効か |
