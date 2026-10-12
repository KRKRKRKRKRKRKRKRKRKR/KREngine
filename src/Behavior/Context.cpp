#include "KR/Behavior/Context.h"

namespace KR::Behavior
{
    void Context::SetFloat(const std::string& key, float value) { values_[key] = value; }
    void Context::SetInt(const std::string& key, int value) { values_[key] = value; }
    void Context::SetBool(const std::string& key, bool value) { values_[key] = value; }

    float Context::GetFloat(const std::string& key, float defaultValue) const
    {
        const auto found = values_.find(key);
        if (found == values_.end())
        {
            return defaultValue;
        }
        if (const float* value = std::get_if<float>(&found->second))
        {
            return *value;
        }
        if (const int* value = std::get_if<int>(&found->second))
        {
            return static_cast<float>(*value);
        }
        return defaultValue;
    }

    int Context::GetInt(const std::string& key, int defaultValue) const
    {
        const auto found = values_.find(key);
        if (found != values_.end())
        {
            if (const int* value = std::get_if<int>(&found->second))
            {
                return *value;
            }
        }
        return defaultValue;
    }

    bool Context::GetBool(const std::string& key, bool defaultValue) const
    {
        const auto found = values_.find(key);
        if (found != values_.end())
        {
            if (const bool* value = std::get_if<bool>(&found->second))
            {
                return *value;
            }
        }
        return defaultValue;
    }

    bool Context::Has(const std::string& key) const
    {
        return values_.find(key) != values_.end();
    }

    void Context::Clear()
    {
        values_.clear();
    }
}
