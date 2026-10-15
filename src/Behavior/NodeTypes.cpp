#include "KR/Behavior/NodeTypes.h"

#include <utility>

namespace KR::Behavior
{
    namespace
    {
        // 組み込みの状態(待機・巡回・プレイヤーへ移動・攻撃)と条件(半径の内・外・秒・攻撃の終わり)
        // 種類を足すときは、ここに 1 つ書くだけ(エディタの欄・メニューは、登録から自動で作られる)
        std::vector<NodeTypeInfo> MakeBuiltinTypes()
        {
            const auto state = [](const char* type, const char* displayName, std::vector<ParamSpec> params)
            {
                NodeTypeInfo info;
                info.kind = NodeKind::State;
                info.type = type;
                info.displayName = displayName;
                info.params = std::move(params);
                return info;
            };

            const auto condition = [](const char* type, const char* displayName, std::vector<ParamSpec> params)
            {
                NodeTypeInfo info;
                info.kind = NodeKind::Condition;
                info.type = type;
                info.displayName = displayName;
                info.params = std::move(params);
                return info;
            };

            // 論理の条件(かつ・または・でない):入力の条件の真偽を、組み合わせる。値は、なし。入力の数の範囲 = [minInputs, maxInputs]
            const auto logic = [](const char* type, const char* displayName, int minInputs, int maxInputs)
            {
                NodeTypeInfo info;
                info.kind = NodeKind::Condition;
                info.type = type;
                info.displayName = displayName;
                info.minInputs = minInputs;
                info.maxInputs = maxInputs;
                return info;
            };

            // 距離の条件の値。横(X)だけの距離が基本。includeVertical が true のときだけ、縦(keyY)も使う
            const auto distanceParams = [](float radius)
            {
                return std::vector<ParamSpec>{
                    {"key", "横の距離のキー", std::string("distanceToPlayerX")},
                    {"radius", "半径", radius, 0.0f, 100.0f},
                    {"includeVertical", "縦も含める", false},
                    {"keyY", "縦の距離のキー", std::string("distanceToPlayerY"), 0.0f, 0.0f, "includeVertical"},
                };
            };

            return {
                state("Wait", "待機", {}),
                state("Patrol", "巡回(左右に歩き、壁で反転)", {
                    {"speed", "速さ", 1.5f, 0.0f, 20.0f},
                }),
                state("MoveToPlayer", "プレイヤーへ移動", {
                    {"speed", "速さ", 2.5f, 0.0f, 20.0f},
                }),
                state("Attack", "攻撃", {
                    {"boxWidth", "攻撃の箱の幅", 1.0f, 0.0f, 20.0f},
                    {"boxHeight", "攻撃の箱の高さ", 0.5f, 0.0f, 20.0f},
                    {"damage", "ダメージ", 1, 0.0f, 999.0f},
                    {"windupSeconds", "溜めの秒", 0.4f, 0.0f, 10.0f},
                    {"activeSeconds", "攻撃が出ている秒", 0.2f, 0.0f, 10.0f},
                    {"recoverySeconds", "硬直の秒", 0.4f, 0.0f, 10.0f},
                }),
                condition("PlayerWithin", "プレイヤーが半径以内", distanceParams(3.0f)),
                condition("PlayerOutside", "プレイヤーが半径の外", distanceParams(6.0f)),
                condition("TimeElapsed", "秒たった(状態に入ってから)", {
                    {"seconds", "秒", 1.0f, 0.0f, 600.0f},
                }),
                condition("AttackFinished", "攻撃が終わった", {
                    {"key", "終わりのキー", std::string("attackFinished")},
                }),
                logic("And", "かつ(すべて成り立つ)", 2, 16),
                logic("Or", "または(どれかが成り立つ)", 2, 16),
                logic("Not", "でない(成り立たない)", 1, 1),
            };
        }

        std::vector<NodeTypeInfo>& Registry()
        {
            static std::vector<NodeTypeInfo> registry = MakeBuiltinTypes();
            return registry;
        }
    }

    bool RegisterNodeType(NodeTypeInfo info)
    {
        if (info.type.empty() || FindNodeType(info.type) != nullptr)
        {
            return false;
        }
        Registry().push_back(std::move(info));
        return true;
    }

    const NodeTypeInfo* FindNodeType(const std::string& type)
    {
        for (const NodeTypeInfo& info : Registry())
        {
            if (info.type == type)
            {
                return &info;
            }
        }
        return nullptr;
    }

    const std::vector<NodeTypeInfo>& GetNodeTypes()
    {
        return Registry();
    }
}
