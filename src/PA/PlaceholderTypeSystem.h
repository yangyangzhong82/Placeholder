#pragma once

#include "Macros.h"
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "LRUCache.h"

namespace PA {

class PlaceholderTypeSystem {
public:
    using Caster = void* (*)(void*); // 函数指针, Derived* -> Base*

    struct InheritancePair {
        std::string derivedKey;
        std::string baseKey;
        Caster      caster;
    };

    PA_API PlaceholderTypeSystem();
    PA_API ~PlaceholderTypeSystem();

    PA_API std::size_t ensureTypeId(const std::string& typeKeyStr);

    PA_API void registerInheritanceByKeys(const std::string& derivedKey, const std::string& baseKey, Caster caster);

    PA_API void registerInheritanceByKeysBatch(const std::vector<InheritancePair>& pairs);

    PA_API void registerTypeAlias(const std::string& alias, const std::string& typeKeyStr);

    PA_API bool findUpcastChain(std::size_t fromTypeId, std::size_t toTypeId, std::vector<Caster>& outChain) const;
    
    PA_API std::string getTypeName(std::size_t typeId) const;

private:
    struct UpcastCacheEntry {
        bool                success;
        std::vector<Caster> chain;
    };

    // 类型系统映射：类型键字符串 <-> 类型ID
    std::unordered_map<std::string, std::size_t> mTypeKeyToId; // 类型键到ID的映射
    std::unordered_map<std::size_t, std::string> mIdToTypeKey; // ID到类型键的映射
    // [新] 类型ID到稳定别名的映射
    std::unordered_map<std::size_t, std::string> mIdToAlias;
    std::size_t mNextTypeId{1}; // 下一个可用的类型ID，0 保留为“无类型”

    // 继承图：派生类ID -> (基类ID -> 上行转换函数)
    std::unordered_map<std::size_t, std::unordered_map<std::size_t, Caster>> mUpcastEdges;

    // 上行链缓存：用于存储已计算过的上行转换路径，避免重复计算
    // 缓存键由 fromTypeId 和 toTypeId 组合而成
    mutable std::unordered_map<uint64_t, UpcastCacheEntry> mUpcastCache;

    mutable std::shared_mutex mMutex;
};

} // namespace PA
