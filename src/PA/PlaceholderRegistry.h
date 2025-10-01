#pragma once

#include "Macros.h"
#include "PlaceholderTypeSystem.h"
#include "Utils.h"
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace PA {

struct PlaceholderContext; // 前向声明

// 占位符注册表，负责管理所有类型的占位符
class PlaceholderRegistry {
public:
    // 从 PlaceholderManager 引入的通用 using 声明
    using ServerReplacer = std::function<std::string()>;
    using ServerReplacerWithParams = std::function<std::string(const Utils::ParsedParams& params)>;
    using CacheDuration = std::chrono::steady_clock::duration;
    using AsyncServerReplacer = std::function<std::future<std::string>()>;
    using AsyncServerReplacerWithParams = std::function<std::future<std::string>(const Utils::ParsedParams& params)>;
    using AsyncAnyPtrReplacer = std::function<std::future<std::string>(void*)>;
    using AsyncAnyPtrReplacerWithParams =
        std::function<std::future<std::string>(void*, const Utils::ParsedParams& params)>;
    using AnyPtrRelationalReplacer = std::function<std::string(void*, void*)>;
    using AnyPtrRelationalReplacerWithParams =
        std::function<std::string(void*, void*, const Utils::ParsedParams& params)>;
    using AnyPtrReplacer = std::function<std::string(void*)>;
    using AnyPtrReplacerWithParams = std::function<std::string(void*, const Utils::ParsedParams& params)>;
    using ServerListReplacer = std::function<std::vector<std::string>()>;
    using ServerListReplacerWithParams = std::function<std::vector<std::string>(const Utils::ParsedParams& params)>;
    using AnyPtrListReplacer = std::function<std::vector<std::string>(void*)>;
    using AnyPtrListReplacerWithParams =
        std::function<std::vector<std::string>(void*, const Utils::ParsedParams& params)>;
    using ServerObjectListReplacer = std::function<std::vector<PlaceholderContext>()>;
    using ServerObjectListReplacerWithParams =
        std::function<std::vector<PlaceholderContext>(const Utils::ParsedParams& params)>;
    using AnyPtrObjectListReplacer = std::function<std::vector<PlaceholderContext>(void*)>;
    using AnyPtrObjectListReplacerWithParams =
        std::function<std::vector<PlaceholderContext>(void*, const Utils::ParsedParams& params)>;
    using Caster = PlaceholderTypeSystem::Caster;

    // 缓存键生成策略
    enum class CacheKeyStrategy {
        Default,    // 默认策略，包含上下文信息
        ServerOnly, // 仅服务器级别，不包含上下文
    };

    // 占位符分类
    enum class PlaceholderCategory {
        Server,      // 服务器级别
        Context,     // 上下文相关
        Relational,  // 关系型
        List,        // 列表
        ObjectList,  // 对象列表
    };
    // 占位符信息结构体，用于查询
    struct PlaceholderInfo {
        std::string              name;           // 占位符名称
        PlaceholderCategory      category;       // 分类
        bool                     isAsync{false}; // 是否异步
        std::string              targetType;     // 目标类型名
        std::string              relationalType; // 关系类型名
        std::vector<std::string> overloads;      // 重载类型列表
    };

    // 所有占位符的集合
    struct AllPlaceholders {
        std::unordered_map<std::string, std::vector<PlaceholderInfo>> placeholders;
    };

    // 占位符类型枚举
    enum class PlaceholderType {
        None,              // 无
        Server,            // 服务器
        Context,           // 上下文
        Relational,        // 关系型
        ListServer,        // 服务器列表
        ListContext,       // 上下文列表
        ObjectListServer,  // 服务器对象列表
        ObjectListContext, // 上下文对象列表
        AsyncServer,       // 异步服务器
        AsyncContext,      // 异步上下文
        SyncFallback       // 同步回退
    };

    // 服务器替换器条目
    struct ServerReplacerEntry {
        std::variant<ServerReplacer, ServerReplacerWithParams> fn;              // 函数
        std::optional<CacheDuration>                           cacheDuration;   // 缓存时长
        CacheKeyStrategy                                       strategy;        // 缓存策略
    };
    // 服务器列表替换器条目
    struct ServerListReplacerEntry {
        std::variant<ServerListReplacer, ServerListReplacerWithParams> fn;
        std::optional<CacheDuration>                                   cacheDuration;
        CacheKeyStrategy                                               strategy;
    };
    // 服务器对象列表替换器条目
    struct ServerObjectListReplacerEntry {
        std::variant<ServerObjectListReplacer, ServerObjectListReplacerWithParams> fn;
        std::optional<CacheDuration>                                               cacheDuration;
        CacheKeyStrategy                                                           strategy;
    };
    // 异步服务器替换器条目
    struct AsyncServerReplacerEntry {
        std::variant<AsyncServerReplacer, AsyncServerReplacerWithParams> fn;
        std::optional<CacheDuration>                                     cacheDuration;
        CacheKeyStrategy                                                 strategy;
    };
    // 带类型的替换器
    struct TypedReplacer {
        std::size_t                                            targetTypeId{0}; // 目标类型ID
        std::variant<AnyPtrReplacer, AnyPtrReplacerWithParams> fn;
        std::optional<CacheDuration>                           cacheDuration;
        CacheKeyStrategy                                       strategy;
    };
    // 带类型的列表替换器
    struct TypedListReplacer {
        std::size_t                                                  targetTypeId{0};
        std::variant<AnyPtrListReplacer, AnyPtrListReplacerWithParams> fn;
        std::optional<CacheDuration>                                 cacheDuration;
        CacheKeyStrategy                                             strategy;
    };
    // 带类型的对象列表替换器
    struct TypedObjectListReplacer {
        std::size_t                                                      targetTypeId{0};
        std::variant<AnyPtrObjectListReplacer, AnyPtrObjectListReplacerWithParams> fn;
        std::optional<CacheDuration>                                     cacheDuration;
        CacheKeyStrategy                                                 strategy;
    };
    // 异步带类型的替换器
    struct AsyncTypedReplacer {
        std::size_t                                                      targetTypeId{0};
        std::variant<AsyncAnyPtrReplacer, AsyncAnyPtrReplacerWithParams> fn;
        std::optional<CacheDuration>                                     cacheDuration;
        CacheKeyStrategy                                                 strategy;
    };
    // 关系型带类型的替换器
    struct RelationalTypedReplacer {
        std::size_t                                                        targetTypeId{0};     // 目标类型ID
        std::size_t                                                        relationalTypeId{0}; // 关系类型ID
        std::variant<AnyPtrRelationalReplacer, AnyPtrRelationalReplacerWithParams> fn;
        std::optional<CacheDuration>                                       cacheDuration;
        CacheKeyStrategy                                                   strategy;
    };

    // 替换器匹配结果
    struct ReplacerMatch {
        PlaceholderType              type{PlaceholderType::None}; // 匹配到的类型
        std::optional<CacheDuration> cacheDuration;               // 缓存时长
        CacheKeyStrategy             strategy{CacheKeyStrategy::Default}; // 缓存策略
        std::variant<
            std::monostate,
            ServerReplacerEntry,
            TypedReplacer,
            RelationalTypedReplacer,
            ServerListReplacerEntry,
            TypedListReplacer,
            ServerObjectListReplacerEntry,
            TypedObjectListReplacer,
            AsyncServerReplacerEntry,
            AsyncTypedReplacer>
            entry;                      // 匹配到的条目
        std::vector<Caster>          chain;           // 主对象的上行转换链
        std::vector<Caster>          relationalChain; // 关系对象的上行转换链
    };

    PA_API explicit PlaceholderRegistry(std::shared_ptr<PlaceholderTypeSystem> typeSystem);
    PA_API ~PlaceholderRegistry();

    // 注册服务器级占位符
    PA_API void registerServerPlaceholder(const std::string& pluginName, const std::string& placeholder,
                                          ServerReplacer replacer, std::optional<CacheDuration> cache_duration,
                                          CacheKeyStrategy strategy);

    // 注册带参数的服务器级占位符
    PA_API void registerServerPlaceholderWithParams(const std::string& pluginName, const std::string& placeholder,
                                                    ServerReplacerWithParams&& replacer,
                                                    std::optional<CacheDuration> cache_duration, CacheKeyStrategy strategy);

    // 注册异步服务器级占位符
    PA_API void registerAsyncServerPlaceholder(const std::string& pluginName, const std::string& placeholder,
                                               AsyncServerReplacer&& replacer,
                                               std::optional<CacheDuration> cache_duration, CacheKeyStrategy strategy);

    // 注册带参数的异步服务器级占位符
    PA_API void registerAsyncServerPlaceholderWithParams(const std::string& pluginName, const std::string& placeholder,
                                                         AsyncServerReplacerWithParams&& replacer,
                                                         std::optional<CacheDuration> cache_duration,
                                                         CacheKeyStrategy strategy);

    // 注册上下文相关的占位符
    PA_API void registerPlaceholderForTypeId(const std::string& pluginName, const std::string& placeholder,
                                             std::size_t targetTypeId, AnyPtrReplacer replacer,
                                             std::optional<CacheDuration> cache_duration, CacheKeyStrategy strategy);

    // 注册带参数的上下文相关的占位符
    PA_API void registerPlaceholderForTypeId(const std::string& pluginName, const std::string& placeholder,
                                             std::size_t targetTypeId, AnyPtrReplacerWithParams&& replacer,
                                             std::optional<CacheDuration> cache_duration, CacheKeyStrategy strategy);

    // 注册异步上下文相关的占位符
    PA_API void registerAsyncPlaceholderForTypeId(const std::string& pluginName, const std::string& placeholder,
                                                  std::size_t targetTypeId, AsyncAnyPtrReplacer&& replacer,
                                                  std::optional<CacheDuration> cache_duration,
                                                  CacheKeyStrategy strategy);

    // 注册带参数的异步上下文相关的占位符
    PA_API void registerAsyncPlaceholderForTypeId(const std::string& pluginName, const std::string& placeholder,
                                                  std::size_t targetTypeId, AsyncAnyPtrReplacerWithParams&& replacer,
                                                  std::optional<CacheDuration> cache_duration,
                                                  CacheKeyStrategy strategy);

    // 注册关系型占位符
    PA_API void registerRelationalPlaceholderForTypeId(const std::string& pluginName, const std::string& placeholder,
                                                       std::size_t targetTypeId, std::size_t relationalTypeId,
                                                       AnyPtrRelationalReplacer&& replacer,
                                                       std::optional<CacheDuration> cache_duration,
                                                       CacheKeyStrategy strategy);

    // 注册带参数的关系型占位符
    PA_API void registerRelationalPlaceholderForTypeId(const std::string& pluginName, const std::string& placeholder,
                                                       std::size_t targetTypeId, std::size_t relationalTypeId,
                                                       AnyPtrRelationalReplacerWithParams&& replacer,
                                                       std::optional<CacheDuration> cache_duration,
                                                       CacheKeyStrategy strategy);

    // 注册服务器级列表占位符
    PA_API void registerServerListPlaceholder(const std::string& pluginName, const std::string& placeholder,
                                              ServerListReplacer&& replacer,
                                              std::optional<CacheDuration> cache_duration, CacheKeyStrategy strategy);

    // 注册带参数的服务器级列表占位符
    PA_API void registerServerListPlaceholderWithParams(const std::string& pluginName, const std::string& placeholder,
                                                        ServerListReplacerWithParams&& replacer,
                                                        std::optional<CacheDuration> cache_duration,
                                                        CacheKeyStrategy strategy);

    // 注册上下文相关的列表占位符
    PA_API void registerListPlaceholderForTypeId(const std::string& pluginName, const std::string& placeholder,
                                                 std::size_t targetTypeId, AnyPtrListReplacer&& replacer,
                                                 std::optional<CacheDuration> cache_duration,
                                                 CacheKeyStrategy strategy);

    // 注册带参数的上下文相关的列表占位符
    PA_API void registerListPlaceholderForTypeId(const std::string& pluginName, const std::string& placeholder,
                                                 std::size_t targetTypeId, AnyPtrListReplacerWithParams&& replacer,
                                                 std::optional<CacheDuration> cache_duration,
                                                 CacheKeyStrategy strategy);

    // 注册服务器级对象列表占位符
    PA_API void registerServerObjectListPlaceholder(const std::string& pluginName, const std::string& placeholder,
                                                    ServerObjectListReplacer&& replacer,
                                                    std::optional<CacheDuration> cache_duration,
                                                    CacheKeyStrategy strategy);

    // 注册带参数的服务器级对象列表占位符
    PA_API void registerServerObjectListPlaceholderWithParams(const std::string& pluginName,
                                                              const std::string& placeholder,
                                                              ServerObjectListReplacerWithParams&& replacer,
                                                              std::optional<CacheDuration> cache_duration,
                                                              CacheKeyStrategy strategy);

    // 注册上下文相关的对象列表占位符
    PA_API void registerObjectListPlaceholderForTypeId(const std::string& pluginName, const std::string& placeholder,
                                                       std::size_t targetTypeId, AnyPtrObjectListReplacer&& replacer,
                                                       std::optional<CacheDuration> cache_duration,
                                                       CacheKeyStrategy strategy);

    // 注册带参数的上下文相关的对象列表占位符
    PA_API void registerObjectListPlaceholderForTypeId(const std::string& pluginName, const std::string& placeholder,
                                                       std::size_t targetTypeId,
                                                       AnyPtrObjectListReplacerWithParams&& replacer,
                                                       std::optional<CacheDuration> cache_duration,
                                                       CacheKeyStrategy strategy);

    // 注销指定插件的所有同步占位符
    PA_API void unregisterPlaceholders(const std::string& pluginName);
    // 注销指定插件的所有异步占位符
    PA_API void unregisterAsyncPlaceholders(const std::string& pluginName);

    // 获取所有已注册的占位符信息
    PA_API AllPlaceholders getAllPlaceholders() const;

    // 检查是否存在指定的占位符
    PA_API bool hasPlaceholder(const std::string& pluginName, const std::string& placeholderName,
                               const std::optional<std::string>& typeKey = std::nullopt) const;

    // 查找最匹配的替换器
    PA_API ReplacerMatch findBestReplacer(std::string_view pluginName, std::string_view placeholderName,
                                          const PlaceholderContext& ctx);

private:
    std::shared_ptr<PlaceholderTypeSystem> mTypeSystem;

    // 存储各种占位符的容器
    std::unordered_map<std::string, std::unordered_map<std::string, ServerReplacerEntry>> mServerPlaceholders;
    std::unordered_map<std::string, std::unordered_map<std::string, AsyncServerReplacerEntry>>
        mAsyncServerPlaceholders;
    std::unordered_map<std::string,
                       std::unordered_map<std::string, std::unordered_multimap<std::size_t, TypedReplacer>>>
        mContextPlaceholders;
    std::unordered_map<std::string,
                       std::unordered_map<std::string, std::unordered_multimap<std::size_t, AsyncTypedReplacer>>>
        mAsyncContextPlaceholders;
    std::unordered_map<std::string, std::unordered_map<std::string, ServerListReplacerEntry>>
        mServerListPlaceholders;
    std::unordered_map<std::string,
                       std::unordered_map<std::string, std::unordered_multimap<std::size_t, TypedListReplacer>>>
        mContextListPlaceholders;
    std::unordered_map<std::string, std::unordered_map<std::string, ServerObjectListReplacerEntry>>
        mServerObjectListPlaceholders;
    std::unordered_map<
        std::string,
        std::unordered_map<std::string, std::unordered_multimap<std::size_t, TypedObjectListReplacer>>>
        mContextObjectListPlaceholders;
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<RelationalTypedReplacer>>>
        mRelationalPlaceholders;

    mutable std::shared_mutex mMutex; // 用于保护容器访问的读写锁
};

} // namespace PA
