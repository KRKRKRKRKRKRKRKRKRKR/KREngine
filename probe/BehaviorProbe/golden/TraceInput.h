#pragma once

// 状態の列の確認用:決まった入力(状況)の列。古いエンジン(版 1 の形)で記録した「正解データ」と、新しいエンジンの結果を、同じ入力で比べるために使う
// 入力は、時刻だけで決まる(乱数なし)= いつ、どのエンジンで動かしても、同じ

#include <algorithm>
#include <cmath>

namespace GoldenTrace
{
    constexpr float StepSeconds = 0.05f;
    constexpr int StepCount = 2400; // 120 秒

    struct Input
    {
        float distanceX;
        float distanceY;
        bool attackFinished;
    };

    inline Input InputAt(int step)
    {
        const float t = static_cast<float>(step) * StepSeconds;
        Input input;
        // 横の距離:0〜12 m を、ゆっくり行き来する + 速い揺れ(境界を、何度も、またぐ)
        input.distanceX = std::max(0.0f, 6.0f + 5.0f * std::sin(t * 0.6f) + 1.5f * std::sin(t * 2.3f));
        // 縦の距離:0〜3 m(縦も含める条件のため)
        input.distanceY = 1.5f + 1.5f * std::sin(t * 0.9f);
        // 攻撃が終わった:1.5 秒の窓を、4.5 秒ごと
        input.attackFinished = ((step / 30) % 3) == 2;
        return input;
    }
}
