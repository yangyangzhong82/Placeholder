#pragma once

// 引入宏定义，例如 PA_API 用于导出符号
#include "Macros.h"
// 引入工具类，例如参数解析
#include "Utils.h"

// 标准库头文件
#include <any>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>


#include "LRUCache.h" // 引入 LRU 缓存
#include "PlaceholderRegistry.h"
#include "PlaceholderTypeSystem.h"

class Player; // 前向声明 Minecraft 玩家类，避免循环引用

namespace PA { // PlaceholderAPI 命名空间

class ThreadPool; // 前向声明线程池类

namespace Utils {
class ParsedParams; // 前向声明参数解析类
} // namespace Utils

/**
 * @brief 提供一个编译期稳定的“类型键”字符串（跨插件一致，且不依赖 RTTI）
 */
template <typename T>
inline std::string typeKey() {
#if defined(_MSC_VER)
    return __FUNCSIG__; // MSVC 编译器特有的函数签名宏
#else
    return __PRETTY_FUNCTION__; // GCC/Clang 编译器特有的函数签名宏
#endif
}

/**
 * @brief 占位符上下文：携带对象指针与其“动态类型ID”
 */
struct PlaceholderContext {
    void*       ptr{nullptr}; // 指向上下文对象的通用指针
    std::size_t typeId{0};    // 上下文对象的内部类型ID，0 表示无类型

    // 新增：关系型上下文
    const PlaceholderContext* relationalContext{nullptr};
};

class PlaceholderManager;

// --- 新：模板编译系统 ---
struct CompiledTemplate;

struct LiteralToken {
    std::string_view text;
};

struct PlaceholderToken {
    std::string_view                  pluginName;
    std::string_view                  placeholderName;
    std::unique_ptr<CompiledTemplate> defaultTemplate;
    std::unique_ptr<CompiledTemplate> paramsTemplate;
};

using Token = std::variant<LiteralToken, PlaceholderToken>;

using CacheKeyStrategy = PlaceholderRegistry::CacheKeyStrategy;

struct CompiledTemplate {
    std::string        source;
    std::vector<Token> tokens;

    CompiledTemplate();
    ~CompiledTemplate();
    CompiledTemplate(CompiledTemplate&&) noexcept;
    CompiledTemplate& operator=(CompiledTemplate&&) noexcept;
};


class PlaceholderManager {
public:
    using ServerReplacer                 = PlaceholderRegistry::ServerReplacer;
    using ServerReplacerWithParams       = PlaceholderRegistry::ServerReplacerWithParams;
    using CacheDuration                  = PlaceholderRegistry::CacheDuration;
    using AsyncServerReplacer            = PlaceholderRegistry::AsyncServerReplacer;
    using AsyncServerReplacerWithParams  = PlaceholderRegistry::AsyncServerReplacerWithParams;
    using AsyncAnyPtrReplacer            = PlaceholderRegistry::AsyncAnyPtrReplacer;
    using AsyncAnyPtrReplacerWithParams  = PlaceholderRegistry::AsyncAnyPtrReplacerWithParams;
    using AnyPtrRelationalReplacer       = PlaceholderRegistry::AnyPtrRelationalReplacer;
    using AnyPtrRelationalReplacerWithParams = PlaceholderRegistry::AnyPtrRelationalReplacerWithParams;
    using AnyPtrReplacer                 = PlaceholderRegistry::AnyPtrReplacer;
    using AnyPtrReplacerWithParams       = PlaceholderRegistry::AnyPtrReplacerWithParams;
    using ServerListReplacer             = PlaceholderRegistry::ServerListReplacer;
    using ServerListReplacerWithParams   = PlaceholderRegistry::ServerListReplacerWithParams;
    using AnyPtrListReplacer             = PlaceholderRegistry::AnyPtrListReplacer;
    using AnyPtrListReplacerWithParams   = PlaceholderRegistry::AnyPtrListReplacerWithParams;
    using ServerObjectListReplacer       = PlaceholderRegistry::ServerObjectListReplacer;
    using ServerObjectListReplacerWithParams = PlaceholderRegistry::ServerObjectListReplacerWithParams;
    using AnyPtrObjectListReplacer       = PlaceholderRegistry::AnyPtrObjectListReplacer;
    using AnyPtrObjectListReplacerWithParams = PlaceholderRegistry::AnyPtrObjectListReplacerWithParams;
    using Caster                         = PlaceholderTypeSystem::Caster;

