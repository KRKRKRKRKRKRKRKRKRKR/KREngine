#pragma once

#include "KR/Behavior/Graph.h"

#include <memory>

namespace KR::Behavior
{
    // ノードエディタの部品:グラフを、ImGui の窓の中に、ノードと線として描く(imnodes を使う。型は、公開しない)
    // 窓(ImGui::Begin / End)は、呼ぶ側が作る。この部品は、窓の中身だけを描く
    class GraphEditor
    {
    public:
        GraphEditor();  // imnodes の文脈を、自分で作る(ImGui の文脈が、先にあること)
        ~GraphEditor();

        GraphEditor(const GraphEditor&) = delete;
        GraphEditor& operator=(const GraphEditor&) = delete;

        // グラフを描く(毎フレーム。ImGui の窓の中で呼ぶ)。graph を編集したら、true を返す(ノードを動かした・値の欄を変えた・線やノードを足した / 消した)
        // 状態 = 青・条件 = 緑・開始ノード = 橙の枠と「開始」の印
        // 線:出力ピン(右)→ 入力ピン(左)へ、ドラッグして、つなぐ。つなげるのは、状態 → 条件・条件 → 状態だけ(条件は、入力 1・出力 1 まで)
        // 消す:線を選んで Delete・線をピンから引き離す
        // ノード:何もない所で右クリック → 登録された種類の一覧から足す。選んで Delete で消す(つながる線も消える)
        bool Draw(Graph& graph);

        // 別のグラフを開いたとき(読み直し・新規など)に呼ぶ。ノードの位置を、グラフから、取り直す
        void Reset();

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
}
