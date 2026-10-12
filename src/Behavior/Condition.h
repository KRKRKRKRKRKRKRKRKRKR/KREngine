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
}