    PA_API static PlaceholderManager& getInstance();

    PA_API void registerServerPlaceholder(
        const std::string&           pluginName,
        const std::string&           placeholder,
        ServerReplacer               replacer,
        std::optional<CacheDuration> cache_duration = std::nullopt,
        CacheKeyStrategy             strategy       = CacheKeyStrategy::Default
    );

    PA_API void registerServerPlaceholderWithParams(
        const std::string&           pluginName,
        const std::string&           placeholder,
        ServerReplacerWithParams&&   replacer,
        std::optional<CacheDuration> cache_duration = std::nullopt,
        CacheKeyStrategy             strategy       = CacheKeyStrategy::Default
    );

    PA_API void registerAsyncServerPlaceholder(
        const std::string&           pluginName,
        const std::string&           placeholder,
        AsyncServerReplacer&&        replacer,
        std::optional<CacheDuration> cache_duration = std::nullopt,
        CacheKeyStrategy             strategy       = CacheKeyStrategy::Default
    );

    PA_API void registerAsyncServerPlaceholderWithParams(
        const std::string&              pluginName,
        const std::string&              placeholder,
        AsyncServerReplacerWithParams&& replacer,
        std::optional<CacheDuration>    cache_duration = std::nullopt,
        CacheKeyStrategy                strategy       = CacheKeyStrategy::Default
    );

    template <typename T, typename T_Rel>
    void registerRelationalPlaceholder(
        const std::string&                       pluginName,
        const std::string&                       placeholder,
        std::function<std::string(T*, T_Rel*)>&& replacer,
        std::optional<CacheDuration>             cache_duration = std::nullopt,
        CacheKeyStrategy                         strategy       = CacheKeyStrategy::Default
    ) {
        auto                     targetId     = mTypeSystem->ensureTypeId(typeKey<T>());
        auto                     relationalId = mTypeSystem->ensureTypeId(typeKey<T_Rel>());
        AnyPtrRelationalReplacer fn = [r = std::move(replacer)](void* p, void* p_rel) -> std::string {
            if (!p || !p_rel) return std::string{};
            return r(reinterpret_cast<T*>(p), reinterpret_cast<T_Rel*>(p_rel));
        };
        mRegistry->registerRelationalPlaceholderForTypeId(
            pluginName,
            placeholder,
            targetId,
            relationalId,
            std::move(fn),
            cache_duration,
            strategy
        );
    }

    template <typename T, typename T_Rel>
    void registerRelationalPlaceholderWithParams(
        const std::string&                                                   pluginName,
        const std::string&                                                   placeholder,
        std::function<std::string(T*, T_Rel*, const Utils::ParsedParams&)>&& replacer,
        std::optional<CacheDuration>                                         cache_duration = std::nullopt,
        CacheKeyStrategy                                                     strategy       = CacheKeyStrategy::Default
    ) {
        auto targetId     = mTypeSystem->ensureTypeId(typeKey<T>());
        auto relationalId = mTypeSystem->ensureTypeId(typeKey<T_Rel>());
        AnyPtrRelationalReplacerWithParams fn =
            [r = std::move(replacer)](void* p, void* p_rel, const Utils::ParsedParams& params) -> std::string {
            if (!p || !p_rel) return std::string{};
            return r(reinterpret_cast<T*>(p), reinterpret_cast<T_Rel*>(p_rel), params);
        };
        mRegistry->registerRelationalPlaceholderForTypeId(
            pluginName,
            placeholder,
            targetId,
            relationalId,
            std::move(fn),
            cache_duration,
            strategy
        );
    }

    PA_API void registerServerPlaceholderWithParams(
        const std::string&                             pluginName,
        const std::string&                             placeholder,
        std::function<std::string(std::string_view)>&& replacer
    ) {
        ServerReplacerWithParams fn = [r = std::move(replacer)](const Utils::ParsedParams& params) -> std::string {
            return r({});
        };
        registerServerPlaceholderWithParams(pluginName, placeholder, std::move(fn));
    }

    template <typename T>
    void registerPlaceholder(
        const std::string&               pluginName,
        const std::string&               placeholder,
        std::function<std::string(T*)>&& replacer,
        std::optional<CacheDuration>     cache_duration = std::nullopt,
        CacheKeyStrategy                 strategy       = CacheKeyStrategy::Default
    ) {
        auto           targetId = mTypeSystem->ensureTypeId(typeKey<T>());
        AnyPtrReplacer fn       = [r = std::move(replacer)](void* p) -> std::string {
            if (!p) return std::string{};
            return r(reinterpret_cast<T*>(p));
        };
        mRegistry->registerPlaceholderForTypeId(pluginName, placeholder, targetId, std::move(fn), cache_duration, strategy);
    }

