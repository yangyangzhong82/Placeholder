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
 * @brief 提供一个跨 DLL 稳定的“类型键”（不依赖 RTTI）
 *        做法：从编译器的函数签名里提取类型名核心，去掉 class/struct/enum 等噪声。
 */
template <typename T>
inline std::string typeKey() {
#if defined(_MSC_VER)
    std::string_view sig = __FUNCSIG__;
    // 形如：std::string __cdecl PA::typeKey<struct foo::Bar>(void)
    auto             lb   = sig.find('<');
    auto             rb   = sig.rfind('>');
    std::string_view core = (lb != std::string_view::npos && rb != std::string_view::npos && rb > lb)
                              ? sig.substr(lb + 1, rb - lb - 1)
                              : sig;
#else
    std::string_view sig = __PRETTY_FUNCTION__;
    // 形如：std::string PA::typeKey() [with T = foo::Bar]
    constexpr std::string_view marker = "T = ";
    auto                       pos    = sig.find(marker);
    auto                       end    = (pos == std::string_view::npos) ? std::string_view::npos : sig.find(']', pos);
    std::string_view           core   = (pos != std::string_view::npos && end != std::string_view::npos)
                                          ? sig.substr(pos + marker.size(), end - (pos + marker.size()))
                                          : sig;
#endif

    // 规范化：去掉 class/struct/enum 前缀，保留其余语义字符
    std::string out;
    out.reserve(core.size());
    for (size_t i = 0; i < core.size();) {
        if (core.compare(i, 6, "class ") == 0) {
            i += 6;
            continue;
        }
        if (core.compare(i, 7, "struct ") == 0) {
            i += 7;
            continue;
        }
        if (core.compare(i, 5, "enum ") == 0) {
            i += 5;
            continue;
        }
#if defined(_MSC_VER)
        // MSVC 偶尔会在类型名里插入 __ptr64 等 ABI 噪声
        if (core.compare(i, 7, "__ptr64") == 0) {
            i += 7;
            continue;
        }
#endif
        out.push_back(core[i++]);
    }
    return out;
}

/**
 * @brief 占位符上下文：携带对象指针与其“动态类型ID”
 */
struct PlaceholderContext {
    void*       ptr{nullptr}; // 指向上下文对象的通用指针
    std::size_t typeId{0};    // 上下文对象的内部类型ID，0 表示无类型

    // 新增：关系型上下文，用于处理需要两个对象关系的占位符
    const PlaceholderContext* relationalContext{nullptr};
};

class PlaceholderManager;

// --- 新：模板编译系统 ---

/**
 * @brief 编译后的模板，用于高效地替换占位符
 */
struct CompiledTemplate;

/**
 * @brief 表示模板中的字面量文本部分
 */
struct LiteralToken {
    std::string_view text; // 文本内容
};

/**
 * @brief 表示模板中的一个占位符部分
 */
struct PlaceholderToken {
    std::string_view                  pluginName;      // 插件名
    std::string_view                  placeholderName; // 占位符名
    std::unique_ptr<CompiledTemplate> defaultTemplate; // 默认值模板（如果存在）
    std::unique_ptr<CompiledTemplate> paramsTemplate;  // 参数模板（如果存在）
};

/**
 * @brief Token 是字面量或占位符的变体类型
 */
using Token = std::variant<LiteralToken, PlaceholderToken>;

/**
 * @brief 缓存键生成策略
 */
using CacheKeyStrategy = PlaceholderRegistry::CacheKeyStrategy;

/**
 * @brief 编译后的模板结构体定义
 */
struct CompiledTemplate {
    std::string        source; // 原始模板字符串
    std::vector<Token> tokens; // 解析后的 Token 序列

    // 移动构造和赋值函数，支持 unique_ptr 的所有权转移
    CompiledTemplate();
    ~CompiledTemplate();
    CompiledTemplate(CompiledTemplate&&) noexcept;
    CompiledTemplate& operator=(CompiledTemplate&&) noexcept;
};


/**
 * @class PlaceholderManager
 * @brief 占位符 API 的核心管理类，负责注册、解析和替换占位符。
 *
 * 这是一个单例类，通过 getInstance() 获取实例。
 * 它整合了类型系统（PlaceholderTypeSystem）、注册表（PlaceholderRegistry）和缓存机制，
 * 提供了同步和异步的占位符处理能力。
 */
