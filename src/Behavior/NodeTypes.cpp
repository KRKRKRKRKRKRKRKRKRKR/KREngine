#include "KR/Behavior/NodeTypes.h"

#include <utility>

namespace KR::Behavior
{
    namespace
    {
        std::vector<NodeTypeInfo>& Registry()
        {
            static std::vector<NodeTypeInfo> registry;
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
