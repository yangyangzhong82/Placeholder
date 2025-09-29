#include "PlaceholderManager.h"
#include "ThreadPool.h"
#include "Utils.h"

#include "ll/api/service/Bedrock.h"
#include "mc/network/ServerNetworkHandler.h"
#include "mc/server/ServerPlayer.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/Level.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <future>
#include <iomanip>
#include <mutex>
#include <optional>
#include <queue>
#include <random>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>


#include "PA/Config/ConfigManager.h" // 引入 ConfigManager


namespace PA {

// --- 新：模板编译系统实现 ---

// 为支持 unique_ptr 的移动语义
CompiledTemplate::CompiledTemplate()                                       = default;
CompiledTemplate::~CompiledTemplate()                                      = default;
CompiledTemplate::CompiledTemplate(CompiledTemplate&&) noexcept            = default;
CompiledTemplate& CompiledTemplate::operator=(CompiledTemplate&&) noexcept = default;


// 单例
PlaceholderManager& PlaceholderManager::getInstance() {
    static PlaceholderManager instance;
    return instance;
}

PlaceholderManager::PlaceholderManager() :
    mGlobalCache(ConfigManager::getInstance().get().globalCacheSize),
    mCompileCache(ConfigManager::getInstance().get().globalCacheSize),
    mParamsCache(ConfigManager::getInstance().get().globalCacheSize) {
    unsigned int hardwareConcurrency = std::thread::hardware_concurrency();
    if (hardwareConcurrency == 0) {
        hardwareConcurrency = 2; // 硬件并发未知时的默认值
    }

    mCombinerThreadPool = std::make_unique<ThreadPool>(1); // 合并线程池只需要一个线程

    const auto&  config        = ConfigManager::getInstance().get();
    int          asyncPoolSize = config.asyncThreadPoolSize;
    if (asyncPoolSize <= 0) {
        asyncPoolSize = hardwareConcurrency;
    }
    mAsyncThreadPool =
        std::make_unique<ThreadPool>(asyncPoolSize, static_cast<size_t>(config.asyncThreadPoolQueueSize));


    // 注册一个回调，当配置重新加载时，更新缓存大小
    ConfigManager::getInstance().onReload([this](const Config& newConfig) {
        mGlobalCache.setCapacity(newConfig.globalCacheSize);
        mCompileCache.setCapacity(newConfig.globalCacheSize);
        mParamsCache.setCapacity(newConfig.globalCacheSize);
        // 注意：线程池大小在运行时不便更改，因此这里不处理 asyncThreadPoolSize 的热重载
    });
}

// ==== 类型系统实现 ====

std::size_t PlaceholderManager::ensureTypeId(const std::string& typeKeyStr) {
    std::unique_lock lk(mMutex);
    auto             it = mTypeKeyToId.find(typeKeyStr);
    if (it != mTypeKeyToId.end()) return it->second;
    auto id                  = mNextTypeId++;
    mTypeKeyToId[typeKeyStr] = id;
    mIdToTypeKey[id]         = typeKeyStr;
    return id;
}

void PlaceholderManager::registerInheritanceByKeys(
    const std::string& derivedKey,
    const std::string& baseKey,
    Caster             caster
) {
    std::unique_lock lk(mMutex);
    auto             d = ensureTypeId(derivedKey);
    auto             b = ensureTypeId(baseKey);
    mUpcastEdges[d][b] = caster;
    mUpcastCache.clear(); // 继承关系变更，清空缓存
}

void PlaceholderManager::registerInheritanceByKeysBatch(const std::vector<InheritancePair>& pairs) {
    std::unique_lock lk(mMutex);
    for (const auto& pair : pairs) {
        auto d             = ensureTypeId(pair.derivedKey);
        auto b             = ensureTypeId(pair.baseKey);
        mUpcastEdges[d][b] = pair.caster;
    }
    mUpcastCache.clear(); // 继承关系变更，清空缓存
}

void PlaceholderManager::registerTypeAlias(const std::string& alias, const std::string& typeKeyStr) {
    auto             id = ensureTypeId(typeKeyStr);
    std::unique_lock lk(mMutex);
    mIdToAlias[id] = alias;
}

// BFS 查询 from -> to 的“最短上行路径”
bool PlaceholderManager::findUpcastChain(std::size_t fromTypeId, std::size_t toTypeId, std::vector<Caster>& outChain)
    const {
    outChain.clear();
    if (fromTypeId == 0 || toTypeId == 0) return false;
    if (fromTypeId == toTypeId) return true; // 空链表示已是同型

    uint64_t cacheKey = (static_cast<uint64_t>(fromTypeId) << 32) | toTypeId;

    // 1. 检查缓存
    {
        std::shared_lock lk(mMutex);
        auto             it = mUpcastCache.find(cacheKey);
        if (it != mUpcastCache.end()) {
            outChain = it->second.chain;
            return it->second.success;
        }
    }

    // 2. 缓存未命中，执行 BFS
    std::vector<Caster> resultChain;
    bool                success = false;
    {
        std::shared_lock                                                lk(mMutex);
        std::unordered_map<std::size_t, std::pair<std::size_t, Caster>> prev;
        std::queue<std::size_t>                                         q;

        prev[fromTypeId] = {0, nullptr};
        q.push(fromTypeId);

        while (!q.empty()) {
            auto cur = q.front();
            q.pop();

            auto it = mUpcastEdges.find(cur);
            if (it == mUpcastEdges.end()) continue;

            for (auto& [nxt, caster] : it->second) {
                if (prev.find(nxt) != prev.end()) continue;
                prev[nxt] = {cur, caster};
                if (nxt == toTypeId) {
                    // 回溯构造链
                    std::size_t x = nxt;
                    while (x != fromTypeId) {
                        auto [p, c] = prev[x];
                        resultChain.push_back(c);
                        x = p;
                    }
                    std::reverse(resultChain.begin(), resultChain.end());
                    success = true;
                    goto bfs_end; // 找到路径，跳出循环
                }
                q.push(nxt);
            }
        }
    }
bfs_end:

    // 3. 结果写入缓存
    {
        std::unique_lock lk(mMutex);
        mUpcastCache[cacheKey] = {success, resultChain};
    }

    outChain = std::move(resultChain);
    return success;
}

// ==== 占位符注册 ====

void PlaceholderManager::registerServerPlaceholder(
    const std::string&           pluginName,
    const std::string&           placeholder,
    ServerReplacer               replacer,
    std::optional<CacheDuration> cache_duration,
    CacheKeyStrategy             strategy
) {
    std::unique_lock lk(mMutex);
    mServerPlaceholders[pluginName][placeholder] = ServerReplacerEntry{replacer, cache_duration, strategy};
}

void PlaceholderManager::registerServerPlaceholderWithParams(
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

void PlaceholderManager::registerPlaceholderForTypeId(
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

void PlaceholderManager::registerPlaceholderForTypeId(
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

void PlaceholderManager::registerPlaceholderForTypeKey(
    const std::string& pluginName,
    const std::string& placeholder,
    const std::string& typeKeyStr,
    AnyPtrReplacer     replacer
) {
    auto id = ensureTypeId(typeKeyStr);
    registerPlaceholderForTypeId(pluginName, placeholder, id, std::move(replacer));
}

void PlaceholderManager::registerPlaceholderForTypeKeyWithParams(
    const std::string&         pluginName,
    const std::string&         placeholder,
    const std::string&         typeKeyStr,
    AnyPtrReplacerWithParams&& replacer
) {
    auto id = ensureTypeId(typeKeyStr);
    registerPlaceholderForTypeId(pluginName, placeholder, id, std::move(replacer));
}


// --- 新：异步占位符注册 ---

void PlaceholderManager::registerAsyncServerPlaceholder(
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

void PlaceholderManager::registerAsyncServerPlaceholderWithParams(
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

void PlaceholderManager::registerAsyncPlaceholderForTypeId(
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

void PlaceholderManager::registerAsyncPlaceholderForTypeId(
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

void PlaceholderManager::registerAsyncPlaceholderForTypeKey(
    const std::string&    pluginName,
    const std::string&    placeholder,
    const std::string&    typeKeyStr,
    AsyncAnyPtrReplacer&& replacer
) {
    auto id = ensureTypeId(typeKeyStr);
    registerAsyncPlaceholderForTypeId(pluginName, placeholder, id, std::move(replacer));
}

void PlaceholderManager::registerAsyncPlaceholderForTypeKeyWithParams(
    const std::string&              pluginName,
    const std::string&              placeholder,
    const std::string&              typeKeyStr,
    AsyncAnyPtrReplacerWithParams&& replacer
) {
    auto id = ensureTypeId(typeKeyStr);
    registerAsyncPlaceholderForTypeId(pluginName, placeholder, id, std::move(replacer));
}

// ==== 注销 & 缓存清理 ====

void PlaceholderManager::clearCache() { mGlobalCache.clear(); }

void PlaceholderManager::clearCache(const std::string& pluginName) {
    mGlobalCache.remove_if([&pluginName](const std::string& key) {
        // Key formats:
        // - ServerOnly: #plugin:ph|params
        // - No context: plugin:ph|params
        // - Context:    ctx#type#plugin:ph|params
        // - Relational: ctx#type#relctx#reltype#plugin:ph|params
        // The plugin name is always located between the last '#' (or start) and the first ':' before '|'.

        auto pipePos = key.find('|');
        if (pipePos == std::string::npos) {
            return false; // Not a valid placeholder cache key.
        }

        std::string_view keyView = std::string_view(key).substr(0, pipePos);

        auto colonPos = keyView.rfind(':');
        if (colonPos == std::string::npos) {
            return false; // Invalid key format, no placeholder name.
        }

        auto lastHashPos = keyView.rfind('#');

        std::string_view keyPluginName;
        if (lastHashPos == std::string::npos) {
            // Case: "plugin:ph" (no context)
            keyPluginName = keyView.substr(0, colonPos);
        } else {
            // Case: "...#plugin:ph" (server, context, relational)
            keyPluginName = keyView.substr(lastHashPos + 1, colonPos - (lastHashPos + 1));
        }

        return keyPluginName == pluginName;
    });
}

void PlaceholderManager::unregisterPlaceholders(const std::string& pluginName) {
    std::unique_lock lk(mMutex);
    mServerPlaceholders.erase(pluginName);
    mContextPlaceholders.erase(pluginName);
    mRelationalPlaceholders.erase(pluginName);
    mServerObjectListPlaceholders.erase(pluginName);
    mContextObjectListPlaceholders.erase(pluginName);
    mUpcastCache.clear(); // 插件注销可能影响类型，为安全起见清空
}

void PlaceholderManager::unregisterAsyncPlaceholders(const std::string& pluginName) {
    std::unique_lock lk(mMutex);
    mAsyncServerPlaceholders.erase(pluginName);
    mAsyncContextPlaceholders.erase(pluginName);
    mServerListPlaceholders.erase(pluginName);
    mContextListPlaceholders.erase(pluginName);
}

PlaceholderManager::AllPlaceholders PlaceholderManager::getAllPlaceholders() const {
    AllPlaceholders  result;
    std::shared_lock lk(mMutex);

    auto getTypeName = [&](std::size_t typeId) -> std::string {
        if (typeId == 0) return "N/A";
        auto aliasIt = mIdToAlias.find(typeId);
        if (aliasIt != mIdToAlias.end()) {
            return aliasIt->second;
        }
        auto keyIt = mIdToTypeKey.find(typeId);
        if (keyIt != mIdToTypeKey.end()) {
            return keyIt->second;
        }
        return "UnknownTypeId(" + std::to_string(typeId) + ")";
    };

    // 辅助函数，用于添加或更新占位符信息
    auto addPlaceholder =
        [&](const std::string& plugin, const std::string& name, PlaceholderCategory category, bool isAsync,
            const std::string& targetType = "", const std::string& relationalType = "") {
            auto& pluginPlaceholders = result.placeholders[plugin];

            auto it = std::find_if(
                pluginPlaceholders.begin(),
                pluginPlaceholders.end(),
                [&](const PlaceholderInfo& p) {
                    return p.name == name && p.category == category && p.isAsync == isAsync;
                }
            );

            if (it != pluginPlaceholders.end()) {
                // 找到了一个具有相同名称、类别和异步状态的现有条目。
                // 这是一个具有不同目标类型的重载。
                if (!targetType.empty() && it->targetType != targetType) {
                    // 如果这是找到的第一个重载，则将初始 targetType 添加到 overloads 中。
                    if (it->overloads.empty() && !it->targetType.empty()) {
                        it->overloads.push_back(it->targetType);
                    }
                    it->overloads.push_back(targetType);
                }
            } else {
                // 添加新条目
                pluginPlaceholders.push_back({name, category, isAsync, targetType, relationalType, {}});
            }
        };

    // 1. Server Placeholders
    for (const auto& [plugin, phs] : mServerPlaceholders) {
        for (const auto& [name, entry] : phs) {
            addPlaceholder(plugin, name, PlaceholderCategory::Server, false);
        }
    }
    for (const auto& [plugin, phs] : mAsyncServerPlaceholders) {
        for (const auto& [name, entry] : phs) {
            addPlaceholder(plugin, name, PlaceholderCategory::Server, true);
        }
    }

    // 2. Context Placeholders
    for (const auto& [plugin, phs] : mContextPlaceholders) {
        for (const auto& [name, overloads] : phs) {
            for (const auto& [typeId, entry] : overloads) {
                addPlaceholder(plugin, name, PlaceholderCategory::Context, false, getTypeName(entry.targetTypeId));
            }
        }
    }
    for (const auto& [plugin, phs] : mAsyncContextPlaceholders) {
        for (const auto& [name, overloads] : phs) {
            for (const auto& [typeId, entry] : overloads) {
                addPlaceholder(plugin, name, PlaceholderCategory::Context, true, getTypeName(entry.targetTypeId));
            }
        }
    }

    // 3. Relational Placeholders
    for (const auto& [plugin, phs] : mRelationalPlaceholders) {
        for (const auto& [name, entries] : phs) {
            for (const auto& entry : entries) {
                addPlaceholder(
                    plugin,
                    name,
                    PlaceholderCategory::Relational,
                    false,
                    getTypeName(entry.targetTypeId),
                    getTypeName(entry.relationalTypeId)
                );
            }
        }
    }

    // 4. List Placeholders
    for (const auto& [plugin, phs] : mServerListPlaceholders) {
        for (const auto& [name, entry] : phs) {
            addPlaceholder(plugin, name, PlaceholderCategory::List, false);
        }
    }
    for (const auto& [plugin, phs] : mContextListPlaceholders) {
        for (const auto& [name, overloads] : phs) {
            for (const auto& [typeId, entry] : overloads) {
                addPlaceholder(plugin, name, PlaceholderCategory::List, false, getTypeName(entry.targetTypeId));
            }
        }
    }

    // 5. Object List Placeholders
    for (const auto& [plugin, phs] : mServerObjectListPlaceholders) {
        for (const auto& [name, entry] : phs) {
            addPlaceholder(plugin, name, PlaceholderCategory::ObjectList, false);
        }
    }
    for (const auto& [plugin, phs] : mContextObjectListPlaceholders) {
        for (const auto& [name, overloads] : phs) {
            for (const auto& [typeId, entry] : overloads) {
                addPlaceholder(plugin, name, PlaceholderCategory::ObjectList, false, getTypeName(entry.targetTypeId));
            }
        }
    }

    return result;
}

bool PlaceholderManager::hasPlaceholder(
    const std::string&                pluginName,
    const std::string&                placeholderName,
    const std::optional<std::string>& typeKey
) const {
    std::shared_lock lk(mMutex);

    if (typeKey) {
        // Check context placeholders
        auto itPlugin = mContextPlaceholders.find(pluginName);
        if (itPlugin != mContextPlaceholders.end()) {
            auto itPh = itPlugin->second.find(placeholderName);
            if (itPh != itPlugin->second.end()) {
                // If a typeKey is provided, we need to check if any registered replacer
                // is compatible with this type. This is a complex check involving inheritance.
                // For simplicity here, we check if any replacer is registered for this placeholder.
                // A more thorough check would involve findUpcastChain.
                return !itPh->second.empty();
            }
        }
        auto itAsyncPlugin = mAsyncContextPlaceholders.find(pluginName);
        if (itAsyncPlugin != mAsyncContextPlaceholders.end()) {
            auto itPh = itAsyncPlugin->second.find(placeholderName);
            if (itPh != itAsyncPlugin->second.end()) {
                return !itPh->second.empty();
            }
        }
    } else {
        // Check server placeholders
        auto itPlugin = mServerPlaceholders.find(pluginName);
        if (itPlugin != mServerPlaceholders.end()) {
            if (itPlugin->second.count(placeholderName)) {
                return true;
            }
        }
        auto itAsyncPlugin = mAsyncServerPlaceholders.find(pluginName);
        if (itAsyncPlugin != mAsyncServerPlaceholders.end()) {
            if (itAsyncPlugin->second.count(placeholderName)) {
                return true;
            }
        }
    }

    return false;
}


// ==== Context 构造 ====

PlaceholderContext
PlaceholderManager::makeContextRaw(void* ptr, const std::string& typeKeyStr, const PlaceholderContext* rel_ctx) {
    PlaceholderContext ctx;
    ctx.ptr               = ptr;
    ctx.typeId            = getInstance().ensureTypeId(typeKeyStr);
    ctx.relationalContext = rel_ctx;
    return ctx;
}

// ==== 配置 ====
void PlaceholderManager::setMaxRecursionDepth(int depth) { mMaxRecursionDepth = std::max(0, depth); }
int  PlaceholderManager::getMaxRecursionDepth() const { return mMaxRecursionDepth; }
void PlaceholderManager::setDoubleBraceEscape(bool enable) { mEnableDoubleBraceEscape = enable; }
bool PlaceholderManager::getDoubleBraceEscape() const { return mEnableDoubleBraceEscape; }

// ==== 替换 ====

std::string PlaceholderManager::replacePlaceholders(const std::string& text) {
    return replacePlaceholders(text, PlaceholderContext{nullptr, 0});
}

std::string PlaceholderManager::replacePlaceholders(const std::string& text, std::any contextObject) {
    if (!contextObject.has_value()) {
        return replacePlaceholders(text, PlaceholderContext{nullptr, 0});
    }

    if (contextObject.type() == typeid(PlaceholderContext)) {
        auto ctx = std::any_cast<PlaceholderContext>(contextObject);
        return replacePlaceholders(text, ctx);
    }

    try {
        if (contextObject.type() == typeid(Player*)) {
            auto p = std::any_cast<Player*>(contextObject);
            return replacePlaceholders(text, makeContext(p));
        }
    } catch (...) {}

    return replacePlaceholders(text, PlaceholderContext{nullptr, 0});
}

std::string PlaceholderManager::replacePlaceholders(const std::string& text, const PlaceholderContext& ctx) {
    if (auto cachedTpl = mCompileCache.get(text)) {
        return replacePlaceholders(**cachedTpl, ctx);
    }
    auto tpl = std::make_shared<CompiledTemplate>(compileTemplate(text));
    mCompileCache.put(text, tpl);
    return replacePlaceholders(*tpl, ctx);
}

std::string PlaceholderManager::replacePlaceholders(const std::string& text, Player* player) {
    return replacePlaceholders(text, makeContext(player));
}


// --- 新：关系型占位符注册 ---

void PlaceholderManager::registerRelationalPlaceholderForTypeId(
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

void PlaceholderManager::registerRelationalPlaceholderForTypeId(
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

void PlaceholderManager::registerRelationalPlaceholderForTypeKey(
    const std::string&           pluginName,
    const std::string&           placeholder,
    const std::string&           typeKeyStr,
    const std::string&           relationalTypeKeyStr,
    AnyPtrRelationalReplacer&&   replacer,
    std::optional<CacheDuration> cache_duration,
    CacheKeyStrategy             strategy
) {
    auto targetId     = ensureTypeId(typeKeyStr);
    auto relationalId = ensureTypeId(relationalTypeKeyStr);
    registerRelationalPlaceholderForTypeId(
        pluginName,
        placeholder,
        targetId,
        relationalId,
        std::move(replacer),
        cache_duration,
        strategy
    );
}

void PlaceholderManager::registerRelationalPlaceholderForTypeKeyWithParams(
    const std::string&                   pluginName,
    const std::string&                   placeholder,
    const std::string&                   typeKeyStr,
    const std::string&                   relationalTypeKeyStr,
    AnyPtrRelationalReplacerWithParams&& replacer,
    std::optional<CacheDuration>         cache_duration,
    CacheKeyStrategy                     strategy
) {
    auto targetId     = ensureTypeId(typeKeyStr);
    auto relationalId = ensureTypeId(relationalTypeKeyStr);
    registerRelationalPlaceholderForTypeId(
        pluginName,
        placeholder,
        targetId,
        relationalId,
        std::move(replacer),
        cache_duration,
        strategy
    );
}

// --- 新：异步替换 ---

std::future<std::string>
PlaceholderManager::replacePlaceholdersAsync(const std::string& text, const PlaceholderContext& ctx) {
    if (auto cachedTpl = mCompileCache.get(text)) {
        return replacePlaceholdersAsync(**cachedTpl, ctx);
    }
    auto tpl = std::make_shared<CompiledTemplate>(compileTemplate(text));
    mCompileCache.put(text, tpl);
    return replacePlaceholdersAsync(*tpl, ctx);
}

std::future<std::string>
PlaceholderManager::replacePlaceholdersAsync(const CompiledTemplate& tpl, const PlaceholderContext& ctx) {
    auto futures = std::make_shared<std::vector<std::future<std::string>>>();
    futures->reserve(tpl.tokens.size());

    for (const auto& token : tpl.tokens) {
        if (auto* literal = std::get_if<LiteralToken>(&token)) {
            std::promise<std::string> p;
            std::string               unescaped;
            std::string_view          text = literal->text;
            unescaped.reserve(text.size());
            for (size_t i = 0; i < text.size(); ++i) {
                if (mEnableDoubleBraceEscape && i + 1 < text.size()) {
                    if (text[i] == '{' && text[i + 1] == '{') {
                        unescaped.push_back('{');
                        i++; // Skip the second brace
                        continue;
                    }
                    if (text[i] == '}' && text[i + 1] == '}') {
                        unescaped.push_back('}');
                        i++; // Skip the second brace
                        continue;
                    }
                }
                unescaped.push_back(text[i]);
            }
            p.set_value(std::move(unescaped));
            futures->push_back(p.get_future());
        } else if (auto* ph_token = std::get_if<PlaceholderToken>(&token)) {
            // Resolve params and default value SYNCHRONOUSLY on the calling thread.
            std::string paramString;
            if (ph_token->paramsTemplate) {
                paramString = replacePlaceholdersSync(*ph_token->paramsTemplate, ctx, 1);
            }

            std::string defaultText;
            if (ph_token->defaultTemplate) {
                defaultText = replacePlaceholdersSync(*ph_token->defaultTemplate, ctx, 1);
            }

            // Execute the placeholder asynchronously and get a future.
            auto ph_future =
                executePlaceholderAsync(ph_token->pluginName, ph_token->placeholderName, paramString, defaultText, ctx);
            futures->push_back(std::move(ph_future));
        }
    }

    // Enqueue a final task that waits for all futures and concatenates the results.
    return mCombinerThreadPool->enqueue([futures, sourceSize = tpl.source.size()]() -> std::string {
        std::string result;
        result.reserve(sourceSize);
        for (auto& f : *futures) {
            result.append(f.get());
        }
        return result;
    });
}

std::vector<std::string> PlaceholderManager::replacePlaceholdersBatch(
    const std::vector<std::reference_wrapper<const CompiledTemplate>>& tpls,
    const PlaceholderContext&                                          ctx
) {
    std::vector<std::string> results;
    results.reserve(tpls.size());

    // Create a shared state for this batch operation
    ReplaceState st;
    st.depth = 0;

    for (const auto& tpl_ref : tpls) {
        const auto& tpl = tpl_ref.get();
        if (st.depth > mMaxRecursionDepth) {
            results.push_back(tpl.source);
            continue;
        }

        std::string result;
        result.reserve(tpl.source.size());

        for (const auto& token : tpl.tokens) {
            if (auto* literal = std::get_if<LiteralToken>(&token)) {
                std::string_view text = literal->text;
                for (size_t i = 0; i < text.size(); ++i) {
                    if (mEnableDoubleBraceEscape && i + 1 < text.size()) {
                        if (text[i] == '{' && text[i + 1] == '{') {
                            result.push_back('{');
                            i++; // Skip the second brace
                            continue;
                        }
                        if (text[i] == '}' && text[i + 1] == '}') {
                            result.push_back('}');
                            i++; // Skip the second brace
                            continue;
                        }
                    }
                    result.push_back(text[i]);
                }
            } else if (auto* placeholder = std::get_if<PlaceholderToken>(&token)) {
                std::string paramString;
                if (placeholder->paramsTemplate) {
                    // Note: Nested replacements in batch mode won't share the batch's cache.
                    // This is a limitation to avoid complexity.
                    paramString = replacePlaceholdersSync(*placeholder->paramsTemplate, ctx, st.depth + 1);
                }

                std::string defaultText;
                if (placeholder->defaultTemplate) {
                    defaultText = replacePlaceholdersSync(*placeholder->defaultTemplate, ctx, st.depth + 1);
                }

                // Use the shared state 'st'
                result.append(executePlaceholder(
                    placeholder->pluginName,
                    placeholder->placeholderName,
                    paramString,
                    defaultText,
                    ctx,
                    st
                ));
            }
        }
        results.push_back(std::move(result));
    }

    return results;
}

// [新] 同步替换实现
std::string
PlaceholderManager::replacePlaceholdersSync(const CompiledTemplate& tpl, const PlaceholderContext& ctx, int depth) {
    if (depth > mMaxRecursionDepth) {
        return tpl.source; // 超出深度，返回原始文本
    }

    std::string result;
    result.reserve(tpl.source.size());

    ReplaceState st; // 同步执行的本地状态
    st.depth = depth;

    for (const auto& token : tpl.tokens) {
        if (auto* literal = std::get_if<LiteralToken>(&token)) {
            std::string_view text = literal->text;
            for (size_t i = 0; i < text.size(); ++i) {
                if (mEnableDoubleBraceEscape && i + 1 < text.size()) {
                    if (text[i] == '{' && text[i + 1] == '{') {
                        result.push_back('{');
                        i++; // Skip the second brace
                        continue;
                    }
                    if (text[i] == '}' && text[i + 1] == '}') {
                        result.push_back('}');
                        i++; // Skip the second brace
                        continue;
                    }
                }
                result.push_back(text[i]);
            }
        } else if (auto* placeholder = std::get_if<PlaceholderToken>(&token)) {
            std::string paramString;
            if (placeholder->paramsTemplate) {
                paramString = replacePlaceholdersSync(*placeholder->paramsTemplate, ctx, depth + 1);
            }

            std::string defaultText;
            if (placeholder->defaultTemplate) {
                defaultText = replacePlaceholdersSync(*placeholder->defaultTemplate, ctx, depth + 1);
            }

            result.append(executePlaceholder(
                placeholder->pluginName,
                placeholder->placeholderName,
                paramString,
                defaultText,
                ctx,
                st
            ));
        }
    }
    return result;
}

// [新] 编译模板
CompiledTemplate PlaceholderManager::compileTemplate(const std::string& text) {
    CompiledTemplate tpl;
    tpl.source         = text; // Keep the source string alive
    std::string_view s = tpl.source;
    size_t           n = s.size();

    for (size_t i = 0; i < n;) {
        char c = s[i];

        // 转义
        if (mEnableDoubleBraceEscape && c == '{' && i + 1 < n && s[i + 1] == '{') {
            // 找到下一个非 {{ 的位置
            size_t literalEnd = i + 2;
            while (literalEnd < n) {
                if (s[literalEnd] == '{' && literalEnd + 1 < n && s[literalEnd + 1] == '{') {
                    literalEnd += 2;
                } else if (s[literalEnd] == '%' && literalEnd + 1 < n && s[literalEnd + 1] == '%') {
                    literalEnd += 2;
                } else {
                    break;
                }
            }
            tpl.tokens.emplace_back(LiteralToken{s.substr(i, literalEnd - i)});
            i = literalEnd;
            continue;
        }
        if (mEnableDoubleBraceEscape && c == '}' && i + 1 < n && s[i + 1] == '}') {
            size_t literalEnd = i + 2;
            while (literalEnd < n) {
                if (s[literalEnd] == '}' && literalEnd + 1 < n && s[literalEnd + 1] == '}') {
                    literalEnd += 2;
                } else if (s[literalEnd] == '%' && literalEnd + 1 < n && s[literalEnd + 1] == '%') {
                    literalEnd += 2;
                } else {
                    break;
                }
            }
            tpl.tokens.emplace_back(LiteralToken{s.substr(i, literalEnd - i)});
            i = literalEnd;
            continue;
        }
        // 新增：百分号转义
        if (c == '%' && i + 1 < n && s[i + 1] == '%') {
            size_t literalEnd = i + 2;
            while (literalEnd < n) {
                if (s[literalEnd] == '%' && literalEnd + 1 < n && s[literalEnd + 1] == '%') {
                    literalEnd += 2;
                } else if (mEnableDoubleBraceEscape && s[literalEnd] == '{' && literalEnd + 1 < n && s[literalEnd + 1] == '{') {
                    literalEnd += 2;
                } else if (mEnableDoubleBraceEscape && s[literalEnd] == '}' && literalEnd + 1 < n && s[literalEnd + 1] == '}') {
                    literalEnd += 2;
                } else {
                    break;
                }
            }
            tpl.tokens.emplace_back(LiteralToken{s.substr(i, literalEnd - i)});
            i = literalEnd;
            continue;
        }

        // 寻找占位符的开始 '{'
        size_t placeholderStart = s.find('{', i);
        if (placeholderStart == std::string::npos) {
            // 剩余全是文本
            if (i < n) {
                tpl.tokens.emplace_back(LiteralToken{s.substr(i)});
            }
            break;
        }

        // i 到 placeholderStart 之间是文本
        if (placeholderStart > i) {
            tpl.tokens.emplace_back(LiteralToken{s.substr(i, placeholderStart - i)});
        }

        // 寻找匹配的 '}'
        size_t j       = placeholderStart + 1;
        int    depth   = 1;
        bool   matched = false;
        while (j < n) {
            if (mEnableDoubleBraceEscape && s[j] == '{' && j + 1 < n && s[j + 1] == '{') {
                j += 2;
                continue;
            }
            if (mEnableDoubleBraceEscape && s[j] == '}' && j + 1 < n && s[j + 1] == '}') {
                j += 2;
                continue;
            }
            if (s[j] == '{') {
                ++depth;
            } else if (s[j] == '}') {
                --depth;
                if (depth == 0) {
                    matched = true;
                    break;
                }
            }
            ++j;
        }

        if (!matched) {
            // 无匹配，后面全是文本
            tpl.tokens.emplace_back(LiteralToken{s.substr(placeholderStart)});
            break;
        }

        // 提取占位符内部
        std::string_view inside = s.substr(placeholderStart + 1, j - (placeholderStart + 1));

        // 解析
        auto colonPosOpt = PA::Utils::findSepOutside(inside, ":");
        if (!colonPosOpt) {
            // 非法格式，视为文本
            if (ConfigManager::getInstance().get().debugMode) {
                logger.warn(
                    "Placeholder format error: '{}' is not a valid placeholder format. It seems to be missing a "
                    "colon ':'. The correct format is {{plugin:placeholder}}.",
                    std::string(inside)
                );
            }
            tpl.tokens.emplace_back(LiteralToken{s.substr(placeholderStart, j - placeholderStart + 1)});
            i = j + 1;
            continue;
        }

        PlaceholderToken token;
        token.pluginName      = PA::Utils::trim_sv(inside.substr(0, *colonPosOpt));
        std::string_view rest = inside.substr(*colonPosOpt + 1);

        auto defaultPosOpt = PA::Utils::findSepOutside(rest, ":-");
        auto pipePosOpt    = PA::Utils::findSepOutside(rest, "|");

        size_t nameEnd        = std::min(defaultPosOpt.value_or(rest.size()), pipePosOpt.value_or(rest.size()));
        token.placeholderName = PA::Utils::trim_sv(rest.substr(0, nameEnd));

        if (defaultPosOpt) {
            size_t defaultStart = *defaultPosOpt + 2;
            size_t defaultEnd   = pipePosOpt ? *pipePosOpt : rest.size();
            if (defaultStart < defaultEnd) {
                token.defaultTemplate = std::make_unique<CompiledTemplate>(
                    compileTemplate(std::string(rest.substr(defaultStart, defaultEnd - defaultStart)))
                );
            }
        }

        if (pipePosOpt) {
            size_t paramStart = *pipePosOpt + 1;
            if (paramStart < rest.size()) {
                token.paramsTemplate =
                    std::make_unique<CompiledTemplate>(compileTemplate(std::string(rest.substr(paramStart))));
            }
        }

        tpl.tokens.emplace_back(std::move(token));
        i = j + 1;
    }
    return tpl;
}

// [新] 私有辅助函数：构造缓存键
std::string PlaceholderManager::buildCacheKey(
    const PlaceholderContext& ctx,
    std::string_view          pluginName,
    std::string_view          placeholderName,
    const std::string&        paramString,
    CacheKeyStrategy          strategy
) {
    std::string cacheKey;
    cacheKey.reserve(pluginName.size() + placeholderName.size() + paramString.size() + 80);
    if (strategy == CacheKeyStrategy::ServerOnly) {
        cacheKey.push_back('#');
        cacheKey.append(pluginName);
        cacheKey.push_back(':');
        cacheKey.append(placeholderName);
        cacheKey.push_back('|');
        cacheKey.append(paramString);
    } else { // Default
        if (ctx.ptr) {
            cacheKey.append(std::to_string(reinterpret_cast<uintptr_t>(ctx.ptr)));
            cacheKey.push_back('#');
            cacheKey.append(std::to_string(ctx.typeId));
            cacheKey.push_back('#');
        }
        // Add relational context to cache key
        if (ctx.relationalContext && ctx.relationalContext->ptr) {
            cacheKey.append(std::to_string(reinterpret_cast<uintptr_t>(ctx.relationalContext->ptr)));
            cacheKey.push_back('#');
            cacheKey.append(std::to_string(ctx.relationalContext->typeId));
            cacheKey.push_back('#');
        }
        cacheKey.append(pluginName);
        cacheKey.push_back(':');
        cacheKey.append(placeholderName);
        cacheKey.push_back('|');
        cacheKey.append(paramString);
    }
    return cacheKey;
}

// [新] 私有辅助函数：执行单个占位符的查找与替换
// --- 新：列表/集合型占位符注册 ---

void PlaceholderManager::registerServerListPlaceholder(
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

void PlaceholderManager::registerServerListPlaceholderWithParams(
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

void PlaceholderManager::registerListPlaceholderForTypeId(
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

void PlaceholderManager::registerListPlaceholderForTypeId(
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

// --- 新：对象列表/集合型占位符注册 ---

void PlaceholderManager::registerServerObjectListPlaceholder(
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

void PlaceholderManager::registerServerObjectListPlaceholderWithParams(
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

void PlaceholderManager::registerObjectListPlaceholderForTypeId(
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

void PlaceholderManager::registerObjectListPlaceholderForTypeId(
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

std::string PlaceholderManager::executePlaceholder(
    std::string_view             pluginName,
    std::string_view             placeholderName,
    const std::string&           paramString,
    const std::string&           defaultText,
    const PlaceholderContext&    ctx,
    ReplaceState&                st,
    std::optional<CacheDuration> cache_duration_override
) {
    const auto startTime = std::chrono::high_resolution_clock::now();
    enum class PlaceholderType {
        None,
        Server,
        Context,
        Relational,
        ListServer,
        ListContext,
        ObjectListServer,
        ObjectListContext
    };
    PlaceholderType type = PlaceholderType::None;

    // --- 1. Find Candidate ---
    struct Candidate {
        std::vector<Caster>                                    chain;
        std::variant<AnyPtrReplacer, AnyPtrReplacerWithParams> fn;
        std::optional<CacheDuration>                           cacheDuration;
        CacheKeyStrategy                                       strategy;
    };
    struct RelationalCandidate {
        std::vector<Caster>                                                        chain;
        std::vector<Caster>                                                        rel_chain;
        std::variant<AnyPtrRelationalReplacer, AnyPtrRelationalReplacerWithParams> fn;
        std::optional<CacheDuration>                                               cacheDuration;
        CacheKeyStrategy                                                           strategy;
    };

    struct ListCandidate {
        std::vector<Caster>                                            chain;
        std::variant<AnyPtrListReplacer, AnyPtrListReplacerWithParams> fn;
        std::optional<CacheDuration>                                   cacheDuration;
        CacheKeyStrategy                                               strategy;
    };

    struct ObjectListCandidate {
        std::vector<Caster>                                                        chain;
        std::variant<AnyPtrObjectListReplacer, AnyPtrObjectListReplacerWithParams> fn;
        std::optional<CacheDuration>                                               cacheDuration;
        CacheKeyStrategy                                                           strategy;
    };
    std::optional<RelationalCandidate> bestRelationalCandidate;
    if (ctx.ptr != nullptr && ctx.typeId != 0 && ctx.relationalContext != nullptr
        && ctx.relationalContext->ptr != nullptr && ctx.relationalContext->typeId != 0) {
        std::vector<RelationalTypedReplacer> potentialReplacers;
        {
            std::shared_lock lk(mMutex);
            auto             plugin_it = mRelationalPlaceholders.find(std::string(pluginName));
            if (plugin_it != mRelationalPlaceholders.end()) {
                auto ph_it = plugin_it->second.find(std::string(placeholderName));
                if (ph_it != plugin_it->second.end()) {
                    potentialReplacers = ph_it->second;
                }
            }
        }

        std::vector<std::pair<int, RelationalCandidate>> candidates;
        for (auto& entry : potentialReplacers) {
            std::vector<Caster> chain;
            std::vector<Caster> rel_chain;
            bool main_ok = (entry.targetTypeId == ctx.typeId) || findUpcastChain(ctx.typeId, entry.targetTypeId, chain);
            bool rel_ok  = (entry.relationalTypeId == ctx.relationalContext->typeId)
                       || findUpcastChain(ctx.relationalContext->typeId, entry.relationalTypeId, rel_chain);

            if (main_ok && rel_ok) {
                candidates.push_back({
                    (int)(chain.size() + rel_chain.size()),
                    RelationalCandidate{
                                        std::move(chain),
                                        std::move(rel_chain),
                                        entry.fn,
                                        entry.cacheDuration,
                                        entry.strategy
                    }
                });
            }
        }
        if (!candidates.empty()) {
            std::sort(candidates.begin(), candidates.end(), [](auto& a, auto& b) { return a.first < b.first; });
            bestRelationalCandidate = std::move(candidates.front().second);
            type                    = PlaceholderType::Relational;
        }
    }


    std::optional<Candidate>           bestCandidate;
    std::optional<ListCandidate>       bestListCandidate;
    std::optional<ObjectListCandidate> bestObjectListCandidate;

    if (!bestRelationalCandidate && ctx.ptr != nullptr && ctx.typeId != 0) {
        std::vector<std::pair<int, Candidate>>           candidates;
        std::vector<std::pair<int, ListCandidate>>       listCandidates;
        std::vector<std::pair<int, ObjectListCandidate>> objectListCandidates;

        {
            std::shared_lock lk(mMutex);
            // Gather all potential replacers by iterating up the inheritance chain
            std::queue<std::size_t>         q;
            q.push(ctx.typeId);
            std::unordered_set<std::size_t> visited;
            visited.insert(ctx.typeId);

            while (!q.empty()) {
                std::size_t currentTypeId = q.front();
                q.pop();

                // Find chain from original type to current type
                std::vector<Caster> chain;
                // findUpcastChain is cached, so this is efficient.
                bool chain_found = findUpcastChain(ctx.typeId, currentTypeId, chain);
                if (!chain_found && ctx.typeId != currentTypeId) continue;

                // 1. Check for normal context placeholders
                auto plugin_it = mContextPlaceholders.find(std::string(pluginName));
                if (plugin_it != mContextPlaceholders.end()) {
                    auto ph_it = plugin_it->second.find(std::string(placeholderName));
                    if (ph_it != plugin_it->second.end()) {
                        auto range = ph_it->second.equal_range(currentTypeId);
                        for (auto it = range.first; it != range.second; ++it) {
                            candidates.push_back({
                                (int)chain.size(),
                                Candidate{chain, it->second.fn, it->second.cacheDuration, it->second.strategy}
                            });
                        }
                    }
                }

                // 2. Check for list context placeholders
                auto list_plugin_it = mContextListPlaceholders.find(std::string(pluginName));
                if (list_plugin_it != mContextListPlaceholders.end()) {
                    auto ph_it = list_plugin_it->second.find(std::string(placeholderName));
                    if (ph_it != list_plugin_it->second.end()) {
                        auto range = ph_it->second.equal_range(currentTypeId);
                        for (auto it = range.first; it != range.second; ++it) {
                            listCandidates.push_back({
                                (int)chain.size(),
                                ListCandidate{chain, it->second.fn, it->second.cacheDuration, it->second.strategy}
                            });
                        }
                    }
                }

                // 3. Check for object list context placeholders
                auto obj_list_plugin_it = mContextObjectListPlaceholders.find(std::string(pluginName));
                if (obj_list_plugin_it != mContextObjectListPlaceholders.end()) {
                    auto ph_it = obj_list_plugin_it->second.find(std::string(placeholderName));
                    if (ph_it != obj_list_plugin_it->second.end()) {
                        auto range = ph_it->second.equal_range(currentTypeId);
                        for (auto it = range.first; it != range.second; ++it) {
                            objectListCandidates.push_back({
                                (int)chain.size(),
                                ObjectListCandidate{
                                    chain,
                                    it->second.fn,
                                    it->second.cacheDuration,
                                    it->second.strategy
                                }
                            });
                        }
                    }
                }

                // Enqueue parents
                auto edge_it = mUpcastEdges.find(currentTypeId);
                if (edge_it != mUpcastEdges.end()) {
                    for (const auto& [parentTypeId, caster] : edge_it->second) {
                        if (visited.find(parentTypeId) == visited.end()) {
                            visited.insert(parentTypeId);
                            q.push(parentTypeId);
                        }
                    }
                }
            }
        }

        if (!candidates.empty()) {
            std::sort(candidates.begin(), candidates.end(), [](auto& a, auto& b) { return a.first < b.first; });
            bestCandidate = std::move(candidates.front().second);
            type          = PlaceholderType::Context;
        } else if (!listCandidates.empty()) {
            std::sort(listCandidates.begin(), listCandidates.end(), [](auto& a, auto& b) { return a.first < b.first; });
            bestListCandidate = std::move(listCandidates.front().second);
            type              = PlaceholderType::ListContext;
        } else if (!objectListCandidates.empty()) {
            std::sort(
                objectListCandidates.begin(),
                objectListCandidates.end(),
                [](auto& a, auto& b) { return a.first < b.first; }
            );
            bestObjectListCandidate = std::move(objectListCandidates.front().second);
            type                    = PlaceholderType::ObjectListContext;
        }
    }

    std::optional<ServerReplacerEntry>           serverEntry;
    std::optional<ServerListReplacerEntry>       serverListEntry;
    std::optional<ServerObjectListReplacerEntry> serverObjectListEntry;
    if (!bestRelationalCandidate && !bestCandidate && !bestListCandidate && !bestObjectListCandidate) {
        std::shared_lock lk(mMutex);
        // 优先查找普通服务器占位符
        auto plugin_it = mServerPlaceholders.find(std::string(pluginName));
        if (plugin_it != mServerPlaceholders.end()) {
            auto placeholder_it = plugin_it->second.find(std::string(placeholderName));
            if (placeholder_it != plugin_it->second.end()) {
                serverEntry = placeholder_it->second;
                type        = PlaceholderType::Server;
            }
        }
        // 如果没找到，再查找列表型服务器占位符
        if (!serverEntry) {
            auto list_plugin_it = mServerListPlaceholders.find(std::string(pluginName));
            if (list_plugin_it != mServerListPlaceholders.end()) {
                auto list_placeholder_it = list_plugin_it->second.find(std::string(placeholderName));
                if (list_placeholder_it != list_plugin_it->second.end()) {
                    serverListEntry = list_placeholder_it->second;
                    type            = PlaceholderType::ListServer;
                }
            }
        }
        // 如果都没找到，再查找对象列表型服务器占位符
        if (!serverEntry && !serverListEntry) {
            auto object_list_plugin_it = mServerObjectListPlaceholders.find(std::string(pluginName));
            if (object_list_plugin_it != mServerObjectListPlaceholders.end()) {
                auto object_list_placeholder_it = object_list_plugin_it->second.find(std::string(placeholderName));
                if (object_list_placeholder_it != object_list_plugin_it->second.end()) {
                    serverObjectListEntry = object_list_placeholder_it->second;
                    type                  = PlaceholderType::ObjectListServer;
                }
            }
        }
    }

    // --- 2. Build Cache Key & Check Cache ---
    std::string                  cacheKey;
    std::optional<CacheDuration> cacheDuration = cache_duration_override;
    bool hasCandidate = bestRelationalCandidate.has_value() || bestCandidate.has_value() || serverEntry.has_value()
                     || bestListCandidate.has_value() || serverListEntry.has_value()
                     || bestObjectListCandidate.has_value() || serverObjectListEntry.has_value();

    if (hasCandidate) {
        CacheKeyStrategy strategy = CacheKeyStrategy::Default;
        if (bestRelationalCandidate) {
            strategy = bestRelationalCandidate->strategy;
        } else if (bestCandidate) {
            strategy = bestCandidate->strategy;
        } else if (bestListCandidate) {
            strategy = bestListCandidate->strategy;
        } else if (serverEntry) {
            strategy = serverEntry->strategy;
        } else if (serverListEntry) {
            strategy = serverListEntry->strategy;
        }
        cacheKey = buildCacheKey(ctx, pluginName, placeholderName, paramString, strategy);

        if (!cacheDuration) {
            if (bestRelationalCandidate) {
                cacheDuration = bestRelationalCandidate->cacheDuration;
            } else if (bestCandidate) {
                cacheDuration = bestCandidate->cacheDuration;
            } else if (bestListCandidate) {
                cacheDuration = bestListCandidate->cacheDuration;
            } else if (serverEntry) {
                cacheDuration = serverEntry->cacheDuration;
            } else if (serverListEntry) {
                cacheDuration = serverListEntry->cacheDuration;
            }
        }

        auto itCache = st.cache.find(cacheKey);
        if (itCache != st.cache.end()) {
            if (ConfigManager::getInstance().get().debugMode) {
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                                    std::chrono::high_resolution_clock::now() - startTime
                )
                                    .count();
                logger.info(
                    "Placeholder '{}:{}' | Cache Hit: Yes (local) | Type: {} | Time: {}us",
                    std::string(pluginName),
                    std::string(placeholderName),
                    type == PlaceholderType::Context ? "Context" : "Server",
                    duration
                );
            }
            std::string out = itCache->second;
            if (out.empty() && !defaultText.empty()) out = defaultText;
            return out;
        }

        if (cacheDuration) {
            auto cached = mGlobalCache.get(cacheKey);
            if (cached && cached->expiresAt > std::chrono::steady_clock::now()) {
                if (ConfigManager::getInstance().get().debugMode) {
                    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                                        std::chrono::high_resolution_clock::now() - startTime
                    )
                                        .count();
                    logger.info(
                        "Placeholder '{}:{}' | Cache Hit: Yes (global) | Type: {} | Time: {}us",
                        std::string(pluginName),
                        std::string(placeholderName),
                        type == PlaceholderType::Context ? "Context" : "Server",
                        duration
                    );
                }
                st.cache.emplace(cacheKey, cached->result);
                std::string out = cached->result;
                if (out.empty() && !defaultText.empty()) out = defaultText;
                return out;
            }
        }
    }

    // --- 3. Execute Placeholder ---
    std::shared_ptr<Utils::ParsedParams> paramsPtr;
    if (auto cachedParams = mParamsCache.get(paramString)) {
        paramsPtr = *cachedParams;
    } else {
        paramsPtr = std::make_shared<Utils::ParsedParams>(paramString);
        mParamsCache.put(paramString, paramsPtr);
    }
    const auto&             params     = *paramsPtr;
    bool                    allowEmpty = params.getBool("allowempty").value_or(false);
    std::string             replaced_val;
    bool                    replaced = false;

    if (bestRelationalCandidate) {
        void* p = ctx.ptr;
        for (auto& cfun : bestRelationalCandidate->chain) {
            p = cfun(p);
        }
        void* p_rel = ctx.relationalContext->ptr;
        for (auto& cfun : bestRelationalCandidate->rel_chain) {
            p_rel = cfun(p_rel);
        }
        if (p && p_rel) {
            if (std::holds_alternative<AnyPtrRelationalReplacer>(bestRelationalCandidate->fn)) {
                replaced_val = std::get<AnyPtrRelationalReplacer>(bestRelationalCandidate->fn)(p, p_rel);
            } else {
                replaced_val =
                    std::get<AnyPtrRelationalReplacerWithParams>(bestRelationalCandidate->fn)(p, p_rel, params);
            }
            if (!replaced_val.empty() || allowEmpty) {
                replaced = true;
            }
        }
    } else if (bestCandidate) {
        void* p = ctx.ptr;
        for (auto& cfun : bestCandidate->chain) {
            p = cfun(p);
        }
        if (p) {
            if (std::holds_alternative<AnyPtrReplacer>(bestCandidate->fn)) {
                replaced_val = std::get<AnyPtrReplacer>(bestCandidate->fn)(p);
            } else {
                replaced_val = std::get<AnyPtrReplacerWithParams>(bestCandidate->fn)(p, params);
            }
            if (!replaced_val.empty() || allowEmpty) {
                replaced = true;
            }
        }
    } else if (bestListCandidate) {
        void* p = ctx.ptr;
        for (auto& cfun : bestListCandidate->chain) {
            p = cfun(p);
        }
        if (p) {
            std::vector<std::string> result_list;
            if (std::holds_alternative<AnyPtrListReplacer>(bestListCandidate->fn)) {
                result_list = std::get<AnyPtrListReplacer>(bestListCandidate->fn)(p);
            } else {
                result_list = std::get<AnyPtrListReplacerWithParams>(bestListCandidate->fn)(p, params);
            }
            if (!result_list.empty() || allowEmpty) {
                std::string separator = std::string(params.get("separator").value_or(", "));
                replaced_val          = PA::Utils::join(result_list, separator);
                replaced              = true;
            }
        }
    } else if (serverEntry) {
        if (std::holds_alternative<ServerReplacer>(serverEntry->fn)) {
            replaced_val = std::get<ServerReplacer>(serverEntry->fn)();
        } else {
            replaced_val = std::get<ServerReplacerWithParams>(serverEntry->fn)(params);
        }
        if (!replaced_val.empty() || allowEmpty) {
            replaced = true;
        }
    } else if (serverListEntry) {
        std::vector<std::string> result_list;
        if (std::holds_alternative<ServerListReplacer>(serverListEntry->fn)) {
            result_list = std::get<ServerListReplacer>(serverListEntry->fn)();
        } else {
            result_list = std::get<ServerListReplacerWithParams>(serverListEntry->fn)(params);
        }
        if (!result_list.empty() || allowEmpty) {
            std::string separator = std::string(params.get("separator").value_or(", "));
            replaced_val          = PA::Utils::join(result_list, separator);
            replaced              = true;
        }
    } else if (bestObjectListCandidate) {
        void* p = ctx.ptr;
        for (auto& cfun : bestObjectListCandidate->chain) {
            p = cfun(p);
        }
        if (p) {
            std::vector<PlaceholderContext> object_list;
            if (std::holds_alternative<AnyPtrObjectListReplacer>(bestObjectListCandidate->fn)) {
                object_list = std::get<AnyPtrObjectListReplacer>(bestObjectListCandidate->fn)(p);
            } else {
                object_list = std::get<AnyPtrObjectListReplacerWithParams>(bestObjectListCandidate->fn)(p, params);
            }

            if (!object_list.empty() || allowEmpty) {
                std::string join_separator = std::string(params.get("join").value_or(""));
                std::string template_str   = std::string(params.get("template").value_or(""));

                std::vector<std::string> replaced_objects;
                replaced_objects.reserve(object_list.size());

                if (!template_str.empty()) {
                    auto compiled_template = compileTemplate(template_str);
                    for (const auto& obj_ctx : object_list) {
                        replaced_objects.push_back(replacePlaceholdersSync(compiled_template, obj_ctx, st.depth + 1));
                    }
                } else {
                    // 如果没有提供模板，则尝试将每个对象的 typeKey 转换为字符串
                    for (const auto& obj_ctx : object_list) {
                        std::string typeName;
                        auto        aliasIt = mIdToAlias.find(obj_ctx.typeId);
                        if (aliasIt != mIdToAlias.end()) {
                            typeName = aliasIt->second;
                        } else {
                            auto keyIt = mIdToTypeKey.find(obj_ctx.typeId);
                            if (keyIt != mIdToTypeKey.end()) {
                                typeName = keyIt->second;
                            } else {
                                typeName = "UnknownTypeId(" + std::to_string(obj_ctx.typeId) + ")";
                            }
                        }
                        replaced_objects.push_back(typeName);
                    }
                }
                replaced_val = PA::Utils::join(replaced_objects, join_separator);
                replaced     = true;
            }
        }
    } else if (serverObjectListEntry) {
        std::vector<PlaceholderContext> object_list;
        if (std::holds_alternative<ServerObjectListReplacer>(serverObjectListEntry->fn)) {
            object_list = std::get<ServerObjectListReplacer>(serverObjectListEntry->fn)();
        } else {
            object_list = std::get<ServerObjectListReplacerWithParams>(serverObjectListEntry->fn)(params);
        }

        if (!object_list.empty() || allowEmpty) {
            std::string join_separator = std::string(params.get("join").value_or(""));
            std::string template_str   = std::string(params.get("template").value_or(""));

            std::vector<std::string> replaced_objects;
            replaced_objects.reserve(object_list.size());

            if (!template_str.empty()) {
                auto compiled_template = compileTemplate(template_str);
                for (const auto& obj_ctx : object_list) {
                    replaced_objects.push_back(replacePlaceholdersSync(compiled_template, obj_ctx, st.depth + 1));
                }
            } else {
                // 如果没有提供模板，则尝试将每个对象的 typeKey 转换为字符串
                for (const auto& obj_ctx : object_list) {
                    std::string typeName;
                    auto        aliasIt = mIdToAlias.find(obj_ctx.typeId);
                    if (aliasIt != mIdToAlias.end()) {
                        typeName = aliasIt->second;
                    } else {
                        auto keyIt = mIdToTypeKey.find(obj_ctx.typeId);
                        if (keyIt != mIdToTypeKey.end()) {
                            typeName = keyIt->second;
                        } else {
                            typeName = "UnknownTypeId(" + std::to_string(obj_ctx.typeId) + ")";
                        }
                    }
                    replaced_objects.push_back(typeName);
                }
            }
            replaced_val = PA::Utils::join(replaced_objects, join_separator);
            replaced     = true;
        }
    }

    // --- 4. Finalize & Cache ---
    std::string formatted_val;
    if (replaced) {
        formatted_val = PA::Utils::applyFormatting(replaced_val, params);
    }

    std::string finalOut;
    if (replaced && (!formatted_val.empty() || allowEmpty)) {
        finalOut = formatted_val;
    } else {
        finalOut = defaultText;
    }

    if (ConfigManager::getInstance().get().debugMode) {
        auto duration =
            std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - startTime)
                .count();
        const char* typeStr = "Unknown";
        if (type == PlaceholderType::Relational) typeStr = "Relational";
        else if (type == PlaceholderType::Context) typeStr = "Context";
        else if (type == PlaceholderType::Server) typeStr = "Server";
        else if (type == PlaceholderType::ListContext) typeStr = "ListContext";
        else if (type == PlaceholderType::ListServer) typeStr = "ListServer";
        else if (type == PlaceholderType::ObjectListContext) typeStr = "ObjectListContext";
        else if (type == PlaceholderType::ObjectListServer) typeStr = "ObjectListServer";

        if (!replaced) {
            logger.warn(
                "Placeholder '{}:{}' not found or returned empty, and no default value provided. Using default: '{}'. "
                "Context ptr: {}, typeId: {}. Params: '{}'. Time: {}us",
                std::string(pluginName),
                std::string(placeholderName),
                defaultText,
                reinterpret_cast<uintptr_t>(ctx.ptr),
                ctx.typeId,
                paramString,
                duration
            );
        } else {
            logger.info(
                "Placeholder '{}:{}' | Cache Hit: No | Type: {} | Time: {}us",
                std::string(pluginName),
                std::string(placeholderName),
                typeStr,
                duration
            );
        }
    }

    if (finalOut.empty() && defaultText.empty() && !replaced && ConfigManager::getInstance().get().debugMode) {
        finalOut.reserve(pluginName.size() + placeholderName.size() + paramString.size() + 4);
        finalOut.append("{").append(pluginName).append(":").append(placeholderName);
        if (!paramString.empty()) finalOut.append("|").append(paramString);
        finalOut.append("}");
    }

    if (replaced) {
        st.cache.emplace(cacheKey, finalOut);
        if (cacheDuration) {
            mGlobalCache.put(cacheKey, {finalOut, std::chrono::steady_clock::now() + *cacheDuration});
        }
    }

    return finalOut;
}


// [新] 私有辅助函数：执行单个占位符的异步查找与替换
std::future<std::string> PlaceholderManager::executePlaceholderAsync(
    std::string_view             pluginName,
    std::string_view             placeholderName,
    const std::string&           paramString,
    const std::string&           defaultText,
    const PlaceholderContext&    ctx,
    std::optional<CacheDuration> cache_duration_override
) {
    const auto startTime = std::chrono::high_resolution_clock::now();
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
    PlaceholderType type = PlaceholderType::None;

    // --- 1. Find Async Candidate ---
    struct AsyncCandidate {
        std::vector<Caster>                                              chain;
        std::variant<AsyncAnyPtrReplacer, AsyncAnyPtrReplacerWithParams> fn;
        std::optional<CacheDuration>                                     cacheDuration;
        CacheKeyStrategy                                                 strategy;
    };
    std::optional<AsyncCandidate> bestAsyncCandidate;

    if (ctx.ptr != nullptr && ctx.typeId != 0) {
        std::vector<std::pair<int, AsyncCandidate>> asyncCandidates;
        {
            std::shared_lock lk(mMutex);
            std::queue<std::size_t>         q;
            q.push(ctx.typeId);
            std::unordered_set<std::size_t> visited;
            visited.insert(ctx.typeId);

            while (!q.empty()) {
                std::size_t currentTypeId = q.front();
                q.pop();

                std::vector<Caster> chain;
                if (findUpcastChain(ctx.typeId, currentTypeId, chain) || ctx.typeId == currentTypeId) {
                    auto plugin_it = mAsyncContextPlaceholders.find(std::string(pluginName));
                    if (plugin_it != mAsyncContextPlaceholders.end()) {
                        auto ph_it = plugin_it->second.find(std::string(placeholderName));
                        if (ph_it != plugin_it->second.end()) {
                            auto range = ph_it->second.equal_range(currentTypeId);
                            for (auto it = range.first; it != range.second; ++it) {
                                asyncCandidates.push_back({(int)chain.size(),
                                                           AsyncCandidate{chain,
                                                                          it->second.fn,
                                                                          it->second.cacheDuration,
                                                                          it->second.strategy}});
                            }
                        }
                    }
                }

                auto edge_it = mUpcastEdges.find(currentTypeId);
                if (edge_it != mUpcastEdges.end()) {
                    for (const auto& [parentTypeId, caster] : edge_it->second) {
                        if (visited.find(parentTypeId) == visited.end()) {
                            visited.insert(parentTypeId);
                            q.push(parentTypeId);
                        }
                    }
                }
            }
        }

        if (!asyncCandidates.empty()) {
            std::sort(asyncCandidates.begin(), asyncCandidates.end(), [](auto& a, auto& b) {
                return a.first < b.first;
            });
            bestAsyncCandidate = std::move(asyncCandidates.front().second);
            type               = PlaceholderType::AsyncContext;
        }
    }

    std::optional<AsyncServerReplacerEntry> asyncServerEntry;
    if (!bestAsyncCandidate) {
        std::shared_lock lk(mMutex);
        auto             plugin_it = mAsyncServerPlaceholders.find(std::string(pluginName));
        if (plugin_it != mAsyncServerPlaceholders.end()) {
            auto placeholder_it = plugin_it->second.find(std::string(placeholderName));
            if (placeholder_it != plugin_it->second.end()) {
                asyncServerEntry = placeholder_it->second;
                type             = PlaceholderType::AsyncServer;
            }
        }
    }

    // --- 2. Build Cache Key & Check Cache ---
    std::string                  cacheKey;
    std::optional<CacheDuration> cacheDuration     = cache_duration_override;
    bool                         hasAsyncCandidate = bestAsyncCandidate.has_value() || asyncServerEntry.has_value();

    if (hasAsyncCandidate) {
        CacheKeyStrategy strategy = bestAsyncCandidate ? bestAsyncCandidate->strategy
                                  : asyncServerEntry   ? asyncServerEntry->strategy
                                                       : CacheKeyStrategy::Default;
        cacheKey                  = buildCacheKey(ctx, pluginName, placeholderName, paramString, strategy);

        if (!cacheDuration) {
            cacheDuration = bestAsyncCandidate ? bestAsyncCandidate->cacheDuration : asyncServerEntry->cacheDuration;
        }

        if (cacheDuration) {
            auto cached = mGlobalCache.get(cacheKey);
            if (cached && cached->expiresAt > std::chrono::steady_clock::now()) {
                if (ConfigManager::getInstance().get().debugMode) {
                    auto duration =
                        std::chrono::duration_cast<std::chrono::microseconds>(
                            std::chrono::high_resolution_clock::now() - startTime
                        )
                            .count();
                    logger.info(
                        "Placeholder '{}:{}' | Cache Hit: Yes (global) | Type: {} | Time: {}us",
                        std::string(pluginName),
                        std::string(placeholderName),
                        type == PlaceholderType::AsyncContext ? "AsyncContext" : "AsyncServer",
                        duration
                    );
                }
                std::promise<std::string> promise;
                promise.set_value(cached->result);
                return promise.get_future();
            }
        }
    }

    // --- 3. Execute Placeholder ---
    std::shared_ptr<Utils::ParsedParams> paramsPtr;
    if (auto cachedParams = mParamsCache.get(paramString)) {
        paramsPtr = *cachedParams;
    } else {
        paramsPtr = std::make_shared<Utils::ParsedParams>(paramString);
        mParamsCache.put(paramString, paramsPtr);
    }

    std::future<std::string> future;
    if (bestAsyncCandidate) {
        const auto& params = *paramsPtr;
        void*       p      = ctx.ptr;
        for (auto& cfun : bestAsyncCandidate->chain) {
            p = cfun(p);
        }
        if (p) {
            if (std::holds_alternative<AsyncAnyPtrReplacer>(bestAsyncCandidate->fn)) {
                future = std::get<AsyncAnyPtrReplacer>(bestAsyncCandidate->fn)(p);
            } else {
                future = std::get<AsyncAnyPtrReplacerWithParams>(bestAsyncCandidate->fn)(p, params);
            }
        } else {
            std::promise<std::string> p_promise;
            p_promise.set_value("");
            future = p_promise.get_future();
        }
    } else if (asyncServerEntry) {
        const auto& params = *paramsPtr;
        if (std::holds_alternative<AsyncServerReplacer>(asyncServerEntry->fn)) {
            future = std::get<AsyncServerReplacer>(asyncServerEntry->fn)();
        } else {
            future = std::get<AsyncServerReplacerWithParams>(asyncServerEntry->fn)(params);
        }
    } else {
        type = PlaceholderType::SyncFallback;
        std::promise<std::string> promise;
        ReplaceState              st;
        promise.set_value(
            executePlaceholder(pluginName, placeholderName, paramString, defaultText, ctx, st, cache_duration_override)
        );
        return promise.get_future();
    }

    // --- 4. Finalize & Cache (for async paths) ---
    const auto& params     = *paramsPtr;
    bool        allowEmpty = params.getBool("allowempty").value_or(false);

    return mCombinerThreadPool->enqueue([this,
                                         startTime,
                                         type,
                                         fut             = std::move(future),
                                         pluginName      = std::string(pluginName),
                                         placeholderName = std::string(placeholderName),
                                         paramString,
                                         defaultText,
                                         allowEmpty,
                                         cacheKey,
                                         cacheDuration,
                                         paramsPtr]() mutable {
        auto timeout = std::chrono::milliseconds(ConfigManager::getInstance().get().asyncPlaceholderTimeoutMs);
        if (fut.wait_for(timeout) == std::future_status::timeout) {
            if (ConfigManager::getInstance().get().debugMode) {
                logger.warn(
                    "Async placeholder '{}:{}' timed out after {}ms. Using default value: '{}'.",
                    pluginName,
                    placeholderName,
                    timeout.count(),
                    defaultText
                );
            }
            return defaultText;
        }

        std::string replaced_val = fut.get();
        bool        replaced     = !replaced_val.empty() || allowEmpty;

        const auto& params = *paramsPtr;
        std::string formatted_val;
        if (replaced) {
            formatted_val = PA::Utils::applyFormatting(replaced_val, params);
        }

        std::string finalOut;
        if (replaced && (!formatted_val.empty() || allowEmpty)) {
            finalOut = formatted_val;
        } else {
            finalOut = defaultText;
        }

        if (ConfigManager::getInstance().get().debugMode) {
            auto duration =
                std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - startTime)
                    .count();
            const char* typeStr = "Unknown";
            if (type == PlaceholderType::Relational) typeStr = "Relational";
            else if (type == PlaceholderType::Context) typeStr = "Context";
            else if (type == PlaceholderType::Server) typeStr = "Server";
            else if (type == PlaceholderType::ListContext) typeStr = "ListContext";
            else if (type == PlaceholderType::ListServer) typeStr = "ListServer";
            else if (type == PlaceholderType::ObjectListContext) typeStr = "ObjectListContext";
            else if (type == PlaceholderType::ObjectListServer) typeStr = "ObjectListServer";
            else if (type == PlaceholderType::AsyncContext) typeStr = "AsyncContext";
            else if (type == PlaceholderType::AsyncServer) typeStr = "AsyncServer";
            else if (type == PlaceholderType::SyncFallback) typeStr = "SyncFallback";

            logger.info(
                "Placeholder '{}:{}' | Cache Hit: No | Type: {} | Time: {}us",
                pluginName,
                placeholderName,
                typeStr,
                duration
            );
        }

        if (finalOut.empty() && defaultText.empty() && !replaced && ConfigManager::getInstance().get().debugMode) {
            finalOut.reserve(pluginName.size() + placeholderName.size() + paramString.size() + 4);
            finalOut.append("{").append(pluginName).append(":").append(placeholderName);
            if (!paramString.empty()) finalOut.append("|").append(paramString);
            finalOut.append("}");
        }

        if (replaced && cacheDuration) {
            mGlobalCache.put(cacheKey, {finalOut, std::chrono::steady_clock::now() + *cacheDuration});
        }
        return finalOut;
    });
}

// [新] 使用编译模板进行替换
std::string PlaceholderManager::replacePlaceholders(const CompiledTemplate& tpl, const PlaceholderContext& ctx) {
    return replacePlaceholdersSync(tpl, ctx, 0);
}

} // namespace PA