class PlaceholderManager {
public:
    // --- 类型别名定义，方便外部使用 ---
    using ServerReplacer                     = PlaceholderRegistry::ServerReplacer;
    using ServerReplacerWithParams           = PlaceholderRegistry::ServerReplacerWithParams;
    using CacheDuration                      = PlaceholderRegistry::CacheDuration;
    using AsyncServerReplacer                = PlaceholderRegistry::AsyncServerReplacer;
    using AsyncServerReplacerWithParams      = PlaceholderRegistry::AsyncServerReplacerWithParams;
    using AsyncAnyPtrReplacer                = PlaceholderRegistry::AsyncAnyPtrReplacer;
    using AsyncAnyPtrReplacerWithParams      = PlaceholderRegistry::AsyncAnyPtrReplacerWithParams;
    using AnyPtrRelationalReplacer           = PlaceholderRegistry::AnyPtrRelationalReplacer;
    using AnyPtrRelationalReplacerWithParams = PlaceholderRegistry::AnyPtrRelationalReplacerWithParams;
    using AnyPtrReplacer                     = PlaceholderRegistry::AnyPtrReplacer;
    using AnyPtrReplacerWithParams           = PlaceholderRegistry::AnyPtrReplacerWithParams;
    using ServerListReplacer                 = PlaceholderRegistry::ServerListReplacer;
    using ServerListReplacerWithParams       = PlaceholderRegistry::ServerListReplacerWithParams;
    using AnyPtrListReplacer                 = PlaceholderRegistry::AnyPtrListReplacer;
    using AnyPtrListReplacerWithParams       = PlaceholderRegistry::AnyPtrListReplacerWithParams;
    using ServerObjectListReplacer           = PlaceholderRegistry::ServerObjectListReplacer;
    using ServerObjectListReplacerWithParams = PlaceholderRegistry::ServerObjectListReplacerWithParams;
    using AnyPtrObjectListReplacer           = PlaceholderRegistry::AnyPtrObjectListReplacer;
    using AnyPtrObjectListReplacerWithParams = PlaceholderRegistry::AnyPtrObjectListReplacerWithParams;
    using Caster                             = PlaceholderTypeSystem::Caster;

    /**
     * @brief 获取 PlaceholderManager 的单例实例
     * @return PlaceholderManager 的引用
     */
    PA_API static PlaceholderManager& getInstance();

    // --- 占位符注册接口 ---

    /**
     * @brief 注册一个服务器范围的占位符（无参数）
     * @param pluginName 插件名
     * @param placeholder 占位符标识符
     * @param replacer 替换函数
     * @param cache_duration 缓存持续时间（可选）
     * @param strategy 缓存键策略（可选）
     */
    PA_API void registerServerPlaceholder(
        const std::string&           pluginName,
        const std::string&           placeholder,
        ServerReplacer               replacer,
        std::optional<CacheDuration> cache_duration = std::nullopt,
        CacheKeyStrategy             strategy       = CacheKeyStrategy::Default
    );

    /**
     * @brief 注册一个带参数的服务器范围占位符
     * @param pluginName 插件名
     * @param placeholder 占位符标识符
     * @param replacer 替换函数
     * @param cache_duration 缓存持续时间（可选）
     * @param strategy 缓存键策略（可选）
     */
    PA_API void registerServerPlaceholderWithParams(
        const std::string&           pluginName,
        const std::string&           placeholder,
        ServerReplacerWithParams&&   replacer,
        std::optional<CacheDuration> cache_duration = std::nullopt,
        CacheKeyStrategy             strategy       = CacheKeyStrategy::Default
    );

    /**
     * @brief 注册一个异步的服务器范围占位符（无参数）
     * @param pluginName 插件名
     * @param placeholder 占位符标识符
     * @param replacer 异步替换函数，返回 std::future<std::string>
     * @param cache_duration 缓存持续时间（可选）
     * @param strategy 缓存键策略（可选）
     */
    PA_API void registerAsyncServerPlaceholder(
        const std::string&           pluginName,
        const std::string&           placeholder,
        AsyncServerReplacer&&        replacer,
        std::optional<CacheDuration> cache_duration = std::nullopt,
        CacheKeyStrategy             strategy       = CacheKeyStrategy::Default
    );

    /**
     * @brief 注册一个带参数的异步服务器范围占位符
     * @param pluginName 插件名
     * @param placeholder 占位符标识符
     * @param replacer 异步替换函数
     * @param cache_duration 缓存持续时间（可选）
     * @param strategy 缓存键策略（可选）
     */
    PA_API void registerAsyncServerPlaceholderWithParams(
        const std::string&              pluginName,
        const std::string&              placeholder,
        AsyncServerReplacerWithParams&& replacer,
        std::optional<CacheDuration>    cache_duration = std::nullopt,
        CacheKeyStrategy                strategy       = CacheKeyStrategy::Default
    );

