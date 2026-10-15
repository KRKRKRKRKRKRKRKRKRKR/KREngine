---
title: AnimatedModel(動くモデル)
project: KREngine
kind: 使い方
status: 最新
updated: 2026-10-15
replaces:
---

# AnimatedModel

## 概要
動くモデル(骨・動き・判定の形・描画)を、**1 体ぶん、まとめて扱う**窓口。**ゲームは、これだけ使えばよい**。glTF(`.glb` / `.gltf`)と FBX(`.fbx`)を読める(拡張子で、自動で選ぶ)。モデルは、ゲームの `Assets/Models/` に置く。

流れ:`Load()` → `Play()` → 毎フレーム `Update()` → `Draw()` → `Unload()`

- **動き(アニメーション)**:モデルに入っている動きに、名前(または番号)で選んで、再生する。
- **判定の形**:骨にくっついた箱・球。骨が動くと、判定も動く。位置・大きさは、ツール `KRAnimationEditor` で調整して、`<モデル名>.hitboxes.json` に保存する。ゲームは、読むだけ。ファイルが無ければ、骨ごとの既定の形が作られる。
- Blender からの書き出し方・注意は、`docs/Usage.md` の 7。

ヘッダー:`KR/Animation/AnimatedModel.h`

## サンプルコード
読み込み・再生・描画・当たり判定。

```cpp
class MyGame : public KR::Application
{
protected:
    void OnInit() override
    {
        m_Boss.Load(GetRenderer(), "Boss/Boss.glb");      // Assets/Models/ からの相対パス。.fbx も可
        m_Boss.Play("Walk");                               // 動きを選ぶ(名前か番号)。最初の動きは、ループで再生しはじめている
    }
    void OnUpdate(float dt) override
    {
        m_Boss.Update(dt);                                 // 時刻を進めて、姿勢を求める

        if (KR::Input::IsKeyPressed(KR::Key::Space)) { m_Boss.Play("Attack", false); }   // ループしない動き
        if (m_Boss.IsFinished()) { m_Boss.Play("Walk"); }                                // 終わったら戻す

        // 当たり判定:今の姿勢の、世界の形(箱・球)。プレイヤーの球と、全部の形を調べる
        const DirectX::XMMATRIX world = DirectX::XMMatrixTranslation(0.0f, 0.0f, 5.0f);
        for (const KR::WorldHitbox& hit : m_Boss.GetWorldHitboxes(world))
        {
            KR::WorldHitbox player; player.shape = KR::HitShape::Sphere; player.sphere = { m_PlayerPos, 0.5f };
            if (KR::Intersects(hit, player)) { KR_LOG_INFO("ボスに当たった"); }
        }
    }
    void OnRender() override
    {
        m_Boss.Draw(GetRenderer(), DirectX::XMMatrixTranslation(0.0f, 0.0f, 5.0f));   // 判定に渡した行列と、同じ物
    }
    void OnShutdown() override { m_Boss.Unload(GetRenderer()); }

private:
    KR::AnimatedModel m_Boss;
    DirectX::XMFLOAT3 m_PlayerPos = { 0, 1, 0 };
};
```

## 関数

### AnimatedModel::Load()　モデルを読む
```cpp
bool Load(Renderer& renderer, const std::filesystem::path& relativePath);
```
モデルを読む。形・骨・動き・判定の形を用意して、最初の動きを、ループで再生しはじめる。失敗したら、エラーをログに出して `false`。判定の形は、同じ場所の `<名前>.hitboxes.json` があれば読み、無ければ、骨ごとの既定の形を作る。

| 引数 | 説明 |
|---|---|
| `renderer` | `GetRenderer()` |
| `relativePath` | `Assets/Models/` からの相対パス(例 `"Boss/Boss.glb"`)。`.glb`・`.gltf`・`.fbx` |

### AnimatedModel::Unload()　片づける
```cpp
void Unload(Renderer& renderer);
```
GPU の形を返して、空にする。`Renderer` を捨てる前(`OnShutdown`)に呼ぶ。

### AnimatedModel::Play()　動きを選んで再生する
```cpp
bool Play(size_t clipIndex, bool loop = true);
bool Play(const std::string& clipName, bool loop = true);
```
動きを選んで、**最初から**再生する(再生中の動きを選び直しても、最初に戻る)。見つからなければ、警告を出して `false`(今の再生は、そのまま)。

