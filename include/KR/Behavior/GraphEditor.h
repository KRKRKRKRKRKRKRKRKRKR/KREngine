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

        // グラフを描く(毎フレーム。ImGui の窓の中で呼ぶ)。graph を編集したら、true を返す(今は、ノードを動かしたときだけ)
        // 状態 = 青・条件 = 緑・開始ノード = 橙の枠と「開始」の印
        bool Draw(Graph& graph);

        // 別のグラフを開いたとき(読み直し・新規など)に呼ぶ。ノードの位置を、グラフから、取り直す
        void Reset();

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
}