    /**
     * @brief 注册一个关系型占位符（需要两个上下文对象）
     * @tparam T 主上下文对象的类型
     * @tparam T_Rel 关系上下文对象的类型
     * @param pluginName 插件名
     * @param placeholder 占位符标识符
     * @param replacer 替换函数，接受两个类型的指针
     * @param cache_duration 缓存持续时间（可选）
     * @param strategy 缓存键策略（可选）
     */
    template <typename T, typename T_Rel>
    void registerRelationalPlaceholder(
        const std::string&                       pluginName,
        const std::string&                       placeholder,
        std::function<std::string(T*, T_Rel*)>&& replacer,
        std::optional<CacheDuration>             cache_duration = std::nullopt,
        CacheKeyStrategy                         strategy       = CacheKeyStrategy::Default
    ) {
        static_assert(
            !std::is_same_v<T, void> && !std::is_same_v<T_Rel, void>,
            "void* is not allowed as a type argument for type-safe placeholder registration."
        );
        auto targetId     = mTypeSystem->ensureTypeId(typeKey<T>());
        auto relationalId = mTypeSystem->ensureTypeId(typeKey<T_Rel>());

        AnyPtrRelationalReplacer fn = [r = std::move(replacer)](void* p, void* p_rel) -> std::string {
            if (!p || !p_rel) return std::string{};
            return r(static_cast<T*>(p), static_cast<T_Rel*>(p_rel));
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

    /**
     * @brief 注册一个带参数的关系型占位符
     * @tparam T 主上下文对象的类型
     * @tparam T_Rel 关系上下文对象的类型
     * @param pluginName 插件名
     * @param placeholder 占位符标识符
     * @param replacer 替换函数，接受两个类型的指针和参数
     * @param cache_duration 缓存持续时间（可选）
     * @param strategy 缓存键策略（可选）
     */
    template <typename T, typename T_Rel>
    void registerRelationalPlaceholderWithParams(
        const std::string&                                                   pluginName,
        const std::string&                                                   placeholder,
        std::function<std::string(T*, T_Rel*, const Utils::ParsedParams&)>&& replacer,
        std::optional<CacheDuration>                                         cache_duration = std::nullopt,
        CacheKeyStrategy                                                     strategy       = CacheKeyStrategy::Default
    ) {
        static_assert(
            !std::is_same_v<T, void> && !std::is_same_v<T_Rel, void>,
            "void* is not allowed as a type argument for type-safe placeholder registration."
        );
        auto                               targetId     = mTypeSystem->ensureTypeId(typeKey<T>());
        auto                               relationalId = mTypeSystem->ensureTypeId(typeKey<T_Rel>());
        AnyPtrRelationalReplacerWithParams fn =
            [r = std::move(replacer)](void* p, void* p_rel, const Utils::ParsedParams& params) -> std::string {
            if (!p || !p_rel) return std::string{};
            return r(static_cast<T*>(p), static_cast<T_Rel*>(p_rel), params);
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

    /**
     * @brief 注册服务器占位符的简化版本（兼容旧API）
     */
    PA_API void registerServerPlaceholderWithParams(
        const std::string&                             pluginName,
        const std::string&                             placeholder,
        std::function<std::string(std::string_view)>&& replacer
    );

    /**
     * @brief 注册一个特定类型的占位符（无参数）
     * @tparam T 上下文对象的类型
     * @param pluginName 插件名
     * @param placeholder 占位符标识符
     * @param replacer 替换函数
     * @param cache_duration 缓存持续时间（可选）
     * @param strategy 缓存键策略（可选）
     */
    template <typename T>
    void registerPlaceholder(
        const std::string&               pluginName,
        const std::string&               placeholder,
        std::function<std::string(T*)>&& replacer,
        std::optional<CacheDuration>     cache_duration = std::nullopt,
        CacheKeyStrategy                 strategy       = CacheKeyStrategy::Default
    ) {
        static_assert(
            !std::is_same_v<T, void>,
            "void* is not allowed as a type argument for type-safe placeholder registration."
        );
        auto           targetId = mTypeSystem->ensureTypeId(typeKey<T>());
        AnyPtrReplacer fn       = [r = std::move(replacer)](void* p) -> std::string {
            if (!p) return std::string{};
            return r(static_cast<T*>(p));
        };
        mRegistry
            ->registerPlaceholderForTypeId(pluginName, placeholder, targetId, std::move(fn), cache_duration, strategy);
    }

    /**
     * @brief 注册一个特定类型的异步占位符（无参数）
     * @tparam T 上下文对象的类型
     * @param pluginName 插件名
     * @param placeholder 占位符标识符
     * @param replacer 异步替换函数
     * @param cache_duration 缓存持续时间（可选）
     * @param strategy 缓存键策略（可选）
     */
    template <typename T>
    void registerAsyncPlaceholder(
        const std::string&                            pluginName,
        const std::string&                            placeholder,
        std::function<std::future<std::string>(T*)>&& replacer,
        std::optional<CacheDuration>                  cache_duration = std::nullopt,
        CacheKeyStrategy                              strategy       = CacheKeyStrategy::Default
    ) {
        static_assert(
            !std::is_same_v<T, void>,
            "void* is not allowed as a type argument for type-safe placeholder registration."
        );
        auto                targetId = mTypeSystem->ensureTypeId(typeKey<T>());
        AsyncAnyPtrReplacer fn       = [r = std::move(replacer)](void* p) -> std::future<std::string> {
            if (!p) {
                std::promise<std::string> promise;
                promise.set_value({});
                return promise.get_future();
            }
            return r(static_cast<T*>(p));
        };
        mRegistry->registerAsyncPlaceholderForTypeId(
            pluginName,
            placeholder,
            targetId,
            std::move(fn),
            cache_duration,
            strategy
        );
    }

    /**
     * @brief 注册带参数占位符的简化版本（已废弃）
     */
    template <typename T>
    [[deprecated]] void registerPlaceholderWithParams(
        const std::string&                                 pluginName,
        const std::string&                                 placeholder,
        std::function<std::string(T*, std::string_view)>&& replacer
    )
    {
        static_assert(
            !std::is_same_v<T, void>,
            "void* is not allowed as a type argument for type-safe placeholder registration."
        );
        auto                     targetId = mTypeSystem->ensureTypeId(typeKey<T>());
        AnyPtrReplacerWithParams fn =
            [r = std::move(replacer)](void* p, const Utils::ParsedParams& params) -> std::string {
            if (!p) return std::string{};
            return r(static_cast<T*>(p), {});
        };
        mRegistry->registerPlaceholderForTypeId(pluginName, placeholder, targetId, std::move(fn));
    }
    /**
     * @brief 注册一个特定类型的带参数占位符
     * @tparam T 上下文对象的类型
     * @param pluginName 插件名
     * @param placeholder 占位符标识符
     * @param replacer 替换函数
     * @param cache_duration 缓存持续时间（可选）
     * @param strategy 缓存键策略（可选）
     */
    template <typename T>
    void registerPlaceholderWithParams(
        const std::string&                                           pluginName,
        const std::string&                                           placeholder,
        std::function<std::string(T*, const Utils::ParsedParams&)>&& replacer,
        std::optional<CacheDuration>                                 cache_duration = std::nullopt,
        CacheKeyStrategy                                             strategy       = CacheKeyStrategy::Default
    ) {
        static_assert(
            !std::is_same_v<T, void>,
            "void* is not allowed as a type argument for type-safe placeholder registration."
        );
        auto                     targetId = mTypeSystem->ensureTypeId(typeKey<T>());
        AnyPtrReplacerWithParams fn =
            [r = std::move(replacer)](void* p, const Utils::ParsedParams& params) -> std::string {
            if (!p) return std::string{};
            return r(static_cast<T*>(p), params);
        };
        mRegistry
            ->registerPlaceholderForTypeId(pluginName, placeholder, targetId, std::move(fn), cache_duration, strategy);
    }

    /**
     * @brief 注册一个特定类型的带参数异步占位符
     * @tparam T 上下文对象的类型
     * @param pluginName 插件名
     * @param placeholder 占位符标识符
     * @param replacer 异步替换函数
     * @param cache_duration 缓存持续时间（可选）
     * @param strategy 缓存键策略（可选）
     */
    template <typename T>
    void registerAsyncPlaceholderWithParams(
        const std::string&                                                        pluginName,
        const std::string&                                                        placeholder,
        std::function<std::future<std::string>(T*, const Utils::ParsedParams&)>&& replacer,
        std::optional<CacheDuration>                                              cache_duration = std::nullopt,
        CacheKeyStrategy                                                          strategy = CacheKeyStrategy::Default
    ) {
        static_assert(
            !std::is_same_v<T, void>,
            "void* is not allowed as a type argument for type-safe placeholder registration."
        );
        auto                          targetId = mTypeSystem->ensureTypeId(typeKey<T>());
        AsyncAnyPtrReplacerWithParams fn =
            [r = std::move(replacer)](void* p, const Utils::ParsedParams& params) -> std::future<std::string> {
            if (!p) {
                std::promise<std::string> promise;
                promise.set_value({});
                return promise.get_future();
            }
            return r(static_cast<T*>(p), params);
        };
        mRegistry->registerAsyncPlaceholderForTypeId(
            pluginName,
            placeholder,
            targetId,
            std::move(fn),
            cache_duration,
            strategy
        );
    }

    // --- 已废弃的非模板注册接口 ---
    [[deprecated("Use the type-safe template overload, e.g., registerPlaceholder<T>(...), instead.")]] PA_API void
    registerPlaceholderForTypeKey(
        const std::string& pluginName,
        const std::string& placeholder,
        const std::string& typeKeyStr,
        AnyPtrReplacer     replacer
    );

    [[deprecated("Use the type-safe template overload, e.g., registerPlaceholderWithParams<T>(...), instead."
    )]] PA_API void
    registerPlaceholderForTypeKeyWithParams(
        const std::string&         pluginName,
        const std::string&         placeholder,
        const std::string&         typeKeyStr,
        AnyPtrReplacerWithParams&& replacer
    );

    [[deprecated("Use the type-safe template overload, e.g., registerRelationalPlaceholder<T, T_Rel>(...), instead."
    )]] PA_API void
    registerRelationalPlaceholderForTypeKey(
        const std::string&           pluginName,
        const std::string&           placeholder,
        const std::string&           typeKeyStr,
        const std::string&           relationalTypeKeyStr,
        AnyPtrRelationalReplacer&&   replacer,
        std::optional<CacheDuration> cache_duration = std::nullopt,
        CacheKeyStrategy             strategy       = CacheKeyStrategy::Default
    );

    [[deprecated(
        "Use the type-safe template overload, e.g., registerRelationalPlaceholderWithParams<T, T_Rel>(...), instead."
    )]] PA_API void
    registerRelationalPlaceholderForTypeKeyWithParams(
        const std::string&                   pluginName,
        const std::string&                   placeholder,
        const std::string&                   typeKeyStr,
        const std::string&                   relationalTypeKeyStr,
        AnyPtrRelationalReplacerWithParams&& replacer,
        std::optional<CacheDuration>         cache_duration = std::nullopt,
        CacheKeyStrategy                     strategy       = CacheKeyStrategy::Default
    );

    // --- 列表和对象列表占位符注册 ---

    /**
     * @brief 注册一个返回对象列表的服务器占位符
     * @param pluginName 插件名
     * @param placeholder 占位符标识符
     * @param replacer 替换函数，返回一个 PlaceholderContext 向量
     * @param cache_duration 缓存持续时间（可选）
     * @param strategy 缓存键策略（可选）
     */
    PA_API void registerServerObjectListPlaceholder(
        const std::string&           pluginName,
        const std::string&           placeholder,
        ServerObjectListReplacer&&   replacer,
        std::optional<CacheDuration> cache_duration = std::nullopt,
        CacheKeyStrategy             strategy       = CacheKeyStrategy::Default
    );

    /**
     * @brief 注册一个带参数、返回对象列表的服务器占位符
     */
    PA_API void registerServerObjectListPlaceholderWithParams(
        const std::string&                   pluginName,
        const std::string&                   placeholder,
        ServerObjectListReplacerWithParams&& replacer,
        std::optional<CacheDuration>         cache_duration = std::nullopt,
        CacheKeyStrategy                     strategy       = CacheKeyStrategy::Default
    );

    /**
     * @brief 注册一个特定类型、返回对象列表的占位符
     * @tparam T 上下文对象类型
     */
    template <typename T>
    void registerObjectListPlaceholder(
        const std::string&                                   pluginName,
        const std::string&                                   placeholder,
        std::function<std::vector<PlaceholderContext>(T*)>&& replacer,
        std::optional<CacheDuration>                         cache_duration = std::nullopt,
        CacheKeyStrategy                                     strategy       = CacheKeyStrategy::Default
    ) {
        static_assert(
            !std::is_same_v<T, void>,
            "void* is not allowed as a type argument for type-safe placeholder registration."
        );
        auto                     targetId = mTypeSystem->ensureTypeId(typeKey<T>());
        AnyPtrObjectListReplacer fn       = [r = std::move(replacer)](void* p) -> std::vector<PlaceholderContext> {
            if (!p) return {};
            return r(static_cast<T*>(p));
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

    /**
     * @brief 注册一个特定类型、带参数、返回对象列表的占位符
     * @tparam T 上下文对象类型
     */
    template <typename T>
    void registerObjectListPlaceholderWithParams(
        const std::string&                                                               pluginName,
        const std::string&                                                               placeholder,
        std::function<std::vector<PlaceholderContext>(T*, const Utils::ParsedParams&)>&& replacer,
        std::optional<CacheDuration>                                                     cache_duration = std::nullopt,
        CacheKeyStrategy strategy = CacheKeyStrategy::Default
    ) {
        static_assert(
            !std::is_same_v<T, void>,
            "void* is not allowed as a type argument for type-safe placeholder registration."
        );
        auto                               targetId = mTypeSystem->ensureTypeId(typeKey<T>());
        AnyPtrObjectListReplacerWithParams fn =
            [r = std::move(replacer)](void* p, const Utils::ParsedParams& params) -> std::vector<PlaceholderContext> {
            if (!p) return {};
            return r(static_cast<T*>(p), params);
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

    /**
     * @brief 注册一个返回字符串列表的服务器占位符
     */
    PA_API void registerServerListPlaceholder(
        const std::string&           pluginName,
        const std::string&           placeholder,
        ServerListReplacer&&         replacer,
        std::optional<CacheDuration> cache_duration = std::nullopt,
        CacheKeyStrategy             strategy       = CacheKeyStrategy::Default
    );

    /**
     * @brief 注册一个带参数、返回字符串列表的服务器占位符
     */
    PA_API void registerServerListPlaceholderWithParams(
        const std::string&             pluginName,
        const std::string&             placeholder,
        ServerListReplacerWithParams&& replacer,
        std::optional<CacheDuration>   cache_duration = std::nullopt,
        CacheKeyStrategy               strategy       = CacheKeyStrategy::Default
    );

    /**
     * @brief 注册一个特定类型、返回字符串列表的占位符
     * @tparam T 上下文对象类型
     */
    template <typename T>
    void registerListPlaceholder(
        const std::string&                            pluginName,
        const std::string&                            placeholder,
        std::function<std::vector<std::string>(T*)>&& replacer,
        std::optional<CacheDuration>                  cache_duration = std::nullopt,
        CacheKeyStrategy                              strategy       = CacheKeyStrategy::Default
    ) {
        static_assert(
            !std::is_same_v<T, void>,
            "void* is not allowed as a type argument for type-safe placeholder registration."
        );
        auto               targetId = mTypeSystem->ensureTypeId(typeKey<T>());
        AnyPtrListReplacer fn       = [r = std::move(replacer)](void* p) -> std::vector<std::string> {
            if (!p) return {};
            return r(static_cast<T*>(p));
        };
        mRegistry->registerListPlaceholderForTypeId(
            pluginName,
            placeholder,
            targetId,
            std::move(fn),
            cache_duration,
            strategy
        );
    }

    /**
     * @brief 注册一个特定类型、带参数、返回字符串列表的占位符
     * @tparam T 上下文对象类型
     */
    template <typename T>
    void registerListPlaceholderWithParams(
        const std::string&                                                        pluginName,
        const std::string&                                                        placeholder,
        std::function<std::vector<std::string>(T*, const Utils::ParsedParams&)>&& replacer,
        std::optional<CacheDuration>                                              cache_duration = std::nullopt,
        CacheKeyStrategy                                                          strategy = CacheKeyStrategy::Default
    ) {
        static_assert(
            !std::is_same_v<T, void>,
            "void* is not allowed as a type argument for type-safe placeholder registration."
        );
        auto                         targetId = mTypeSystem->ensureTypeId(typeKey<T>());
        AnyPtrListReplacerWithParams fn =
            [r = std::move(replacer)](void* p, const Utils::ParsedParams& params) -> std::vector<std::string> {
            if (!p) return {};
            return r(static_cast<T*>(p), params);
        };
        mRegistry->registerListPlaceholderForTypeId(
            pluginName,
            placeholder,
            targetId,
            std::move(fn),
            cache_duration,
            strategy
        );
    }

    // --- 已废弃的异步非模板注册接口 ---
    [[deprecated("Use the type-safe template overload, e.g., registerAsyncPlaceholder<T>(...), instead.")]] PA_API void
    registerAsyncPlaceholderForTypeKey(
        const std::string&    pluginName,
        const std::string&    placeholder,
        const std::string&    typeKeyStr,
        AsyncAnyPtrReplacer&& replacer
    );

    [[deprecated("Use the type-safe template overload, e.g., registerAsyncPlaceholderWithParams<T>(...), instead."
    )]] PA_API void
    registerAsyncPlaceholderForTypeKeyWithParams(
        const std::string&              pluginName,
        const std::string&              placeholder,
        const std::string&              typeKeyStr,
        AsyncAnyPtrReplacerWithParams&& replacer
    );

    // --- 类型系统相关接口 ---

    /**
     * @brief 注册类型继承关系
     * @tparam Derived 派生类
     * @tparam Base 基类
     */
    template <typename Derived, typename Base>
    void registerInheritance() {
        static_assert(
            std::is_base_of_v<Base, Derived>,
            "In registerInheritance<Derived, Base>, Base must be a public base of Derived."
        );
        mTypeSystem->registerInheritanceByKeys(
            typeKey<Derived>(),
            typeKey<Base>(),
            +[](void* p) -> void* { return static_cast<Base*>(static_cast<Derived*>(p)); }
        );
    }

    /**
     * @brief 通过类型键字符串注册继承关系
     * @param derivedKey 派生类的类型键
     * @param baseKey 基类的类型键
     * @param caster 从派生类指针到基类指针的转换函数
     */
    PA_API void registerInheritanceByKeys(const std::string& derivedKey, const std::string& baseKey, Caster caster);

    using InheritancePair = PlaceholderTypeSystem::InheritancePair;

    /**
     * @brief 批量注册继承关系
     * @param pairs 继承关系对的向量
     */
    PA_API void registerInheritanceByKeysBatch(const std::vector<InheritancePair>& pairs);

    /**
     * @brief 为一个类型注册别名
     * @tparam T 目标类型
     * @param alias 别名字符串
     */
    template <typename T>
    void registerTypeAlias(const std::string& alias) {
        mTypeSystem->registerTypeAlias(alias, typeKey<T>());
    }

    /**
     * @brief 通过类型键为类型注册别名
     * @param alias 别名
     * @param typeKeyStr 目标类型的类型键
     */
    PA_API void registerTypeAlias(const std::string& alias, const std::string& typeKeyStr);

    // --- 注销与查询 ---

    /**
     * @brief 注销一个插件的所有同步占位符
     * @param pluginName 插件名
     */
    PA_API void unregisterPlaceholders(const std::string& pluginName);

    /**
     * @brief 注销一个插件的所有异步占位符
     * @param pluginName 插件名
     */
    PA_API void unregisterAsyncPlaceholders(const std::string& pluginName);

    using PlaceholderCategory = PlaceholderRegistry::PlaceholderCategory;
    using PlaceholderInfo     = PlaceholderRegistry::PlaceholderInfo;
    using AllPlaceholders     = PlaceholderRegistry::AllPlaceholders;
    /**
     * @brief 获取所有已注册的占位符信息
     * @return 包含所有占位符信息的结构体
     */
    PA_API AllPlaceholders getAllPlaceholders() const;

    /**
     * @brief 检查是否存在指定的占位符
     * @param pluginName 插件名
     * @param placeholderName 占位符名
     * @param typeKey 上下文类型键（可选）
     * @return 如果存在则返回 true
     */
    PA_API bool hasPlaceholder(
        const std::string&                pluginName,
        const std::string&                placeholderName,
        const std::optional<std::string>& typeKey = std::nullopt
    ) const;

    // --- 占位符替换核心接口 ---

    /**
     * @brief 替换字符串中的服务器占位符
     * @param text 包含占位符的文本
     * @return 替换后的字符串
     */
    PA_API std::string replacePlaceholders(const std::string& text);

    /**
     * @brief 使用 std::any 类型的上下文对象替换占位符
     * @param text 包含占位符的文本
     * @param contextObject 上下文对象
     * @return 替换后的字符串
     */
    PA_API std::string replacePlaceholders(const std::string& text, std::any contextObject);

    /**
     * @brief 使用 PlaceholderContext 替换占位符
     * @param text 包含占位符的文本
     * @param ctx 占位符上下文
     * @return 替换后的字符串
     */
    PA_API std::string replacePlaceholders(const std::string& text, const PlaceholderContext& ctx);

    /**
     * @brief 使用 Player* 作为上下文替换占位符
     * @param text 包含占位符的文本
     * @param player 玩家指针
     * @return 替换后的字符串
     */
    PA_API std::string replacePlaceholders(const std::string& text, Player* player);

    /**
     * @brief 将字符串编译成模板以提高重复替换的效率
     * @param text 原始模板字符串
     * @return 编译后的模板对象
     */
    PA_API CompiledTemplate compileTemplate(const std::string& text);

    /**
     * @brief 使用编译后的模板和上下文进行替换
     * @param tpl 编译后的模板
     * @param ctx 占位符上下文
     * @return 替换后的字符串
     */
    PA_API std::string replacePlaceholders(const CompiledTemplate& tpl, const PlaceholderContext& ctx);

    /**
     * @brief 异步替换占位符
     * @param text 包含占位符的文本
     * @param ctx 占位符上下文
     * @return 一个持有最终结果的 future
     */
    PA_API std::future<std::string> replacePlaceholdersAsync(const std::string& text, const PlaceholderContext& ctx);

    /**
     * @brief 使用编译后的模板进行异步替换
     * @param tpl 编译后的模板
     * @param ctx 占位符上下文
     * @return 一个持有最终结果的 future
     */
    PA_API std::future<std::string>
           replacePlaceholdersAsync(const CompiledTemplate& tpl, const PlaceholderContext& ctx);

    /**
     * @brief 批量替换多个模板
     * @param tpls 编译后模板的引用向量
     * @param ctx 占位符上下文
     * @return 替换后字符串的向量
     */
    PA_API std::vector<std::string> replacePlaceholdersBatch(
        const std::vector<std::reference_wrapper<const CompiledTemplate>>& tpls,
        const PlaceholderContext&                                          ctx
    );

    /**
     * @brief 模板化的替换函数，方便直接传入对象指针
     * @tparam T 上下文对象类型
     * @param text 包含占位符的文本
     * @param obj 对象指针
     * @return 替换后的字符串
     */
    template <typename T>
    std::string replacePlaceholders(const std::string& text, T* obj) {
        return replacePlaceholders(text, makeContext(obj));
    }

    // --- 上下文创建辅助函数 ---

    /**
     * @brief 根据类型安全的指针创建上下文
     * @tparam T 对象类型
     * @param ptr 对象指针
     * @param rel_ctx 关系上下文（可选）
     * @return 创建的 PlaceholderContext
     */
    template <typename T>
    PlaceholderContext makeContext(T* ptr, const PlaceholderContext* rel_ctx = nullptr) {
        return makeContextRaw(static_cast<void*>(ptr), typeKey<T>(), rel_ctx);
    }

    /**
     * @brief 根据 void* 指针和类型键创建上下文
     * @param ptr void* 指针
     * @param typeKeyStr 类型键字符串
     * @param rel_ctx 关系上下文（可选）
     * @return 创建的 PlaceholderContext
     */
    PA_API PlaceholderContext
    makeContextRaw(void* ptr, const std::string& typeKeyStr, const PlaceholderContext* rel_ctx = nullptr);

    // --- 其他公共接口 ---

    /**
     * @brief 确保一个类型键存在于类型系统中，并返回其ID
     * @param typeKeyStr 类型键字符串
     * @return 类型ID
     */
    PA_API std::size_t ensureTypeId(const std::string& typeKeyStr);

    /**
     * @brief 查找两个类型之间的向上转型链
     * @param fromTypeId 起始类型ID
     * @param toTypeId 目标类型ID
     * @param outChain 输出的转换函数链
     * @return 如果找到转型链则返回 true
     */
    PA_API bool findUpcastChain(std::size_t fromTypeId, std::size_t toTypeId, std::vector<Caster>& outChain) const;

    /**
     * @brief 清理所有占位符的全局缓存
     */
    PA_API void clearCache();

    /**
     * @brief 清理特定插件的占位符缓存
     * @param pluginName 插件名
     */
    PA_API void clearCache(const std::string& pluginName);

    /**
     * @brief 设置最大递归深度
     * @param depth 深度值
     */
    PA_API void setMaxRecursionDepth(int depth);
    /**
     * @brief 获取最大递归深度
     * @return 深度值
     */
    PA_API int  getMaxRecursionDepth() const;
    /**
     * @brief 设置是否启用双大括号 `{{ }}` 转义
     * @param enable 是否启用
     */
    PA_API void setDoubleBraceEscape(bool enable);
    /**
     * @brief 获取是否启用双大括号转义
     * @return 是否启用
     */
    PA_API bool getDoubleBraceEscape() const;

private:
    // --- 私有辅助函数 ---

    /**
     * @brief 构建用于缓存的键
     */
    std::string buildCacheKey(
        const PlaceholderContext& ctx,
        std::string_view          pluginName,
        std::string_view          placeholderName,
        const std::string&        paramString,
        CacheKeyStrategy          strategy
    );
    using ReplacerMatch   = PlaceholderRegistry::ReplacerMatch;
    using PlaceholderType = PlaceholderRegistry::PlaceholderType;

    /**
     * @brief 缓存条目
     */
    struct CacheEntry {
        std::string                           result;    // 缓存的结果
        std::chrono::steady_clock::time_point expiresAt; // 过期时间点
    };

    // --- 核心成员变量 ---
    std::shared_ptr<PlaceholderTypeSystem> mTypeSystem; // 类型系统
    std::shared_ptr<PlaceholderRegistry>   mRegistry;   // 占位符注册表

    mutable LRUCache<std::string, CacheEntry> mGlobalCache; // 全局结果缓存

    mutable LRUCache<std::string, std::shared_ptr<CompiledTemplate>> mCompileCache; // 模板编译缓存

    mutable LRUCache<std::string, std::shared_ptr<Utils::ParsedParams>> mParamsCache; // 参数解析缓存

    std::unique_ptr<ThreadPool> mCombinerThreadPool; // 用于合并异步结果的线程池
    std::unique_ptr<ThreadPool> mAsyncThreadPool;    // 用于执行异步占位符的线程池

    // 用于请求合并（避免对同一占位符的并发重复计算）
    mutable std::recursive_mutex                                             mFuturesMutex;
    mutable std::unordered_map<std::string, std::shared_future<std::string>> mComputingFutures;

    int  mMaxRecursionDepth{12};       // 最大递归深度，防止无限循环
    bool mEnableDoubleBraceEscape{true}; // 是否启用 `{{ }}` 转义

private:
    // --- 构造与析构 ---
    PlaceholderManager();
    ~PlaceholderManager() = default;

    // 禁止拷贝和赋值
    PlaceholderManager(const PlaceholderManager&)            = delete;
    PlaceholderManager& operator=(const PlaceholderManager&) = delete;

    /**
     * @brief 替换过程中的状态，用于跟踪递归深度和请求内的缓存
     */
    struct ReplaceState {
        int                                          depth{0};  // 当前递归深度
        std::unordered_map<std::string, std::string> cache;   // 本次替换请求的局部缓存
    };
    /**
     * @brief 为长字符串生成哈希作为缓存键的一部分，以优化性能
     */
    static std::string hashString(const std::string& str) {
        if (str.size() <= 128) {
            return str; // 短字符串直接使用
        }
        // 对长字符串使用哈希
        std::hash<std::string> hasher;
        return "hash:" + std::to_string(hasher(str));
    }
    /**
     * @brief 同步替换的核心实现
     */
    std::string replacePlaceholdersSync(const CompiledTemplate& tpl, const PlaceholderContext& ctx, int depth);

    /**
     * @brief 获取解析后的参数对象（带缓存）
     */
    std::shared_ptr<Utils::ParsedParams> getParsedParams(const std::string& paramString);

    /**
     * @brief 执行已找到的替换器函数
     */
    std::string executeFoundReplacer(
        const ReplacerMatch&       match,
        void*                      p,
        void*                      p_rel,
        const Utils::ParsedParams& params,
        bool                       allowEmpty,
        ReplaceState&              st
    );

    /**
     * @brief 应用格式化、处理缓存和日志
     */
    std::string applyFormattingAndCache(
        const std::string&                    originalResult,
        const Utils::ParsedParams&            params,
        const std::string&                    defaultText,
        bool                                  allowEmpty,
        const std::string&                    cacheKey,
        std::optional<CacheDuration>          cacheDuration,
        PlaceholderType                       type,
        std::chrono::steady_clock::time_point startTime,
        ReplaceState&                         st,
        std::string_view                      pluginName,
        std::string_view                      placeholderName,
        const std::string&                    paramString,
        const PlaceholderContext&             ctx,
        bool                                  replaced
    );

    /**
     * @brief 执行单个占位符的替换逻辑（同步）
     */
    std::string executePlaceholder(
        std::string_view             pluginName,
        std::string_view             placeholderName,
        const std::string&           paramString,
        const std::string&           defaultText,
        const PlaceholderContext&    ctx,
        ReplaceState&                st,
        std::optional<CacheDuration> cache_duration_override = std::nullopt
    );

    /**
     * @brief 执行单个占位符的替换逻辑（异步）
     */
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
