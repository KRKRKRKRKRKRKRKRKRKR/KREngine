#pragma once

// 条件ノードの評価(エンジンの中だけ。ゲームには見せない)

#include "KR/Behavior/Context.h"
#include "KR/Behavior/Graph.h"

namespace KR::Behavior
{
    // 条件ノード 1 つが、今、成り立っているか
    // timeInState = 今の状態に入ってからの秒(TimeElapsed が使う)
    // 状況(Context)にキーが無いときは、偽
    bool EvaluateCondition(const Node& node, const Context& context, float timeInState);

    // 条件のノード(葉でも、かつ・または・でない でもよい)を、入力をたどって、評価する(ネストできる)
    //   かつ(And)= すべての入力が真 / または(Or)= どれかの入力が真 / でない(Not)= 入力が偽
    //   つながっていない入力・id 0・無いノードは、偽(Not も、入力がつながっていなければ、偽)
    //   循環・ネストが深すぎる(上限 32)ときは、**全体が偽** + 警告(1 回)。同じノードは、1 回の評価で、1 度しか評価しない(共有された部分で、遅くならない)
    bool EvaluateConditionTree(const Graph& graph, NodeId conditionId, const Context& context, float timeInState);
}
