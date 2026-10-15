#include "Condition.h"

#include "KR/Core/Log.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <utility>
#include <vector>

namespace KR::Behavior
{
    namespace
    {
        float FloatParam(const Node& node, const char* name, float defaultValue)
        {
            const auto found = node.params.find(name);
            if (found != node.params.end())
            {
                if (const float* value = std::get_if<float>(&found->second))
                {
                    return *value;
                }
            }
            return defaultValue;
        }

        bool BoolParam(const Node& node, const char* name, bool defaultValue)
        {
            const auto found = node.params.find(name);
            if (found != node.params.end())
            {
                if (const bool* value = std::get_if<bool>(&found->second))
                {
                    return *value;
                }
            }
            return defaultValue;
        }

        std::string StringParam(const Node& node, const char* name, const char* defaultValue)
        {
            const auto found = node.params.find(name);
            if (found != node.params.end())
            {
                if (const std::string* value = std::get_if<std::string>(&found->second))
                {
                    return *value;
                }
            }
            return defaultValue;
        }

        // 状況にキーがあるか。無ければ、そのキーについて、警告を 1 回だけ出す(毎フレームの洪水を防ぐ)
        bool HasKeyOrWarn(const Context& context, const std::string& key)
        {
            if (context.Has(key))
            {
                return true;
            }
            static std::set<std::string> warned;
            if (warned.insert(key).second)
            {
                KR_LOG_WARN("Behavior: 状況にキー \"{}\" がありません(この条件は偽になります。ゲームが SetFloat などで入れてください)", key);
            }
            return false;
        }

        enum class Zone
        {
            Unknown, // キーが無くて、判定できない
            Inside,  // 半径の内側(ちょうど半径も、内側)
            Outside,
        };

        // 横(X)だけの距離。includeVertical が true なら、縦(keyY)も含めて √(x² + y²)
        Zone JudgeZone(const Node& node, const Context& context)
        {
            const std::string keyX = StringParam(node, "key", "distanceToPlayerX");
            if (!HasKeyOrWarn(context, keyX))
            {
                return Zone::Unknown;
            }
            float distance = context.GetFloat(keyX);

            if (BoolParam(node, "includeVertical", false))
            {
                const std::string keyY = StringParam(node, "keyY", "distanceToPlayerY");
                if (!HasKeyOrWarn(context, keyY))
                {
                    return Zone::Unknown;
                }
                const float y = context.GetFloat(keyY);
                distance = std::sqrt(distance * distance + y * y);
            }

            return distance <= FloatParam(node, "radius", 0.0f) ? Zone::Inside : Zone::Outside;
        }
    }

    bool EvaluateCondition(const Node& node, const Context& context, float timeInState)
    {
        if (node.type == "PlayerWithin")
        {
            return JudgeZone(node, context) == Zone::Inside;
        }
        if (node.type == "PlayerOutside")
        {
            return JudgeZone(node, context) == Zone::Outside;
        }
        if (node.type == "TimeElapsed")
        {
            return timeInState >= FloatParam(node, "seconds", 0.0f);
        }
        if (node.type == "AttackFinished")
        {
            const std::string key = StringParam(node, "key", "attackFinished");
            return HasKeyOrWarn(context, key) && context.GetBool(key);
        }
        return false;
    }

    namespace
    {
        constexpr int MaxConditionDepth = 32; // 論理の条件の、ネストの深さの上限(深すぎるグラフで、スタックを使い切らないように)

        // 1 回の評価(1 つの行の入口)の間だけ使う入れ物
        struct TreeEvaluation
        {
            const Graph& graph;
            const Context& context;
            float timeInState;
            bool failed = false; // 循環・深すぎるネストを見つけた → 全体の結果を、偽にする
            // ノードごとの結果(同じノードを、何度もたどらない = 共有された部分の木で、爆発的に遅くならない)。-1 = 評価の途中 / 0 = 偽 / 1 = 真
            std::vector<std::pair<NodeId, signed char>> memo;

            signed char* Find(NodeId id)
            {
                for (auto& entry : memo)
                {
                    if (entry.first == id)
                    {
                        return &entry.second;
                    }
                }
                return nullptr;
            }
        };

        void WarnTreeOnce(NodeId id)
        {
            static bool warned = false; // 1 回だけ(毎フレームの洪水を防ぐ)
            if (!warned)
            {
                warned = true;
                KR_LOG_WARN("Behavior: 論理の条件が、循環している(または、深すぎる)ので、この条件は偽にします(ノード {}。上限 {})", id, MaxConditionDepth);
            }
        }

        bool EvaluateTree(TreeEvaluation& e, NodeId id, int depth)
        {
            if (id == 0)
            {
                return false; // つながっていない入力・入口 = 偽
            }
            const Node* node = e.graph.FindNode(id);
            if (node == nullptr || node->kind != NodeKind::Condition)
            {
                return false;
            }

            if (signed char* known = e.Find(id))
            {
                if (*known < 0) // 評価の途中のノードに、戻ってきた = 循環
                {
                    e.failed = true;
                    WarnTreeOnce(id);
                    return false;
                }
                return *known == 1;
            }
            if (depth > MaxConditionDepth)
            {
                e.failed = true;
                WarnTreeOnce(id);
                return false;
            }
            e.memo.emplace_back(id, static_cast<signed char>(-1));
            const size_t slot = e.memo.size() - 1;

            bool result = false;
            if (node->type == "And")
            {
                // すべての入力が成り立つ。つながっていない入力は、偽(= つなぎ終わるまで、成り立たない)。1 つ偽なら、そこで止める
                result = !node->inputs.empty();
                for (const NodeId input : node->inputs)
                {
                    if (!EvaluateTree(e, input, depth + 1))
                    {
                        result = false;
                        break;
                    }
                }
            }
            else if (node->type == "Or")
            {
                // どれかの入力が成り立つ。つながっていない入力は、偽(ほかの入力に、影響しない)。1 つ真なら、そこで止める
                for (const NodeId input : node->inputs)
                {
                    if (EvaluateTree(e, input, depth + 1))
                    {
                        result = true;
                        break;
                    }
                }
            }
            else if (node->type == "Not")
            {
                // 入力が成り立たない。**つながっていない入力のときは、偽**(「偽の否定 = 真」にすると、つなぐ前に、すぐ移ってしまう)
                if (!node->inputs.empty() && node->inputs.front() != 0)
                {
                    result = !EvaluateTree(e, node->inputs.front(), depth + 1);
                }
            }
            else
            {
                result = EvaluateCondition(*node, e.context, e.timeInState); // 葉の条件(状況を読む)
            }
            e.memo[slot].second = result ? 1 : 0;
            return result;
        }
    }

    bool EvaluateConditionTree(const Graph& graph, NodeId conditionId, const Context& context, float timeInState,
        std::vector<ConditionResult>* results)
    {
        TreeEvaluation evaluation{ graph, context, timeInState };
        const bool result = EvaluateTree(evaluation, conditionId, 0);
        const bool finalResult = evaluation.failed ? false : result; // 循環・深すぎるときは、途中の結果が、どうであっても、偽

        if (results != nullptr && !evaluation.failed)
        {
            for (const auto& [id, value] : evaluation.memo)
            {
                if (value < 0)
                {
                    continue; // 評価の途中(ここには、来ないはず)
                }
                const auto existing = std::find_if(results->begin(), results->end(), [id = id](const ConditionResult& r) { return r.node == id; });
                if (existing != results->end())
                {
                    existing->value = value == 1;
                }
                else
                {
                    results->push_back({ id, value == 1 });
                }
            }
        }
        return finalResult;
    }
}
