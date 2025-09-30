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

struct PlaceholderContext; // Forward declaration

class PlaceholderRegistry {
public:
    // Common using declarations from PlaceholderManager
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

    enum class CacheKeyStrategy {
        Default,
        ServerOnly,
    };

    enum class PlaceholderCategory {
        Server,
        Context,
        Relational,
        List,
        ObjectList,
    };
    struct PlaceholderInfo {
        std::string              name;
        PlaceholderCategory      category;
        bool                     isAsync{false};
        std::string              targetType;
        std::string              relationalType;
        std::vector<std::string> overloads;
    };

    struct AllPlaceholders {
        std::unordered_map<std::string, std::vector<PlaceholderInfo>> placeholders;
    };

    enum class PlaceholderType {
        None,
        Server,
        Context,
        Relational,
        ListServer,
        ListContext,
        ObjectListServer,
        ObjectListContext,
        AsyncServer,
        AsyncContext,
        SyncFallback
    };

    struct ServerReplacerEntry {
        std::variant<ServerReplacer, ServerReplacerWithParams> fn;
        std::optional<CacheDuration>                           cacheDuration;
        CacheKeyStrategy                                       strategy;
    };
    struct ServerListReplacerEntry {
        std::variant<ServerListReplacer, ServerListReplacerWithParams> fn;
        std::optional<CacheDuration>                                   cacheDuration;
        CacheKeyStrategy                                               strategy;
    };
    struct ServerObjectListReplacerEntry {
        std::variant<ServerObjectListReplacer, ServerObjectListReplacerWithParams> fn;
        std::optional<CacheDuration>                                               cacheDuration;
        CacheKeyStrategy                                                           strategy;
    };
    struct AsyncServerReplacerEntry {
        std::variant<AsyncServerReplacer, AsyncServerReplacerWithParams> fn;
        std::optional<CacheDuration>                                     cacheDuration;
        CacheKeyStrategy                                                 strategy;
    };
    struct TypedReplacer {
        std::size_t                                            targetTypeId{0};
        std::variant<AnyPtrReplacer, AnyPtrReplacerWithParams> fn;
        std::optional<CacheDuration>                           cacheDuration;
        CacheKeyStrategy                                       strategy;
    };
    struct TypedListReplacer {
        std::size_t                                                  targetTypeId{0};
        std::variant<AnyPtrListReplacer, AnyPtrListReplacerWithParams> fn;
        std::optional<CacheDuration>                                 cacheDuration;
        CacheKeyStrategy                                             strategy;
    };
    struct TypedObjectListReplacer {
        std::size_t                                                      targetTypeId{0};
        std::variant<AnyPtrObjectListReplacer, AnyPtrObjectListReplacerWithParams> fn;
        std::optional<CacheDuration>                                     cacheDuration;
        CacheKeyStrategy                                                 strategy;
    };
    struct AsyncTypedReplacer {
        std::size_t                                                      targetTypeId{0};
        std::variant<AsyncAnyPtrReplacer, AsyncAnyPtrReplacerWithParams> fn;
        std::optional<CacheDuration>                                     cacheDuration;
        CacheKeyStrategy                                                 strategy;
    };
    struct RelationalTypedReplacer {
        std::size_t                                                        targetTypeId{0};
        std::size_t                                                        relationalTypeId{0};
        std::variant<AnyPtrRelationalReplacer, AnyPtrRelationalReplacerWithParams> fn;
        std::optional<CacheDuration>                                       cacheDuration;
        CacheKeyStrategy                                                   strategy;
    };

    struct ReplacerMatch {
        PlaceholderType              type{PlaceholderType::None};
        std::optional<CacheDuration> cacheDuration;
        CacheKeyStrategy             strategy{CacheKeyStrategy::Default};
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
            entry;
        std::vector<Caster>          chain;
        std::vector<Caster>          relationalChain;
    };

    PA_API explicit PlaceholderRegistry(std::shared_ptr<PlaceholderTypeSystem> typeSystem);
    PA_API ~PlaceholderRegistry();

    PA_API void registerServerPlaceholder(const std::string& pluginName, const std::string& placeholder,
                                          ServerReplacer replacer, std::optional<CacheDuration> cache_duration,
                                          CacheKeyStrategy strategy);

    PA_API void registerServerPlaceholderWithParams(const std::string& pluginName, const std::string& placeholder,
                                                    ServerReplacerWithParams&& replacer,
                                                    std::optional<CacheDuration> cache_duration, CacheKeyStrategy strategy);

    PA_API void registerAsyncServerPlaceholder(const std::string& pluginName, const std::string& placeholder,
                                               AsyncServerReplacer&& replacer,
                                               std::optional<CacheDuration> cache_duration, CacheKeyStrategy strategy);

    PA_API void registerAsyncServerPlaceholderWithParams(const std::string& pluginName, const std::string& placeholder,
                                                         AsyncServerReplacerWithParams&& replacer,
                                                         std::optional<CacheDuration> cache_duration,
                                                         CacheKeyStrategy strategy);

