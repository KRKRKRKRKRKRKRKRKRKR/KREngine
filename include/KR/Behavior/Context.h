#pragma once

#include "KR/Behavior/Graph.h"

#include <string>

namespace KR::Behavior
{
    // ゲームが毎フレーム入れる「今の状況」:キー(文字列)→ 値
    // 例:SetFloat("distanceToPlayerX", 2.5f)・SetBool("attackFinished", true)
    // エンジンは、ゲーム固有の型を知らない。条件ノードが、キーで値を読む
    class Context
    {
    public:
        void SetFloat(const std::string& key, float value);
        void SetInt(const std::string& key, int value);
        void SetBool(const std::string& key, bool value);

        // キーが無い・型が違うときは、defaultValue を返す(GetFloat は、int の値も float として読む)
        float GetFloat(const std::string& key, float defaultValue = 0.0f) const;
        int GetInt(const std::string& key, int defaultValue = 0) const;
        bool GetBool(const std::string& key, bool defaultValue = false) const;

        // そのキーに、float・int・bool のどれかの値が入っているか
        bool Has(const std::string& key) const;

        void Clear();

    private:
        ParamMap values_;
    };
}
