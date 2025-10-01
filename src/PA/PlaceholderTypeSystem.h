#pragma once

#include "Macros.h"
#include "LRUCache.h"
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace PA {

/**
 * @brief 占位符类型系统，负责管理类型信息和继承关系。
 */
class PlaceholderTypeSystem {
public:
    /**
     * @brief 类型转换函数指针，用于将派生类指针转换为基类指针。
     * @param void* 指向派生类对象的指针。
     * @return void* 指向基类对象的指针。
     */
    using Caster = void* (*)(void*);

    /**
     * @brief 继承关系对，用于批量注册。
     */
    struct InheritancePair {
        std::string derivedKey; // 派生类的类型键。
        std::string baseKey;    // 基类的类型键。
        Caster      caster;     // 转换函数。
    };

    PA_API PlaceholderTypeSystem();
    PA_API ~PlaceholderTypeSystem();

    /**
     * @brief 确保给定类型键存在一个唯一的类型ID，如果不存在则创建。
     * @param typeKeyStr 类型的字符串键。
     * @return std::size_t 类型的唯一ID。
     */
    PA_API std::size_t ensureTypeId(const std::string& typeKeyStr);

    /**
     * @brief 通过类型键注册单个继承关系。
     * @param derivedKey 派生类的类型键。
     * @param baseKey 基类的类型键。
     * @param caster 从派生类到基类的转换函数。
     */
    PA_API void registerInheritanceByKeys(const std::string& derivedKey, const std::string& baseKey, Caster caster);

    /**
     * @brief 批量注册继承关系。
     * @param pairs 包含多个继承关系对的向量。
     */
    PA_API void registerInheritanceByKeysBatch(const std::vector<InheritancePair>& pairs);

    /**
     * @brief 为一个已存在的类型注册一个别名。
     * @param alias 要注册的别名。
     * @param typeKeyStr 原始类型的字符串键。
     */
    PA_API void registerTypeAlias(const std::string& alias, const std::string& typeKeyStr);

    /**
     * @brief 查找从一个类型到另一个类型的上行转换链（继承路径）。
     * @param fromTypeId 起始类型的ID。
     * @param toTypeId 目标类型的ID。
     * @param outChain 如果找到路径，则用转换函数链填充此向量。
     * @return bool 如果找到上行转换路径，则为 true，否则为 false。
     */
    PA_API bool findUpcastChain(std::size_t fromTypeId, std::size_t toTypeId, std::vector<Caster>& outChain) const;

    /**
     * @brief 获取与类型ID关联的名称（优先返回别名）。
     * @param typeId 类型的ID。
     * @return std::string 类型的名称。
     */
    PA_API std::string getTypeName(std::size_t typeId) const;

private:
    /**
     * @brief 上行转换缓存条目，存储查找结果以提高性能。
     */
    struct UpcastCacheEntry {
        bool                success; // 是否成功找到路径。
        std::vector<Caster> chain;   // 转换函数链。
    };

    // 类型系统映射：类型键字符串 <-> 类型ID
    std::unordered_map<std::string, std::size_t> mTypeKeyToId; // 类型键到ID的映射。
    std::unordered_map<std::size_t, std::string> mIdToTypeKey; // ID到类型键的映射。
    std::unordered_map<std::size_t, std::string> mIdToAlias;   // 类型ID到稳定别名的映射。
    std::size_t mNextTypeId{1}; // 下一个可用的类型ID，0 保留为“无类型”。

    // 继承图：派生类ID -> (基类ID -> 上行转换函数)
    std::unordered_map<std::size_t, std::unordered_map<std::size_t, Caster>> mUpcastEdges;

    // 上行链缓存：用于存储已计算过的上行转换路径，避免重复计算。
    // 缓存键由 fromTypeId 和 toTypeId 组合而成。
    mutable std::unordered_map<uint64_t, UpcastCacheEntry> mUpcastCache;

    mutable std::shared_mutex mMutex; // 用于保护内部数据结构的读写锁。
};

} // namespace PA