    template <typename T>
    void registerAsyncPlaceholder(
        const std::string&                            pluginName,
        const std::string&                            placeholder,
        std::function<std::future<std::string>(T*)>&& replacer,
        std::optional<CacheDuration>                  cache_duration = std::nullopt,
        CacheKeyStrategy                              strategy       = CacheKeyStrategy::Default
    ) {
        auto                targetId = mTypeSystem->ensureTypeId(typeKey<T>());
        AsyncAnyPtrReplacer fn       = [r = std::move(replacer)](void* p) -> std::future<std::string> {
            if (!p) {
                std::promise<std::string> promise;
                promise.set_value({});
                return promise.get_future();
            }
            return r(reinterpret_cast<T*>(p));
        };
        mRegistry->registerAsyncPlaceholderForTypeId(pluginName, placeholder, targetId, std::move(fn), cache_duration, strategy);
    }

    template <typename T>
    [[deprecated]] void registerPlaceholderWithParams(
        const std::string&                                 pluginName,
        const std::string&                                 placeholder,
        std::function<std::string(T*, std::string_view)>&& replacer
    ) {
        auto                     targetId = mTypeSystem->ensureTypeId(typeKey<T>());
        AnyPtrReplacerWithParams fn =
            [r = std::move(replacer)](void* p, const Utils::ParsedParams& params) -> std::string {
            if (!p) return std::string{};
            return r(reinterpret_cast<T*>(p), {});
        };
        mRegistry->registerPlaceholderForTypeId(pluginName, placeholder, targetId, std::move(fn));
    }

    template <typename T>
    void registerPlaceholderWithParams(
        const std::string&                                           pluginName,
        const std::string&                                           placeholder,
        std::function<std::string(T*, const Utils::ParsedParams&)>&& replacer,
        std::optional<CacheDuration>                                 cache_duration = std::nullopt,
        CacheKeyStrategy                                             strategy       = CacheKeyStrategy::Default
    ) {
        auto                     targetId = mTypeSystem->ensureTypeId(typeKey<T>());
        AnyPtrReplacerWithParams fn =
            [r = std::move(replacer)](void* p, const Utils::ParsedParams& params) -> std::string {
            if (!p) return std::string{};
            return r(reinterpret_cast<T*>(p), params);
        };
        mRegistry->registerPlaceholderForTypeId(pluginName, placeholder, targetId, std::move(fn), cache_duration, strategy);
    }

    template <typename T>
    void registerAsyncPlaceholderWithParams(
        const std::string&                                                        pluginName,
        const std::string&                                                        placeholder,
        std::function<std::future<std::string>(T*, const Utils::ParsedParams&)>&& replacer,
        std::optional<CacheDuration>                                              cache_duration = std::nullopt,
        CacheKeyStrategy                                                          strategy       = CacheKeyStrategy::Default
    ) {
        auto                          targetId = mTypeSystem->ensureTypeId(typeKey<T>());
        AsyncAnyPtrReplacerWithParams fn =
            [r = std::move(replacer)](void* p, const Utils::ParsedParams& params) -> std::future<std::string> {
            if (!p) {
                std::promise<std::string> promise;
                promise.set_value({});
                return promise.get_future();
            }
            return r(reinterpret_cast<T*>(p), params);
        };
        mRegistry->registerAsyncPlaceholderForTypeId(pluginName, placeholder, targetId, std::move(fn), cache_duration, strategy);
    }

    PA_API void registerPlaceholderForTypeKey(
        const std::string& pluginName,
        const std::string& placeholder,
        const std::string& typeKeyStr,
        AnyPtrReplacer     replacer
    );

    PA_API void registerPlaceholderForTypeKeyWithParams(
        const std::string&         pluginName,
        const std::string&         placeholder,
        const std::string&         typeKeyStr,
        AnyPtrReplacerWithParams&& replacer
    );

    PA_API void registerRelationalPlaceholderForTypeKey(
        const std::string&           pluginName,
        const std::string&           placeholder,
        const std::string&           typeKeyStr,
        const std::string&           relationalTypeKeyStr,
        AnyPtrRelationalReplacer&&   replacer,
        std::optional<CacheDuration> cache_duration = std::nullopt,
        CacheKeyStrategy             strategy       = CacheKeyStrategy::Default
    );