| 引数 | 説明 |
|---|---|
| `clipIndex` / `clipName` | 動きの番号 / 名前 |
| `loop` | `true` で繰り返す。`false` なら、最後で止まる(`IsFinished()` が `true` になる) |

### AnimatedModel::Stop()　再生をやめる
```cpp
void Stop();
```
動きをやめて、基準姿勢(動かない姿勢)に戻す。

### AnimatedModel::Pause()　一時停止
```cpp
void Pause(bool paused = true);
```
`true` で止める(時刻が進まない)。`false` で、続きから再生する。

### AnimatedModel::SetSpeed()　再生の速さ
```cpp
void SetSpeed(float speed);
```
1 = 等倍・0 = 止まる・負の値は、逆再生。

### AnimatedModel::SetLooping()　ループの切り替え
```cpp
void SetLooping(bool loop);
```
再生中の動きを、ループするかを変える。

### AnimatedModel::SetTime()　時刻を決める
```cpp
void SetTime(float time);
```
動きの時刻(秒)を決めて、姿勢も求め直す。止めているときに、姿勢を確かめるときなどに使う。

### AnimatedModel::Update()　毎フレーム進める
```cpp
void Update(float deltaTime);
```
**毎フレーム 1 回**、`OnUpdate` で呼ぶ。時刻を進めて、姿勢を求める(`Pause` 中は、時刻を進めない)。

### AnimatedModel::Draw()　描く
```cpp
void Draw(Renderer& renderer, const DirectX::XMMATRIX& world, TextureHandle texture = {}) const;
```
今の姿勢で、形を描く(`OnRender` で呼ぶ)。骨つきは `DrawSkinnedMesh`・骨なしは `DrawMesh` を、自動で選ぶ。

| 引数 | 説明 |
|---|---|
| `world` | モデル全体の置き方の行列 |
| `texture` | 貼る画像(省略すると、頂点の色) |

### AnimatedModel::GetWorldHitboxes()　判定の形を、世界の座標で取る
```cpp
std::vector<WorldHitbox> GetWorldHitboxes(const DirectX::XMMATRIX& world) const;
```
今の姿勢での、判定の形(世界の座標)を返す。`GetHitboxes()` と同じ数・同じ並び。`world` は、`Draw()` に渡す行列と、**同じ物**。`KR::Intersects(WorldHitbox, WorldHitbox)` で調べる。→ [Hitbox](Hitbox.md)

### AnimatedModel::SaveHitboxes()　判定の形を保存する
```cpp
bool SaveHitboxes() const;
```
判定の形を、`<名前>.hitboxes.json` に保存する(Debug では、元の `Assets` フォルダー)。失敗したら `false`。普通は、ツールで調整して保存する。

### AnimatedModel::ReloadHitboxes()　判定の形を読み直す
```cpp
bool ReloadHitboxes();
```
ファイルから、読み直す。ファイルが無い・壊れていれば、骨ごとの既定の形に戻す。ファイルから読めたら `true`。

## 列挙型・構造体

### AnimatedModel::Bounds　モデルを囲む箱
基準姿勢での、メッシュを囲む箱(モデルの座標)。カメラを置いたり、中心を決めたりするのに使う。

| メンバー | 説明 |
|---|---|
| `min`, `max` | 箱の最小・最大の角 |
| `Center()` | 中心 |
| `Radius()` | 中心から、いちばん遠い角までの距離(箱の対角線の半分) |

## アクセサー一覧
| 関数 | 説明 |
|---|---|
| `IsLoaded()` | 読み込み済みか |
| `GetClipCount()` | 動きの数 |
| `GetClipName(index)` | 動きの名前 |
| `GetCurrentClip()` | 再生中の動きの番号(無い・`Stop` 後は −1) |
| `GetDuration()` | 再生中の動きの長さ(秒) |
| `GetTime()` | 動きの、今の時刻(秒) |
| `IsFinished()` | ループしない動きが、最後まで再生されたか |
| `IsPaused()` | 一時停止中か |
| `GetSpeed()` | 再生の速さ |
| `IsLooping()` | ループするか |
| `GetSkeleton()` | 骨の階層(`Skeleton`) |
| `GetPose()` | 今の姿勢(各骨の、世界の行列) |
| `GetBounds()` | モデルを囲む箱 |
| `GetHitboxes()` | 骨にくっついた判定の形(編集できる) |
