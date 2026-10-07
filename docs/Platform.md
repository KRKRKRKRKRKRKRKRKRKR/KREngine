# プラットフォーム(キー操作・OS メッセージ・ログ)

Win32 のウィンドウ操作、OS からのメッセージ、ログの仕組み。

[← 資料の案内(Structure.md)に戻る](Structure.md)

## キー操作とサイズ変更

| 操作 | 流れ | 場所 |
|---|---|---|
| F11 | Window::ToggleBorderless → WM_SIZE → Renderer::Resize | [Window.cpp:174](../src/Platform/Windows/Window.cpp#L174) |
| サイズ変更 | Flush → SwapChain と深度バッファを作り直す(縦横比は毎フレーム自動で計算) | [Renderer.cpp:162](../src/Renderer/Renderer.cpp#L162) |
| F10(Debug のみ) | Application::OnKeyDown → Renderer::ReloadShaders(失敗したら前のシェーダーのまま) | [Application.cpp:62](../src/Core/Application.cpp#L62) |

**シェーダーファイルの場所**:Debug は `Assets/Shaders`(編集中の元ファイル)、Release は exe の横の `Shaders/`(ビルド時にコピー)。


---

## OS からのメッセージ

| メッセージ | 処理 | 場所 |
|---|---|---|
| `WM_NCCREATE` | HWND と Window オブジェクト(this)を結びつける | [Window.cpp:93](../src/Platform/Windows/Window.cpp#L93) |
| すべてのメッセージ(先頭) | `MessageHook`(窓口)に先に見せる。Application がこれを Renderer 経由で ImGui につなぐ。ImGui が処理したら、そこで終わり | [Window.cpp](../src/Platform/Windows/Window.cpp) |
| `WM_SIZE` | サイズを更新してコールバックを呼ぶ | [Window.cpp:113](../src/Platform/Windows/Window.cpp#L113) |
| `WM_KEYDOWN` / `WM_SYSKEYDOWN` | F11 はボーダーレス、それ以外はキーのコールバックへ(F10 のメニューは止める) | [Window.cpp:124](../src/Platform/Windows/Window.cpp#L124) |
| `WM_KEYUP` / `WM_SYSKEYUP` | キーを離したコールバックへ(F10 のメニューは止める) | [Window.cpp](../src/Platform/Windows/Window.cpp) |
| `WM_KILLFOCUS` | 非アクティブになったコールバックへ(押しっぱなしのまま固まらないように、全部離した扱いにする) | [Window.cpp](../src/Platform/Windows/Window.cpp) |
| `WM_DPICHANGED` | 拡大率の違うモニターへ移動したときにサイズを調整 | [Window.cpp:150](../src/Platform/Windows/Window.cpp#L150) |
| `WM_DESTROY` | 終了要求を出す | [Window.cpp:166](../src/Platform/Windows/Window.cpp#L166) |


---

## ログ

- `KR_LOG_TRACE / INFO / WARN / ERROR / FATAL`:Release では TRACE と INFO がコードごと消える
- 出力先:コンソール(色付き)と VS の出力ウィンドウ
- D3D12 デバッグレイヤー、シェーダーのコンパイルエラー、無効な番号札のエラーもログに流れる
- `KR_DX_CHECK(...)`:DX12 の関数が失敗したら Fatal で停止する
- 終了時に DX12 オブジェクトの解放漏れをチェックする