    PA_API void registerRelationalPlaceholderForTypeKeyWithParams(
        const std::string&                   pluginName,
        const std::string&                   placeholder,
        const std::string&                   typeKeyStr,
        const std::string&                   relationalTypeKeyStr,
        AnyPtrRelationalReplacerWithParams&& replacer,
        std::optional<CacheDuration>         cache_duration = std::nullopt,
        CacheKeyStrategy                     strategy       = CacheKeyStrategy::Default
    );

    PA_API void registerServerObjectListPlaceholder(
        const std::string&               pluginName,
        const std::string&               placeholder,
        ServerObjectListReplacer&&       replacer,
        std::optional<CacheDuration>     cache_duration = std::nullopt,
        CacheKeyStrategy                 strategy       = CacheKeyStrategy::Default
    );

    PA_API void registerServerObjectListPlaceholderWithParams(
        const std::string&                   pluginName,
        const std::string&                   placeholder,
        ServerObjectListReplacerWithParams&& replacer,
        std::optional<CacheDuration>         cache_duration = std::nullopt,
        CacheKeyStrategy                     strategy       = CacheKeyStrategy::Default
    );

    template <typename T>
    void registerObjectListPlaceholder(
        const std::string&                                     pluginName,
        const std::string&                                     placeholder,
        std::function<std::vector<PlaceholderContext>(T*)>&&   replacer,
        std::optional<CacheDuration>                           cache_duration = std::nullopt,
        CacheKeyStrategy                                       strategy       = CacheKeyStrategy::Default
    ) {
        auto                   targetId = mTypeSystem->ensureTypeId(typeKey<T>());
        AnyPtrObjectListReplacer fn     = [r = std::move(replacer)](void* p) -> std::vector<PlaceholderContext> {
            if (!p) return {};
            return r(reinterpret_cast<T*>(p));
        };
        mRegistry->registerObjectListPlaceholderForTypeId(
            pluginName,
            placeholder,
            targetId,
            std::move(fn),
            cache_duration,
            strategy
        );
    }

    template <typename T>
    void registerObjectListPlaceholderWithParams(
        const std::string&                                                           pluginName,
        const std::string&                                                           placeholder,
        std::function<std::vector<PlaceholderContext>(T*, const Utils::ParsedParams&)>&& replacer,
        std::optional<CacheDuration>                                                 cache_duration = std::nullopt,
        CacheKeyStrategy                                                             strategy       = CacheKeyStrategy::Default
    ) {
        auto targetId = mTypeSystem->ensureTypeId(typeKey<T>());
        AnyPtrObjectListReplacerWithParams fn =
            [r = std::move(replacer)](void* p, const Utils::ParsedParams& params) -> std::vector<PlaceholderContext> {
            if (!p) return {};
            return r(reinterpret_cast<T*>(p), params);
        };
        mRegistry->registerObjectListPlaceholderForTypeId(
            pluginName,
            placeholder,
            targetId,
            std::move(fn),
            cache_duration,
            strategy
        );
    }

    PA_API void registerServerListPlaceholder(
        const std::string&           pluginName,
        const std::string&           placeholder,
        ServerListReplacer&&         replacer,
        std::optional<CacheDuration> cache_duration = std::nullopt,
        CacheKeyStrategy             strategy       = CacheKeyStrategy::Default
    );

    PA_API void registerServerListPlaceholderWithParams(
        const std::string&             pluginName,
        const std::string&             placeholder,
        ServerListReplacerWithParams&& replacer,
        std::optional<CacheDuration>   cache_duration = std::nullopt,
        CacheKeyStrategy               strategy       = CacheKeyStrategy::Default
    );

    template <typename T>
    void registerListPlaceholder(
        const std::string&                                   pluginName,
        const std::string&                                   placeholder,
        std::function<std::vector<std::string>(T*)>&&        replacer,
        std::optional<CacheDuration>                         cache_duration = std::nullopt,
        CacheKeyStrategy                                     strategy       = CacheKeyStrategy::Default
    ) {
        auto               targetId = mTypeSystem->ensureTypeId(typeKey<T>());
        AnyPtrListReplacer fn       = [r = std::move(replacer)](void* p) -> std::vector<std::string> {
            if (!p) return {};
            return r(reinterpret_cast<T*>(p));
        };
        mRegistry->registerListPlaceholderForTypeId(pluginName, placeholder, targetId, std::move(fn), cache_duration, strategy);
    }

