#pragma once

#include "KR/Behavior/Graph.h"

#include <memory>

namespace KR::Behavior
{
    // ノードエディタの部品:グラフを、ImGui の窓の中に、ノードと線として描く(imgui-node-editor を使う。型は、公開しない)
    // 窓(ImGui::Begin / End)は、呼ぶ側が作る。この部品は、窓の中身だけを描く
    // 見た目は、UE のブループリント風:状態 = 青・条件 = 緑・論理の条件(かつ・または・でない)= 紫の題。
    //   状態は、「遷移の行」を、ノードの中に並べる(左 = もし [条件の入口 ●]・右 = → 次の状態 [● 出口]。行ごとに、別のピン)
    //   線の色:流れ = 白(光らせるノードから出る線は、橙)/ 真偽 = 赤。開始ノード = 橙の枠と「[開始]」
    // 操作:ホイール = 拡大縮小 / 右ボタンのドラッグ = 画面の移動 / 左ボタンのドラッグ = ノードの移動・枠で複数選択
    //   (C5b〜:ピンから、ピンへドラッグ = 線 / 右クリック = ノードを足す・ここから始める・消す / Delete / 遷移の行の「＋」「▲▼」「✕」)
    class GraphEditor
    {
    public:
        GraphEditor();  // imgui-node-editor の文脈を、自分で作る(ImGui の文脈が、先にあること)
        ~GraphEditor();

        GraphEditor(const GraphEditor&) = delete;
        GraphEditor& operator=(const GraphEditor&) = delete;

        // グラフを描く(毎フレーム。ImGui の窓の中で呼ぶ)。graph を編集したら、true を返す(ノードを動かした・値の欄を変えた・線やノードを足した / 消した)
        bool Draw(Graph& graph);

        // 検証の警告の一覧(ImGui の窓の中で呼ぶ。エディタの上か下に置く)。警告が無ければ「警告なし」。中身は ValidateGraph(GraphValidate.h)
        void DrawWarnings(const Graph& graph);

        // 別のグラフを開いたとき(読み直し・新規など)に呼ぶ。ノードの位置を、グラフから、取り直す
        void Reset();

        // 光らせるノード(ゲームの敵の今の状態。Observe.h)。0 = 光らせない。Draw の前に呼ぶ
        // そのノードの題と枠を橙(paused = ゲームが止まっているなら灰色)・そのノードから出る線を橙にする(DrawGraphDebug と同じ見せ方)
        // id で探すので、直している途中でも、そのノードが残っていれば光り続ける。無ければ、何も光らない
        void SetObservedNode(NodeId node, bool paused);

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
}
