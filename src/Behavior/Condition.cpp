#include "Condition.h"

#include <cmath>

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
            if (!context.Has(keyX))
            {
                return Zone::Unknown;
            }
            float distance = context.GetFloat(keyX);

            if (BoolParam(node, "includeVertical", false))
            {
                const std::string keyY = StringParam(node, "keyY", "distanceToPlayerY");
                if (!context.Has(keyY))
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
        (void)timeInState; // B2-3 で使う
        if (node.type == "PlayerWithin")
        {
            return JudgeZone(node, context) == Zone::Inside;
        }
        if (node.type == "PlayerOutside")
        {
            return JudgeZone(node, context) == Zone::Outside;
        }
        return false; // TimeElapsed・AttackFinished は B2-3
    }
}