    template <typename T>
    void registerListPlaceholderWithParams(
        const std::string&                                                       pluginName,
        const std::string&                                                       placeholder,
        std::function<std::vector<std::string>(T*, const Utils::ParsedParams&)>&& replacer,
        std::optional<CacheDuration>                                             cache_duration = std::nullopt,
        CacheKeyStrategy                                                         strategy       = CacheKeyStrategy::Default
    ) {
        auto                         targetId = mTypeSystem->ensureTypeId(typeKey<T>());
        AnyPtrListReplacerWithParams fn =
            [r = std::move(replacer)](void* p, const Utils::ParsedParams& params) -> std::vector<std::string> {
            if (!p) return {};
            return r(reinterpret_cast<T*>(p), params);
        };
        mRegistry->registerListPlaceholderForTypeId(pluginName, placeholder, targetId, std::move(fn), cache_duration, strategy);
    }

    PA_API void registerAsyncPlaceholderForTypeKey(
        const std::string&    pluginName,
        const std::string&    placeholder,
        const std::string&    typeKeyStr,
        AsyncAnyPtrReplacer&& replacer
    );

    PA_API void registerAsyncPlaceholderForTypeKeyWithParams(
        const std::string&              pluginName,
        const std::string&              placeholder,
        const std::string&              typeKeyStr,
        AsyncAnyPtrReplacerWithParams&& replacer
    );

    template <typename Derived, typename Base>
    void registerInheritance() {
        mTypeSystem->registerInheritanceByKeys(
            typeKey<Derived>(),
            typeKey<Base>(),
            +[](void* p) -> void* { return static_cast<Base*>(reinterpret_cast<Derived*>(p)); }
        );
    }

    PA_API void registerInheritanceByKeys(const std::string& derivedKey, const std::string& baseKey, Caster caster);

    using InheritancePair = PlaceholderTypeSystem::InheritancePair;

    PA_API void registerInheritanceByKeysBatch(const std::vector<InheritancePair>& pairs);

    template <typename T>
    void registerTypeAlias(const std::string& alias) {
        mTypeSystem->registerTypeAlias(alias, typeKey<T>());
    }

    PA_API void registerTypeAlias(const std::string& alias, const std::string& typeKeyStr);

    PA_API void unregisterPlaceholders(const std::string& pluginName);

    PA_API void unregisterAsyncPlaceholders(const std::string& pluginName);

    using PlaceholderCategory = PlaceholderRegistry::PlaceholderCategory;
    using PlaceholderInfo     = PlaceholderRegistry::PlaceholderInfo;
    using AllPlaceholders     = PlaceholderRegistry::AllPlaceholders;
    PA_API AllPlaceholders getAllPlaceholders() const;

    PA_API bool hasPlaceholder(
        const std::string&                pluginName,
        const std::string&                placeholderName,
        const std::optional<std::string>& typeKey = std::nullopt
    ) const;

    PA_API std::string replacePlaceholders(const std::string& text);

    PA_API std::string replacePlaceholders(const std::string& text, std::any contextObject);

    PA_API std::string replacePlaceholders(const std::string& text, const PlaceholderContext& ctx);

    PA_API std::string replacePlaceholders(const std::string& text, Player* player);

    PA_API CompiledTemplate compileTemplate(const std::string& text);

    PA_API std::string replacePlaceholders(const CompiledTemplate& tpl, const PlaceholderContext& ctx);

    PA_API std::future<std::string> replacePlaceholdersAsync(const std::string& text, const PlaceholderContext& ctx);

    PA_API std::future<std::string>
    replacePlaceholdersAsync(const CompiledTemplate& tpl, const PlaceholderContext& ctx);

    PA_API std::vector<std::string> replacePlaceholdersBatch(
        const std::vector<std::reference_wrapper<const CompiledTemplate>>& tpls,
        const PlaceholderContext&                                          ctx
    );

    template <typename T>
    std::string replacePlaceholders(const std::string& text, T* obj) {
        return replacePlaceholders(text, makeContext(obj));
    }

    template <typename T>
    PlaceholderContext makeContext(T* ptr, const PlaceholderContext* rel_ctx = nullptr) {
        return makeContextRaw(static_cast<void*>(ptr), typeKey<T>(), rel_ctx);
    }