    PA_API void registerPlaceholderForTypeId(const std::string& pluginName, const std::string& placeholder,
                                             std::size_t targetTypeId, AnyPtrReplacer replacer,
                                             std::optional<CacheDuration> cache_duration, CacheKeyStrategy strategy);

    PA_API void registerPlaceholderForTypeId(const std::string& pluginName, const std::string& placeholder,
                                             std::size_t targetTypeId, AnyPtrReplacerWithParams&& replacer,
                                             std::optional<CacheDuration> cache_duration, CacheKeyStrategy strategy);

    PA_API void registerAsyncPlaceholderForTypeId(const std::string& pluginName, const std::string& placeholder,
                                                  std::size_t targetTypeId, AsyncAnyPtrReplacer&& replacer,
                                                  std::optional<CacheDuration> cache_duration,
                                                  CacheKeyStrategy strategy);

    PA_API void registerAsyncPlaceholderForTypeId(const std::string& pluginName, const std::string& placeholder,
                                                  std::size_t targetTypeId, AsyncAnyPtrReplacerWithParams&& replacer,
                                                  std::optional<CacheDuration> cache_duration,
                                                  CacheKeyStrategy strategy);

    PA_API void registerRelationalPlaceholderForTypeId(const std::string& pluginName, const std::string& placeholder,
                                                       std::size_t targetTypeId, std::size_t relationalTypeId,
                                                       AnyPtrRelationalReplacer&& replacer,
                                                       std::optional<CacheDuration> cache_duration,
                                                       CacheKeyStrategy strategy);

    PA_API void registerRelationalPlaceholderForTypeId(const std::string& pluginName, const std::string& placeholder,
                                                       std::size_t targetTypeId, std::size_t relationalTypeId,
                                                       AnyPtrRelationalReplacerWithParams&& replacer,
                                                       std::optional<CacheDuration> cache_duration,
                                                       CacheKeyStrategy strategy);

    PA_API void registerServerListPlaceholder(const std::string& pluginName, const std::string& placeholder,
                                              ServerListReplacer&& replacer,
                                              std::optional<CacheDuration> cache_duration, CacheKeyStrategy strategy);

    PA_API void registerServerListPlaceholderWithParams(const std::string& pluginName, const std::string& placeholder,
                                                        ServerListReplacerWithParams&& replacer,
                                                        std::optional<CacheDuration> cache_duration,
                                                        CacheKeyStrategy strategy);

    PA_API void registerListPlaceholderForTypeId(const std::string& pluginName, const std::string& placeholder,
                                                 std::size_t targetTypeId, AnyPtrListReplacer&& replacer,
                                                 std::optional<CacheDuration> cache_duration,
                                                 CacheKeyStrategy strategy);

    PA_API void registerListPlaceholderForTypeId(const std::string& pluginName, const std::string& placeholder,
                                                 std::size_t targetTypeId, AnyPtrListReplacerWithParams&& replacer,
                                                 std::optional<CacheDuration> cache_duration,
                                                 CacheKeyStrategy strategy);

    PA_API void registerServerObjectListPlaceholder(const std::string& pluginName, const std::string& placeholder,
                                                    ServerObjectListReplacer&& replacer,
                                                    std::optional<CacheDuration> cache_duration,
                                                    CacheKeyStrategy strategy);

    PA_API void registerServerObjectListPlaceholderWithParams(const std::string& pluginName,
                                                              const std::string& placeholder,
                                                              ServerObjectListReplacerWithParams&& replacer,
                                                              std::optional<CacheDuration> cache_duration,
                                                              CacheKeyStrategy strategy);

    PA_API void registerObjectListPlaceholderForTypeId(const std::string& pluginName, const std::string& placeholder,
                                                       std::size_t targetTypeId, AnyPtrObjectListReplacer&& replacer,
                                                       std::optional<CacheDuration> cache_duration,
                                                       CacheKeyStrategy strategy);

    PA_API void registerObjectListPlaceholderForTypeId(const std::string& pluginName, const std::string& placeholder,
                                                       std::size_t targetTypeId,
                                                       AnyPtrObjectListReplacerWithParams&& replacer,
                                                       std::optional<CacheDuration> cache_duration,
                                                       CacheKeyStrategy strategy);

    PA_API void unregisterPlaceholders(const std::string& pluginName);
    PA_API void unregisterAsyncPlaceholders(const std::string& pluginName);

    PA_API AllPlaceholders getAllPlaceholders() const;

    PA_API bool hasPlaceholder(const std::string& pluginName, const std::string& placeholderName,
                               const std::optional<std::string>& typeKey = std::nullopt) const;

    PA_API ReplacerMatch findBestReplacer(std::string_view pluginName, std::string_view placeholderName,
                                          const PlaceholderContext& ctx);

private:
    std::shared_ptr<PlaceholderTypeSystem> mTypeSystem;

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

    mutable std::shared_mutex mMutex;
};

} // namespace PA
