#include "PlaceholderRegistry.h"
#include "PlaceholderManager.h" // For PlaceholderContext
#include <algorithm>
#include <queue>
#include <unordered_set>

namespace PA {

PlaceholderRegistry::PlaceholderRegistry(std::shared_ptr<PlaceholderTypeSystem> typeSystem) :
    mTypeSystem(std::move(typeSystem)) {}

PlaceholderRegistry::~PlaceholderRegistry() = default;

// ==== 占位符注册 ====

void PlaceholderRegistry::registerServerPlaceholder(
    const std::string&           pluginName,
    const std::string&           placeholder,
    ServerReplacer               replacer,
    std::optional<CacheDuration> cache_duration,
    CacheKeyStrategy             strategy
) {
    std::unique_lock lk(mMutex);
    mServerPlaceholders[pluginName][placeholder] = ServerReplacerEntry{std::move(replacer), cache_duration, strategy};
}

void PlaceholderRegistry::registerServerPlaceholderWithParams(
    const std::string&           pluginName,
    const std::string&           placeholder,
    ServerReplacerWithParams&&   replacer,
    std::optional<CacheDuration> cache_duration,
    CacheKeyStrategy             strategy
) {
    std::unique_lock lk(mMutex);
    mServerPlaceholders[pluginName][placeholder] =
        ServerReplacerEntry{ServerReplacerWithParams(std::move(replacer)), cache_duration, strategy};
}

void PlaceholderRegistry::registerPlaceholderForTypeId(
    const std::string&           pluginName,
    const std::string&           placeholder,
    std::size_t                  targetTypeId,
    AnyPtrReplacer               replacer,
    std::optional<CacheDuration> cache_duration,
    CacheKeyStrategy             strategy
) {
    std::unique_lock lk(mMutex);
    mContextPlaceholders[pluginName][placeholder].emplace(targetTypeId, TypedReplacer{
        targetTypeId,
        std::variant<AnyPtrReplacer, AnyPtrReplacerWithParams>(std::move(replacer)),
        cache_duration,
        strategy,
    });
}

void PlaceholderRegistry::registerPlaceholderForTypeId(
    const std::string&           pluginName,
    const std::string&           placeholder,
    std::size_t                  targetTypeId,
    AnyPtrReplacerWithParams&&   replacer,
    std::optional<CacheDuration> cache_duration,
    CacheKeyStrategy             strategy
) {
    std::unique_lock lk(mMutex);
    mContextPlaceholders[pluginName][placeholder].emplace(targetTypeId, TypedReplacer{
        targetTypeId,
        std::variant<AnyPtrReplacer, AnyPtrReplacerWithParams>(std::move(replacer)),
        cache_duration,
        strategy,
    });
}

void PlaceholderRegistry::registerAsyncServerPlaceholder(
    const std::string&           pluginName,
    const std::string&           placeholder,
    AsyncServerReplacer&&        replacer,
    std::optional<CacheDuration> cache_duration,
    CacheKeyStrategy             strategy
) {
    std::unique_lock lk(mMutex);
    mAsyncServerPlaceholders[pluginName][placeholder] =
        AsyncServerReplacerEntry{std::move(replacer), cache_duration, strategy};
}

void PlaceholderRegistry::registerAsyncServerPlaceholderWithParams(
    const std::string&              pluginName,
    const std::string&              placeholder,
    AsyncServerReplacerWithParams&& replacer,
    std::optional<CacheDuration>    cache_duration,
    CacheKeyStrategy                strategy
) {
    std::unique_lock lk(mMutex);
    mAsyncServerPlaceholders[pluginName][placeholder] =
        AsyncServerReplacerEntry{std::move(replacer), cache_duration, strategy};
}

void PlaceholderRegistry::registerAsyncPlaceholderForTypeId(
    const std::string&           pluginName,
    const std::string&           placeholder,
    std::size_t                  targetTypeId,
    AsyncAnyPtrReplacer&&        replacer,
    std::optional<CacheDuration> cache_duration,
    CacheKeyStrategy             strategy
) {
    std::unique_lock lk(mMutex);
    mAsyncContextPlaceholders[pluginName][placeholder].emplace(targetTypeId, AsyncTypedReplacer{
        targetTypeId,
        std::variant<AsyncAnyPtrReplacer, AsyncAnyPtrReplacerWithParams>(std::move(replacer)),
        cache_duration,
        strategy
    });
}

void PlaceholderRegistry::registerAsyncPlaceholderForTypeId(
    const std::string&              pluginName,
    const std::string&              placeholder,
    std::size_t                     targetTypeId,
    AsyncAnyPtrReplacerWithParams&& replacer,
    std::optional<CacheDuration>    cache_duration,
    CacheKeyStrategy                strategy
) {
    std::unique_lock lk(mMutex);
    mAsyncContextPlaceholders[pluginName][placeholder].emplace(targetTypeId, AsyncTypedReplacer{
        targetTypeId,
        std::variant<AsyncAnyPtrReplacer, AsyncAnyPtrReplacerWithParams>(std::move(replacer)),
        cache_duration,
        strategy
    });
}

void PlaceholderRegistry::registerRelationalPlaceholderForTypeId(
    const std::string&           pluginName,
    const std::string&           placeholder,
    std::size_t                  targetTypeId,
    std::size_t                  relationalTypeId,
    AnyPtrRelationalReplacer&&   replacer,
    std::optional<CacheDuration> cache_duration,
    CacheKeyStrategy             strategy
) {
    std::unique_lock lk(mMutex);
    mRelationalPlaceholders[pluginName][placeholder].push_back(RelationalTypedReplacer{
        targetTypeId,
        relationalTypeId,
        std::variant<AnyPtrRelationalReplacer, AnyPtrRelationalReplacerWithParams>(std::move(replacer)),
        cache_duration,
        strategy,
    });
}

void PlaceholderRegistry::registerRelationalPlaceholderForTypeId(
    const std::string&                   pluginName,
    const std::string&                   placeholder,
    std::size_t                          targetTypeId,
    std::size_t                          relationalTypeId,
    AnyPtrRelationalReplacerWithParams&& replacer,
    std::optional<CacheDuration>         cache_duration,
    CacheKeyStrategy                     strategy
) {
    std::unique_lock lk(mMutex);
    mRelationalPlaceholders[pluginName][placeholder].push_back(RelationalTypedReplacer{
        targetTypeId,
        relationalTypeId,
        std::variant<AnyPtrRelationalReplacer, AnyPtrRelationalReplacerWithParams>(std::move(replacer)),
        cache_duration,
        strategy,
    });
}

void PlaceholderRegistry::registerServerListPlaceholder(
    const std::string&           pluginName,
    const std::string&           placeholder,
    ServerListReplacer&&         replacer,
    std::optional<CacheDuration> cache_duration,
    CacheKeyStrategy             strategy
) {
    std::unique_lock lk(mMutex);
    mServerListPlaceholders[pluginName][placeholder] =
        ServerListReplacerEntry{std::move(replacer), cache_duration, strategy};
}

void PlaceholderRegistry::registerServerListPlaceholderWithParams(
    const std::string&             pluginName,
    const std::string&             placeholder,
    ServerListReplacerWithParams&& replacer,
    std::optional<CacheDuration>   cache_duration,
    CacheKeyStrategy               strategy
) {
    std::unique_lock lk(mMutex);
    mServerListPlaceholders[pluginName][placeholder] =
        ServerListReplacerEntry{std::move(replacer), cache_duration, strategy};
}

void PlaceholderRegistry::registerListPlaceholderForTypeId(
    const std::string&           pluginName,
    const std::string&           placeholder,
    std::size_t                  targetTypeId,
    AnyPtrListReplacer&&         replacer,
    std::optional<CacheDuration> cache_duration,
    CacheKeyStrategy             strategy
) {
    std::unique_lock lk(mMutex);
    mContextListPlaceholders[pluginName][placeholder].emplace(targetTypeId, TypedListReplacer{
        targetTypeId,
        std::variant<AnyPtrListReplacer, AnyPtrListReplacerWithParams>(std::move(replacer)),
        cache_duration,
        strategy,
    });
}

void PlaceholderRegistry::registerListPlaceholderForTypeId(
    const std::string&             pluginName,
    const std::string&             placeholder,
    std::size_t                    targetTypeId,
    AnyPtrListReplacerWithParams&& replacer,
    std::optional<CacheDuration>   cache_duration,
    CacheKeyStrategy               strategy
) {
    std::unique_lock lk(mMutex);
    mContextListPlaceholders[pluginName][placeholder].emplace(targetTypeId, TypedListReplacer{
        targetTypeId,
        std::variant<AnyPtrListReplacer, AnyPtrListReplacerWithParams>(std::move(replacer)),
        cache_duration,
        strategy,
    });
}

void PlaceholderRegistry::registerServerObjectListPlaceholder(
    const std::string&           pluginName,
    const std::string&           placeholder,
    ServerObjectListReplacer&&   replacer,
    std::optional<CacheDuration> cache_duration,
    CacheKeyStrategy             strategy
) {
    std::unique_lock lk(mMutex);
    mServerObjectListPlaceholders[pluginName][placeholder] =
        ServerObjectListReplacerEntry{std::move(replacer), cache_duration, strategy};
}

void PlaceholderRegistry::registerServerObjectListPlaceholderWithParams(
    const std::string&                   pluginName,
    const std::string&                   placeholder,
    ServerObjectListReplacerWithParams&& replacer,
    std::optional<CacheDuration>         cache_duration,
    CacheKeyStrategy                     strategy
) {
    std::unique_lock lk(mMutex);
    mServerObjectListPlaceholders[pluginName][placeholder] =
        ServerObjectListReplacerEntry{std::move(replacer), cache_duration, strategy};
}

void PlaceholderRegistry::registerObjectListPlaceholderForTypeId(
    const std::string&           pluginName,
    const std::string&           placeholder,
    std::size_t                  targetTypeId,
    AnyPtrObjectListReplacer&&   replacer,
    std::optional<CacheDuration> cache_duration,
    CacheKeyStrategy             strategy
) {
    std::unique_lock lk(mMutex);
    mContextObjectListPlaceholders[pluginName][placeholder].emplace(targetTypeId, TypedObjectListReplacer{
        targetTypeId,
        std::variant<AnyPtrObjectListReplacer, AnyPtrObjectListReplacerWithParams>(std::move(replacer)),
        cache_duration,
        strategy,
    });
}

void PlaceholderRegistry::registerObjectListPlaceholderForTypeId(
    const std::string&                   pluginName,
    const std::string&                   placeholder,
    std::size_t                          targetTypeId,
    AnyPtrObjectListReplacerWithParams&& replacer,
    std::optional<CacheDuration>         cache_duration,
    CacheKeyStrategy                     strategy
) {
    std::unique_lock lk(mMutex);
    mContextObjectListPlaceholders[pluginName][placeholder].emplace(targetTypeId, TypedObjectListReplacer{
        targetTypeId,
        std::variant<AnyPtrObjectListReplacer, AnyPtrObjectListReplacerWithParams>(std::move(replacer)),
        cache_duration,
        strategy,
    });
}

// ==== 注销 ====

void PlaceholderRegistry::unregisterPlaceholders(const std::string& pluginName) {
    std::unique_lock lk(mMutex);
    mServerPlaceholders.erase(pluginName);
    mContextPlaceholders.erase(pluginName);
    mRelationalPlaceholders.erase(pluginName);
    mServerObjectListPlaceholders.erase(pluginName);
    mContextObjectListPlaceholders.erase(pluginName);
    mServerListPlaceholders.erase(pluginName);
    mContextListPlaceholders.erase(pluginName);
}

void PlaceholderRegistry::unregisterAsyncPlaceholders(const std::string& pluginName) {
    std::unique_lock lk(mMutex);
    mAsyncServerPlaceholders.erase(pluginName);
    mAsyncContextPlaceholders.erase(pluginName);
}

// ==== 查询 ====

PlaceholderRegistry::AllPlaceholders PlaceholderRegistry::getAllPlaceholders() const {
    AllPlaceholders  result;
    std::shared_lock lk(mMutex);

    auto getTypeName = [&](std::size_t typeId) -> std::string { return mTypeSystem->getTypeName(typeId); };

    auto addPlaceholder =
        [&](const std::string& plugin, const std::string& name, PlaceholderCategory category, bool isAsync,
            const std::string& targetType = "", const std::string& relationalType = "") {
            auto& pluginPlaceholders = result.placeholders[plugin];
            auto  it                 = std::find_if(
                pluginPlaceholders.begin(),
                pluginPlaceholders.end(),
                [&](const PlaceholderInfo& p) {
                    return p.name == name && p.category == category && p.isAsync == isAsync;
                }
            );

            if (it != pluginPlaceholders.end()) {
                if (!targetType.empty() && it->targetType != targetType) {
                    if (it->overloads.empty() && !it->targetType.empty()) {
                        it->overloads.push_back(it->targetType);
                    }
                    it->overloads.push_back(targetType);
                }
            } else {
                pluginPlaceholders.push_back({name, category, isAsync, targetType, relationalType, {}});
            }
        };

    for (const auto& [plugin, phs] : mServerPlaceholders)
        for (const auto& [name, entry] : phs) addPlaceholder(plugin, name, PlaceholderCategory::Server, false);
    for (const auto& [plugin, phs] : mAsyncServerPlaceholders)
        for (const auto& [name, entry] : phs) addPlaceholder(plugin, name, PlaceholderCategory::Server, true);
    for (const auto& [plugin, phs] : mContextPlaceholders)
        for (const auto& [name, overloads] : phs)
            for (const auto& [typeId, entry] : overloads)
                addPlaceholder(plugin, name, PlaceholderCategory::Context, false, getTypeName(entry.targetTypeId));
    for (const auto& [plugin, phs] : mAsyncContextPlaceholders)
        for (const auto& [name, overloads] : phs)
            for (const auto& [typeId, entry] : overloads)
                addPlaceholder(plugin, name, PlaceholderCategory::Context, true, getTypeName(entry.targetTypeId));
    for (const auto& [plugin, phs] : mRelationalPlaceholders)
        for (const auto& [name, entries] : phs)
            for (const auto& entry : entries)
                addPlaceholder(
                    plugin,
                    name,
                    PlaceholderCategory::Relational,
                    false,
                    getTypeName(entry.targetTypeId),
                    getTypeName(entry.relationalTypeId)
                );
    for (const auto& [plugin, phs] : mServerListPlaceholders)
        for (const auto& [name, entry] : phs) addPlaceholder(plugin, name, PlaceholderCategory::List, false);
    for (const auto& [plugin, phs] : mContextListPlaceholders)
        for (const auto& [name, overloads] : phs)
            for (const auto& [typeId, entry] : overloads)
                addPlaceholder(plugin, name, PlaceholderCategory::List, false, getTypeName(entry.targetTypeId));
    for (const auto& [plugin, phs] : mServerObjectListPlaceholders)
        for (const auto& [name, entry] : phs) addPlaceholder(plugin, name, PlaceholderCategory::ObjectList, false);
    for (const auto& [plugin, phs] : mContextObjectListPlaceholders)
        for (const auto& [name, overloads] : phs)
            for (const auto& [typeId, entry] : overloads)
                addPlaceholder(
                    plugin,
                    name,
                    PlaceholderCategory::ObjectList,
                    false,
                    getTypeName(entry.targetTypeId)
                );

    return result;
}

bool PlaceholderRegistry::hasPlaceholder(
    const std::string&                pluginName,
    const std::string&                placeholderName,
    const std::optional<std::string>& typeKey
) const {
    std::shared_lock lk(mMutex);

    if (typeKey) {
        auto check = [&](const auto& container) {
            auto itPlugin = container.find(pluginName);
            if (itPlugin != container.end()) {
                auto itPh = itPlugin->second.find(placeholderName);
                if (itPh != itPlugin->second.end() && !itPh->second.empty()) {
                    return true;
                }
            }
            return false;
        };
        return check(mContextPlaceholders) || check(mAsyncContextPlaceholders);
    } else {
        auto check = [&](const auto& container) {
            auto itPlugin = container.find(pluginName);
            if (itPlugin != container.end()) {
                if (itPlugin->second.count(placeholderName)) {
                    return true;
                }
            }
            return false;
        };
        return check(mServerPlaceholders) || check(mAsyncServerPlaceholders);
    }
}

PlaceholderRegistry::ReplacerMatch PlaceholderRegistry::findBestReplacer(
    std::string_view          pluginName,
    std::string_view          placeholderName,
    const PlaceholderContext& ctx
) {
    std::string pluginNameStr(pluginName);
    std::string placeholderNameStr(placeholderName);

    // 1. 查找关系型占位符 (仅同步)
    if (ctx.ptr && ctx.typeId != 0 && ctx.relationalContext && ctx.relationalContext->ptr
        && ctx.relationalContext->typeId != 0) {
        std::shared_lock                     lk(mMutex);
        auto                                 plugin_it = mRelationalPlaceholders.find(pluginNameStr);
        if (plugin_it != mRelationalPlaceholders.end()) {
            auto ph_it = plugin_it->second.find(placeholderNameStr);
            if (ph_it != plugin_it->second.end()) {
                std::vector<std::pair<int, ReplacerMatch>> candidates;
                for (const auto& entry : ph_it->second) {
                    std::vector<Caster> chain;
                    std::vector<Caster> rel_chain;
                    bool main_ok = (entry.targetTypeId == ctx.typeId)
                                || mTypeSystem->findUpcastChain(ctx.typeId, entry.targetTypeId, chain);
                    bool rel_ok = (entry.relationalTypeId == ctx.relationalContext->typeId)
                                || mTypeSystem->findUpcastChain(
                                    ctx.relationalContext->typeId,
                                    entry.relationalTypeId,
                                    rel_chain
                                );

                    if (main_ok && rel_ok) {
                        candidates.emplace_back(
                            (int)(chain.size() + rel_chain.size()),
                            ReplacerMatch{
                                PlaceholderType::Relational,
                                entry.cacheDuration,
                                entry.strategy,
                                entry,
                                std::move(chain),
                                std::move(rel_chain)}
                        );
                    }
                }
                if (!candidates.empty()) {
                    auto best_it = std::min_element(
                        candidates.begin(),
                        candidates.end(),
                        [](const auto& a, const auto& b) { return a.first < b.first; }
                    );
                    return std::move(best_it->second);
                }
            }
        }
    }

    // 2. 查找上下文相关占位符 (同步/异步)
    if (ctx.ptr && ctx.typeId != 0) {
        std::vector<std::pair<int, ReplacerMatch>> candidates;
        std::shared_lock                           lk(mMutex);

        auto findCandidatesInMap =
            [&](const auto& container, PlaceholderType type) {
                auto plugin_it = container.find(pluginNameStr);
                if (plugin_it == container.end()) return;
                auto ph_it = plugin_it->second.find(placeholderNameStr);
                if (ph_it == plugin_it->second.end()) return;

                for (const auto& [targetTypeId, entry] : ph_it->second) {
                    std::vector<Caster> chain;
                    if (ctx.typeId == targetTypeId || mTypeSystem->findUpcastChain(ctx.typeId, targetTypeId, chain)) {
                        candidates.emplace_back(
                            (int)chain.size(),
                            ReplacerMatch{type, entry.cacheDuration, entry.strategy, entry, std::move(chain)}
                        );
                    }
                }
            };

        findCandidatesInMap(mContextPlaceholders, PlaceholderType::Context);
        findCandidatesInMap(mAsyncContextPlaceholders, PlaceholderType::AsyncContext);
        findCandidatesInMap(mContextListPlaceholders, PlaceholderType::ListContext);
        findCandidatesInMap(mContextObjectListPlaceholders, PlaceholderType::ObjectListContext);

        if (!candidates.empty()) {
            auto best_it = std::min_element(
                candidates.begin(),
                candidates.end(),
                [](const auto& a, const auto& b) { return a.first < b.first; }
            );
            return std::move(best_it->second);
        }
    }

    // 3. 查找服务器级占位符 (同步/异步)
    {
        std::shared_lock lk(mMutex);
        auto findInServerMap = [&](const auto& container, PlaceholderType type) -> std::optional<ReplacerMatch> {
            auto plugin_it = container.find(pluginNameStr);
            if (plugin_it != container.end()) {
                auto placeholder_it = plugin_it->second.find(placeholderNameStr);
                if (placeholder_it != plugin_it->second.end()) {
                    return ReplacerMatch{
                        type,
                        placeholder_it->second.cacheDuration,
                        placeholder_it->second.strategy,
                        placeholder_it->second};
                }
            }
            return std::nullopt;
        };

        // 异步优先
        if (auto res = findInServerMap(mAsyncServerPlaceholders, PlaceholderType::AsyncServer)) return *res;
        if (auto res = findInServerMap(mServerPlaceholders, PlaceholderType::Server)) return *res;
        if (auto res = findInServerMap(mServerListPlaceholders, PlaceholderType::ListServer)) return *res;
        if (auto res = findInServerMap(mServerObjectListPlaceholders, PlaceholderType::ObjectListServer)) return *res;
    }

    return {}; // 未找到任何匹配
}

} // namespace PA
