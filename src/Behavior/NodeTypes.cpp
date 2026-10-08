#include "KR/Behavior/NodeTypes.h"

#include <utility>

namespace KR::Behavior
{
    namespace
    {
        // 組み込みの状態(待機・巡回・プレイヤーへ移動・攻撃)
        // 種類を足すときは、ここに 1 つ書くだけ(エディタの欄・メニューは、登録から自動で作られる)
        std::vector<NodeTypeInfo> MakeBuiltinStates()
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
            };
        }

        std::vector<NodeTypeInfo>& Registry()
        {
            static std::vector<NodeTypeInfo> registry = MakeBuiltinStates();
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