    PA_API PlaceholderContext
    makeContextRaw(void* ptr, const std::string& typeKeyStr, const PlaceholderContext* rel_ctx = nullptr);

    PA_API std::size_t ensureTypeId(const std::string& typeKeyStr);

    PA_API bool
    findUpcastChain(std::size_t fromTypeId, std::size_t toTypeId, std::vector<Caster>& outChain) const;

    PA_API void clearCache();

    PA_API void clearCache(const std::string& pluginName);

    PA_API void setMaxRecursionDepth(int depth);
    PA_API int getMaxRecursionDepth() const;
    PA_API void setDoubleBraceEscape(bool enable);
    PA_API bool getDoubleBraceEscape() const;

private:
    std::string buildCacheKey(
        const PlaceholderContext& ctx,
        std::string_view          pluginName,
        std::string_view          placeholderName,
        const std::string&        paramString,
        CacheKeyStrategy          strategy
    );
    using ReplacerMatch   = PlaceholderRegistry::ReplacerMatch;
    using PlaceholderType = PlaceholderRegistry::PlaceholderType;

    struct CacheEntry {
        std::string                           result;
        std::chrono::steady_clock::time_point expiresAt;
    };

    std::shared_ptr<PlaceholderTypeSystem> mTypeSystem;
    std::shared_ptr<PlaceholderRegistry>   mRegistry;

    mutable LRUCache<std::string, CacheEntry> mGlobalCache;

    mutable LRUCache<std::string, std::shared_ptr<CompiledTemplate>> mCompileCache;

    mutable LRUCache<std::string, std::shared_ptr<Utils::ParsedParams>> mParamsCache;

    std::unique_ptr<ThreadPool> mCombinerThreadPool;
    std::unique_ptr<ThreadPool> mAsyncThreadPool;

    // For request coalescing
    mutable std::mutex                                                  mFuturesMutex;
    mutable std::unordered_map<std::string, std::shared_future<std::string>> mComputingFutures;

    int  mMaxRecursionDepth{12};
    bool mEnableDoubleBraceEscape{true};

private:
    PlaceholderManager();
    ~PlaceholderManager() = default;

    PlaceholderManager(const PlaceholderManager&)            = delete;
    PlaceholderManager& operator=(const PlaceholderManager&) = delete;

    struct ReplaceState {
        int                                          depth{0};
        std::unordered_map<std::string, std::string> cache;
    };
    // 为长字符串生成哈希作为缓存键的一部分
    static std::string hashString(const std::string& str) {
        if (str.size() <= 128) {
            return str; // 短字符串直接使用
        }
        // 对长字符串使用哈希
        std::hash<std::string> hasher;
        return "hash:" + std::to_string(hasher(str));
    }
    std::string replacePlaceholdersSync(const CompiledTemplate& tpl, const PlaceholderContext& ctx, int depth);

    std::shared_ptr<Utils::ParsedParams> getParsedParams(const std::string& paramString);

    std::string executeFoundReplacer(
        const ReplacerMatch&       match,
        void*                      p,
        void*                      p_rel,
        const Utils::ParsedParams& params,
        bool                       allowEmpty,
        ReplaceState&              st
    );

    std::string applyFormattingAndCache(
        const std::string&                   originalResult,
        const Utils::ParsedParams&           params,
        const std::string&                   defaultText,
        bool                                 allowEmpty,
        const std::string&                   cacheKey,
        std::optional<CacheDuration>         cacheDuration,
        PlaceholderType                      type,
        std::chrono::steady_clock::time_point startTime,
        ReplaceState&                        st,
        std::string_view                     pluginName,
        std::string_view                     placeholderName,
        const std::string&                   paramString,
        const PlaceholderContext&            ctx,
        bool                                 replaced
    );

    std::string executePlaceholder(
        std::string_view             pluginName,
        std::string_view             placeholderName,
        const std::string&           paramString,
        const std::string&           defaultText,
        const PlaceholderContext&    ctx,
        ReplaceState&                st,
        std::optional<CacheDuration> cache_duration_override = std::nullopt
    );

    std::future<std::string> executePlaceholderAsync(
        std::string_view             pluginName,
        std::string_view             placeholderName,
        const std::string&           paramString,
        const std::string&           defaultText,
        const PlaceholderContext&    ctx,
        std::optional<CacheDuration> cache_duration_override = std::nullopt
    );
};

} // namespace PA
