#pragma once

// 引入宏定义，例如 PA_API 用于导出符号
#include "Macros.h"
// 引入工具类，例如参数解析
#include "Utils.h"

// 标准库头文件
#include <any>           // 用于存储任意类型的数据
#include <chrono>        // 用于时间相关的操作，例如缓存过期
#include <functional>    // 用于 std::function
#include <future>        // 用于 std::future，实现异步操作
#include <memory>        // 用于智能指针，例如 unique_ptr
#include <optional>      // 用于可选值
#include <shared_mutex>  // 用于读写锁，实现线程安全
#include <string>        // 用于字符串
#include <string_view>   // 用于字符串视图，避免拷贝
#include <type_traits>   // 用于类型特性
#include <unordered_map> // 用于哈希表
#include <variant>       // 用于变体类型
#include <vector>        // 用于动态数组


#include "LRUCache.h" // 引入 LRU 缓存

class Player; // 前向声明 Minecraft 玩家类，避免循环引用

namespace PA { // PlaceholderAPI 命名空间

class ThreadPool; // 前向声明线程池类

namespace Utils {
class ParsedParams; // 前向声明参数解析类
} // namespace Utils

/**
 * @brief 提供一个编译期稳定的“类型键”字符串（跨插件一致，且不依赖 RTTI）
 *
 * 使用编译器内置宏来获取函数签名，作为类型的唯一标识符。
 * 这样可以避免在不同编译单元或插件之间因 RTTI 行为不一致导致的问题。
 * @tparam T 要获取类型键的类型
 * @return 类型的唯一字符串标识符
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
 *
 * 这个结构体用于在占位符替换过程中传递上下文信息。
 * `ptr` 指向实际的对象，`typeId` 是该对象在 PlaceholderManager 内部注册的类型ID。
 * 外部插件不需要知道 `typeId` 的具体数值，只需通过 `makeContext()` 或 `register*` 接口传入 `typeKey` 字符串即可。
 */
struct PlaceholderContext {
    void*       ptr{nullptr}; // 指向上下文对象的通用指针
    std::size_t typeId{0};    // 上下文对象的内部类型ID，0 表示无类型

    // 新增：关系型上下文
    const PlaceholderContext* relationalContext{nullptr};
};

/**
 * @brief 负责管理占位符（服务器级、带上下文的多态占位符）以及类型系统（自定义多态/继承）
 *
 * PlaceholderManager 是整个占位符系统的核心，采用单例模式。
 * 它负责：
 * 1. 注册和管理不同类型的占位符（服务器级和上下文相关）。
 * 2. 实现一个自定义的类型系统，支持类型继承和上行转换。
 * 3. 编译和替换包含占位符的模板字符串。
 */
class PlaceholderManager; // 前向声明 PlaceholderManager 类

// --- 新：模板编译系统 ---
struct CompiledTemplate; // 前向声明 CompiledTemplate 结构体，用于存储编译后的模板

/**
 * @brief 字面量 Token
 *
 * 表示模板字符串中的普通文本部分。
 */
struct LiteralToken {
    std::string_view text; // 字面量文本视图
};

/**
 * @brief 占位符 Token
 *
 * 表示模板字符串中的一个占位符，包含插件名、占位符名、默认值模板和参数模板。
 */
struct PlaceholderToken {
    std::string_view pluginName;                       // 插件名称
    std::string_view placeholderName;                  // 占位符名称
    std::unique_ptr<CompiledTemplate> defaultTemplate; // 嵌套的默认值模板，用于占位符无值时提供默认内容
    std::unique_ptr<CompiledTemplate> paramsTemplate; // 嵌套的参数模板，用于解析占位符的参数
};

// Token 类型可以是字面量或占位符
using Token = std::variant<LiteralToken, PlaceholderToken>;

/**
 * @brief 缓存键加强策略
 */
enum class CacheKeyStrategy {
    Default,    // 默认策略：key 中包含 ctx 指针与 typeId
    ServerOnly, // 快速路径：纯服务器级占位符，key 中不含 ctx
};

/**
 * @brief 编译后的模板结构
 *
 * 存储原始模板字符串和解析后的 Token 序列，用于高效地重复替换。
 */
struct CompiledTemplate {
    std::string        source; // 原始模板字符串，用于保证 string_view 的生命周期
    std::vector<Token> tokens; // 解析后的 Token 序列

    // 为支持 unique_ptr 的移动语义，需要自定义移动构造函数和移动赋值运算符
    CompiledTemplate();                                       // 默认构造函数
    ~CompiledTemplate();                                      // 默认析构函数
    CompiledTemplate(CompiledTemplate&&) noexcept;            // 移动构造函数
    CompiledTemplate& operator=(CompiledTemplate&&) noexcept; // 移动赋值运算符
};


class PlaceholderManager {
public:
    // 服务器占位符：不依赖任何上下文对象，直接返回字符串
    using ServerReplacer = std::function<std::string()>;
    // 服务器占位符（带参数）：不依赖上下文，但接受解析后的参数
    using ServerReplacerWithParams = std::function<std::string(const Utils::ParsedParams& params)>;

    // 缓存持续时间类型
    using CacheDuration = std::chrono::steady_clock::duration;

    // --- 新：异步占位符 ---
    // 异步服务器占位符
    using AsyncServerReplacer           = std::function<std::future<std::string>()>;
    using AsyncServerReplacerWithParams = std::function<std::future<std::string>(const Utils::ParsedParams& params)>;

    // 异步上下文占位符
    using AsyncAnyPtrReplacer = std::function<std::future<std::string>(void*)>;
    using AsyncAnyPtrReplacerWithParams =
        std::function<std::future<std::string>(void*, const Utils::ParsedParams& params)>;

    // 关系型上下文占位符
    using AnyPtrRelationalReplacer = std::function<std::string(void*, void*)>;
    using AnyPtrRelationalReplacerWithParams =
        std::function<std::string(void*, void*, const Utils::ParsedParams& params)>;


    // 上下文占位符：接受一个 void* 指针，并返回字符串。内部会进行类型转换。
    using AnyPtrReplacer = std::function<std::string(void*)>;
    // 上下文占位符（带参数）：接受一个 void* 指针和解析后的参数，并返回字符串。
    using AnyPtrReplacerWithParams = std::function<std::string(void*, const Utils::ParsedParams& params)>;

    // --- 新：列表/集合型占位符 ---
    // 返回字符串向量的占位符
    using ServerListReplacer           = std::function<std::vector<std::string>()>;
    using ServerListReplacerWithParams = std::function<std::vector<std::string>(const Utils::ParsedParams& params)>;
    using AnyPtrListReplacer           = std::function<std::vector<std::string>(void*)>;
    using AnyPtrListReplacerWithParams =
        std::function<std::vector<std::string>(void*, const Utils::ParsedParams& params)>;

    // --- 新：对象列表/集合型占位符 ---
    // 返回 PlaceholderContext 向量的占位符
    using ServerObjectListReplacer           = std::function<std::vector<PlaceholderContext>()>;
    using ServerObjectListReplacerWithParams = std::function<std::vector<PlaceholderContext>(const Utils::ParsedParams& params)>;
    using AnyPtrObjectListReplacer           = std::function<std::vector<PlaceholderContext>(void*)>;
    using AnyPtrObjectListReplacerWithParams =
        std::function<std::vector<PlaceholderContext>(void*, const Utils::ParsedParams& params)>;

    // 类型转换器：用于将派生类指针上行转换为基类指针
    using Caster = void* (*)(void*); // 函数指针，Derived* -> Base*

    /**
     * @brief 获取 PlaceholderManager 的单例实例
     * @return PlaceholderManager 的引用
     */
    PA_API static PlaceholderManager& getInstance();

    // ---------------- 注册服务器占位符（无上下文） ----------------
    /**
     * @brief 注册一个不依赖上下文的服务器级占位符
     * @param pluginName 插件名称
     * @param placeholder 占位符名称 (例如 "player_count")
     * @param replacer 替换函数，无参数，返回替换后的字符串
     * @param cache_duration 可选的缓存持续时间
     */
    PA_API void registerServerPlaceholder(
        const std::string&           pluginName,
        const std::string&           placeholder,
        ServerReplacer               replacer,
        std::optional<CacheDuration> cache_duration = std::nullopt,
        CacheKeyStrategy             strategy       = CacheKeyStrategy::Default
    );

    /**
     * @brief [新] 注册一个带参数的服务器级占位符
     * @param pluginName 插件名称
     * @param placeholder 占位符名称
     * @param replacer 替换函数，接受 ParsedParams 参数，返回替换后的字符串
     * @param cache_duration 可选的缓存持续时间
     */
    PA_API void registerServerPlaceholderWithParams(
        const std::string&           pluginName,
        const std::string&           placeholder,
        ServerReplacerWithParams&&   replacer,
        std::optional<CacheDuration> cache_duration = std::nullopt,
        CacheKeyStrategy             strategy       = CacheKeyStrategy::Default
    );

    /**
     * @brief [新] 注册一个异步的、不依赖上下文的服务器级占位符
     * @param pluginName 插件名称
     * @param placeholder 占位符名称
     * @param replacer 替换函数，返回 std::future<std::string>
     * @param cache_duration 可选的缓存持续时间
     */
    PA_API void registerAsyncServerPlaceholder(
        const std::string&           pluginName,
        const std::string&           placeholder,
        AsyncServerReplacer&&        replacer,
        std::optional<CacheDuration> cache_duration = std::nullopt,
        CacheKeyStrategy             strategy       = CacheKeyStrategy::Default
    );

    /**
     * @brief [新] 注册一个带参数的异步服务器级占位符
     * @param pluginName 插件名称
     * @param placeholder 占位符名称
     * @param replacer 替换函数，接受 ParsedParams，返回 std::future<std::string>
     * @param cache_duration 可选的缓存持续时间
     */
    PA_API void registerAsyncServerPlaceholderWithParams(
        const std::string&              pluginName,
        const std::string&              placeholder,
        AsyncServerReplacerWithParams&& replacer,
        std::optional<CacheDuration>    cache_duration = std::nullopt,
        CacheKeyStrategy                strategy       = CacheKeyStrategy::Default
    );

    /**
     * @brief [新] 注册关系型上下文占位符（模板版）
     *
     * 当传入的动态类型分别是 T 或其派生类，以及 T_Rel 或其派生类时，
     * 将通过类型系统上行转换为 T* 和 T_Rel* 再调用 replacer。
     * @tparam T 主体类型
     * @tparam T_Rel 关系类型
     * @param pluginName 插件名称
     * @param placeholder 占位符名称
     * @param replacer 替换函数，接受 T* 和 T_Rel* 指针，返回替换后的字符串
     */
    template <typename T, typename T_Rel>
    void registerRelationalPlaceholder(
        const std::string&                       pluginName,
        const std::string&                       placeholder,
        std::function<std::string(T*, T_Rel*)>&& replacer,
        std::optional<CacheDuration>             cache_duration = std::nullopt,
        CacheKeyStrategy                         strategy       = CacheKeyStrategy::Default
    ) {
        auto                     targetId     = ensureTypeId(typeKey<T>());
        auto                     relationalId = ensureTypeId(typeKey<T_Rel>());
        AnyPtrRelationalReplacer fn = [r = std::move(replacer)](void* p, void* p_rel) -> std::string {
            if (!p || !p_rel) return std::string{};
            return r(reinterpret_cast<T*>(p), reinterpret_cast<T_Rel*>(p_rel));
        };
        registerRelationalPlaceholderForTypeId(
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
     * @brief [新] 注册带参数的关系型上下文占位符（模板版）
     *
     * @tparam T 主体类型
     * @tparam T_Rel 关系类型
     * @param pluginName 插件名称
     * @param placeholder 占位符名称
     * @param replacer 替换函数，接受 T*, T_Rel* 指针和 const Utils::ParsedParams& 参数，返回替换后的字符串
     */
    template <typename T, typename T_Rel>
    void registerRelationalPlaceholderWithParams(
        const std::string&                                                   pluginName,
        const std::string&                                                   placeholder,
        std::function<std::string(T*, T_Rel*, const Utils::ParsedParams&)>&& replacer,
        std::optional<CacheDuration>                                         cache_duration = std::nullopt,
        CacheKeyStrategy                                                     strategy       = CacheKeyStrategy::Default
    ) {
        auto                               targetId     = ensureTypeId(typeKey<T>());
        auto                               relationalId = ensureTypeId(typeKey<T_Rel>());
        AnyPtrRelationalReplacerWithParams fn =
            [r = std::move(replacer)](void* p, void* p_rel, const Utils::ParsedParams& params) -> std::string {
            if (!p || !p_rel) return std::string{};
            return r(reinterpret_cast<T*>(p), reinterpret_cast<T_Rel*>(p_rel), params);
        };
        registerRelationalPlaceholderForTypeId(
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
     * @brief [便捷] 兼容旧的 string_view 签名，注册带参数的服务器级占位符
     *
     * 注意：这个兼容层会涉及额外的内存分配和行为改变，因为它无法直接获取原始参数字符串。
     * 建议更新为使用 `ServerReplacerWithParams`。
     * @param pluginName 插件名称
     * @param placeholder 占位符名称
     * @param replacer 替换函数，接受 string_view 参数，返回替换后的字符串
     */
    PA_API void registerServerPlaceholderWithParams(
        const std::string&                             pluginName,
        const std::string&                             placeholder,
        std::function<std::string(std::string_view)>&& replacer
    ) {
        // 这个兼容层复杂且低效。
        // 暂时保留，但要认识到它是内存分配的来源。
        ServerReplacerWithParams fn = [r = std::move(replacer)](const Utils::ParsedParams& params) -> std::string {
            // 目前，我们传递一个空的 string_view，因为原始参数字符串无法直接获取
            // 而无需重建。这是行为上的改变，但避免了昂贵的内存分配。
            // 更好的解决方案是在 ParsedParams 中存储原始字符串。
            return r({});
        };
        registerServerPlaceholderWithParams(pluginName, placeholder, std::move(fn));
    }

    /**
     * @brief [新] 注册上下文占位符（模板版）
     *
     * 以 T 的类型键作为“目标类型”，当传入的动态类型是 T 或其派生类时，
     * 将通过类型系统上行转换为 T* 再调用 replacer。
     * @tparam T 目标类型
     * @param pluginName 插件名称
     * @param placeholder 占位符名称
     * @param replacer 替换函数，接受 T* 指针，返回替换后的字符串
     */
    template <typename T>
    void registerPlaceholder(
        const std::string&               pluginName,
        const std::string&               placeholder,
        std::function<std::string(T*)>&& replacer,
        std::optional<CacheDuration>     cache_duration = std::nullopt,
        CacheKeyStrategy                 strategy       = CacheKeyStrategy::Default
    ) {
        auto           targetId = ensureTypeId(typeKey<T>()); // 获取目标类型的内部ID
        AnyPtrReplacer fn       = [r = std::move(replacer)](void* p) -> std::string {
            if (!p) return std::string{};      // 空指针检查
            return r(reinterpret_cast<T*>(p)); // 转换为 T* 并调用替换函数
        };
        registerPlaceholderForTypeId(pluginName, placeholder, targetId, std::move(fn), cache_duration, strategy);
    }

    /**
     * @brief [新] 注册异步上下文占位符（模板版）
     * @tparam T 目标类型
     * @param pluginName 插件名称
     * @param placeholder 占位符名称
     * @param replacer 替换函数，接受 T* 指针，返回 std::future<std::string>
     */
    template <typename T>
    void registerAsyncPlaceholder(
        const std::string&                            pluginName,
        const std::string&                            placeholder,
        std::function<std::future<std::string>(T*)>&& replacer,
        std::optional<CacheDuration>                  cache_duration = std::nullopt,
        CacheKeyStrategy                              strategy       = CacheKeyStrategy::Default
    ) {
        auto                targetId = ensureTypeId(typeKey<T>());
        AsyncAnyPtrReplacer fn       = [r = std::move(replacer)](void* p) -> std::future<std::string> {
            if (!p) {
                std::promise<std::string> promise;
                promise.set_value({});
                return promise.get_future();
            }
            return r(reinterpret_cast<T*>(p));
        };
        registerAsyncPlaceholderForTypeId(pluginName, placeholder, targetId, std::move(fn), cache_duration, strategy);
    }

    /**
     * @brief [新] 注册上下文占位符（模板版，带参数，兼容旧版）
     * @tparam T 目标类型
     * @param pluginName 插件名称
     * @param placeholder 占位符名称
     * @param replacer 替换函数，接受 T* 指针和 string_view 参数，返回替换后的字符串
     * @warning 此函数为兼容性保留，但无法传递实际参数。推荐使用接受 ParsedParams 的重载。
     */
    template <typename T>
    [[deprecated]] void registerPlaceholderWithParams(
        const std::string&                                 pluginName,
        const std::string&                                 placeholder,
        std::function<std::string(T*, std::string_view)>&& replacer
    ) {
        auto                     targetId = ensureTypeId(typeKey<T>()); // 获取目标类型的内部ID
        AnyPtrReplacerWithParams fn =
            [r = std::move(replacer)](void* p, const Utils::ParsedParams& params) -> std::string {
            if (!p) return std::string{}; // 空指针检查
            // 参见 registerServerPlaceholderWithParams 中的注释。传递空 string_view。
            return r(reinterpret_cast<T*>(p), {}); // 转换为 T* 并调用替换函数
        };
        registerPlaceholderForTypeId(pluginName, placeholder, targetId, std::move(fn));
    }

    /**
     * @brief [新] 注册上下文占位符（模板版，带参数，推荐）
     *
     * @tparam T 目标类型
     * @param pluginName 插件名称
     * @param placeholder 占位符名称
     * @param replacer 替换函数，接受 T* 指针和 const Utils::ParsedParams& 参数，返回替换后的字符串
     */
    template <typename T>
    void registerPlaceholderWithParams(
        const std::string&                                           pluginName,
        const std::string&                                           placeholder,
        std::function<std::string(T*, const Utils::ParsedParams&)>&& replacer,
        std::optional<CacheDuration>                                 cache_duration = std::nullopt,
        CacheKeyStrategy                                             strategy       = CacheKeyStrategy::Default
    ) {
        auto                     targetId = ensureTypeId(typeKey<T>());
        AnyPtrReplacerWithParams fn =
            [r = std::move(replacer)](void* p, const Utils::ParsedParams& params) -> std::string {
            if (!p) return std::string{};
            return r(reinterpret_cast<T*>(p), params);
        };
        registerPlaceholderForTypeId(pluginName, placeholder, targetId, std::move(fn), cache_duration, strategy);
    }

    /**
     * @brief [新] 注册异步上下文占位符（模板版，带参数）
     * @tparam T 目标类型
     * @param pluginName 插件名称
     * @param placeholder 占位符名称
     * @param replacer 替换函数，接受 T* 和 ParsedParams，返回 std::future<std::string>
     */
    template <typename T>
    void registerAsyncPlaceholderWithParams(
        const std::string&                                                        pluginName,
        const std::string&                                                        placeholder,
        std::function<std::future<std::string>(T*, const Utils::ParsedParams&)>&& replacer,
        std::optional<CacheDuration>                                              cache_duration = std::nullopt,
        CacheKeyStrategy                                                          strategy = CacheKeyStrategy::Default
    ) {
        auto                          targetId = ensureTypeId(typeKey<T>());
        AsyncAnyPtrReplacerWithParams fn =
            [r = std::move(replacer)](void* p, const Utils::ParsedParams& params) -> std::future<std::string> {
            if (!p) {
                std::promise<std::string> promise;
                promise.set_value({});
                return promise.get_future();
            }
            return r(reinterpret_cast<T*>(p), params);
        };
        registerAsyncPlaceholderForTypeId(pluginName, placeholder, targetId, std::move(fn), cache_duration, strategy);
    }

    /**
     * @brief [新] 注册上下文占位符（显式类型键版）
     *
     * 允许直接传入类型键字符串来注册占位符，适用于无法使用模板的场景。
     * @param pluginName 插件名称
     * @param placeholder 占位符名称
     * @param typeKeyStr 目标类型的字符串键
     * @param replacer 替换函数，接受 void* 指针，返回替换后的字符串
     */
    PA_API void registerPlaceholderForTypeKey(
        const std::string& pluginName,
        const std::string& placeholder,
        const std::string& typeKeyStr,
        AnyPtrReplacer     replacer
    );

    /**
     * @brief [新] 注册上下文占位符（显式类型键版，带参数）
     * @param pluginName 插件名称
     * @param placeholder 占位符名称
     * @param typeKeyStr 目标类型的字符串键
     * @param replacer 替换函数，接受 void* 指针和 ParsedParams 参数，返回替换后的字符串
     */
    PA_API void registerPlaceholderForTypeKeyWithParams(
        const std::string&         pluginName,
        const std::string&         placeholder,
        const std::string&         typeKeyStr,
        AnyPtrReplacerWithParams&& replacer
    );

    /**
     * @brief [新] 注册关系型上下文占位符（显式类型键版）
     * @param pluginName 插件名称
     * @param placeholder 占位符名称
     * @param typeKeyStr 主体类型的字符串键
     * @param relationalTypeKeyStr 关系类型的字符串键
     * @param replacer 替换函数，接受两个 void* 指针，返回替换后的字符串
     */
    PA_API void registerRelationalPlaceholderForTypeKey(
        const std::string&           pluginName,
        const std::string&           placeholder,
        const std::string&           typeKeyStr,
        const std::string&           relationalTypeKeyStr,
        AnyPtrRelationalReplacer&&   replacer,
        std::optional<CacheDuration> cache_duration = std::nullopt,
        CacheKeyStrategy             strategy       = CacheKeyStrategy::Default
    );

    /**
     * @brief [新] 注册带参数的关系型上下文占位符（显式类型键版）
     * @param pluginName 插件名称
     * @param placeholder 占位符名称
     * @param typeKeyStr 主体类型的字符串键
     * @param relationalTypeKeyStr 关系类型的字符串键
     * @param replacer 替换函数，接受两个 void* 指针和 ParsedParams 参数，返回替换后的字符串
     */
    PA_API void registerRelationalPlaceholderForTypeKeyWithParams(
        const std::string&                   pluginName,
        const std::string&                   placeholder,
        const std::string&                   typeKeyStr,
        const std::string&                   relationalTypeKeyStr,
        AnyPtrRelationalReplacerWithParams&& replacer,
        std::optional<CacheDuration>         cache_duration = std::nullopt,
        CacheKeyStrategy                     strategy       = CacheKeyStrategy::Default
    );

    // ---------------- 注册对象列表/集合型占位符 ----------------

    /**
     * @brief [新] 注册一个返回对象列表的服务器级占位符
     */
    PA_API void registerServerObjectListPlaceholder(
        const std::string&               pluginName,
        const std::string&               placeholder,
        ServerObjectListReplacer&&       replacer,
        std::optional<CacheDuration>     cache_duration = std::nullopt,
        CacheKeyStrategy                 strategy       = CacheKeyStrategy::Default
    );

    /**
     * @brief [新] 注册一个返回对象列表的服务器级占位符（带参数）
     */
    PA_API void registerServerObjectListPlaceholderWithParams(
        const std::string&                   pluginName,
        const std::string&                   placeholder,
        ServerObjectListReplacerWithParams&& replacer,
        std::optional<CacheDuration>         cache_duration = std::nullopt,
        CacheKeyStrategy                     strategy       = CacheKeyStrategy::Default
    );

    /**
     * @brief [新] 注册一个返回对象列表的上下文占位符（模板版）
     */
    template <typename T>
    void registerObjectListPlaceholder(
        const std::string&                                     pluginName,
        const std::string&                                     placeholder,
        std::function<std::vector<PlaceholderContext>(T*)>&&   replacer,
        std::optional<CacheDuration>                           cache_duration = std::nullopt,
        CacheKeyStrategy                                       strategy       = CacheKeyStrategy::Default
    ) {
        auto                   targetId = ensureTypeId(typeKey<T>());
        AnyPtrObjectListReplacer fn     = [r = std::move(replacer)](void* p) -> std::vector<PlaceholderContext> {
            if (!p) return {};
            return r(reinterpret_cast<T*>(p));
        };
        registerObjectListPlaceholderForTypeId(pluginName, placeholder, targetId, std::move(fn), cache_duration, strategy);
    }

    /**
     * @brief [新] 注册一个返回对象列表的上下文占位符（模板版，带参数）
     */
    template <typename T>
    void registerObjectListPlaceholderWithParams(
        const std::string&                                                           pluginName,
        const std::string&                                                           placeholder,
        std::function<std::vector<PlaceholderContext>(T*, const Utils::ParsedParams&)>&& replacer,
        std::optional<CacheDuration>                                                 cache_duration = std::nullopt,
        CacheKeyStrategy                                                             strategy       = CacheKeyStrategy::Default
    ) {
        auto                             targetId = ensureTypeId(typeKey<T>());
        AnyPtrObjectListReplacerWithParams fn =
            [r = std::move(replacer)](void* p, const Utils::ParsedParams& params) -> std::vector<PlaceholderContext> {
            if (!p) return {};
            return r(reinterpret_cast<T*>(p), params);
        };
        registerObjectListPlaceholderForTypeId(pluginName, placeholder, targetId, std::move(fn), cache_duration, strategy);
    }


    // ---------------- 注册列表/集合型占位符 ----------------

    /**
     * @brief [新] 注册一个返回列表的服务器级占位符
     * @param pluginName 插件名称
     * @param placeholder 占位符名称
     * @param replacer 替换函数，返回字符串向量
     * @param cache_duration 可选的缓存持续时间
     */
    PA_API void registerServerListPlaceholder(
        const std::string&             pluginName,
        const std::string&             placeholder,
        ServerListReplacer&&           replacer,
        std::optional<CacheDuration>   cache_duration = std::nullopt,
        CacheKeyStrategy               strategy       = CacheKeyStrategy::Default
    );

    /**
     * @brief [新] 注册一个返回列表的服务器级占位符（带参数）
     * @param pluginName 插件名称
     * @param placeholder 占位符名称
     * @param replacer 替换函数，返回字符串向量
     * @param cache_duration 可选的缓存持续时间
     */
    PA_API void registerServerListPlaceholderWithParams(
        const std::string&               pluginName,
        const std::string&               placeholder,
        ServerListReplacerWithParams&&   replacer,
        std::optional<CacheDuration>     cache_duration = std::nullopt,
        CacheKeyStrategy                 strategy       = CacheKeyStrategy::Default
    );

    /**
     * @brief [新] 注册一个返回列表的上下文占位符（模板版）
     * @tparam T 目标类型
     * @param pluginName 插件名称
     * @param placeholder 占位符名称
     * @param replacer 替换函数，接受 T*，返回字符串向量
     */
    template <typename T>
    void registerListPlaceholder(
        const std::string&                                     pluginName,
        const std::string&                                     placeholder,
        std::function<std::vector<std::string>(T*)>&&          replacer,
        std::optional<CacheDuration>                           cache_duration = std::nullopt,
        CacheKeyStrategy                                       strategy       = CacheKeyStrategy::Default
    ) {
        auto               targetId = ensureTypeId(typeKey<T>());
        AnyPtrListReplacer fn       = [r = std::move(replacer)](void* p) -> std::vector<std::string> {
            if (!p) return {};
            return r(reinterpret_cast<T*>(p));
        };
        registerListPlaceholderForTypeId(pluginName, placeholder, targetId, std::move(fn), cache_duration, strategy);
    }

    /**
     * @brief [新] 注册一个返回列表的上下文占位符（模板版，带参数）
     * @tparam T 目标类型
     * @param pluginName 插件名称
     * @param placeholder 占位符名称
     * @param replacer 替换函数，接受 T* 和 ParsedParams，返回字符串向量
     */
    template <typename T>
    void registerListPlaceholderWithParams(
        const std::string&                                                       pluginName,
        const std::string&                                                       placeholder,
        std::function<std::vector<std::string>(T*, const Utils::ParsedParams&)>&& replacer,
        std::optional<CacheDuration>                                             cache_duration = std::nullopt,
        CacheKeyStrategy                                                         strategy       = CacheKeyStrategy::Default
    ) {
        auto                         targetId = ensureTypeId(typeKey<T>());
        AnyPtrListReplacerWithParams fn       =
            [r = std::move(replacer)](void* p, const Utils::ParsedParams& params) -> std::vector<std::string> {
            if (!p) return {};
            return r(reinterpret_cast<T*>(p), params);
        };
        registerListPlaceholderForTypeId(pluginName, placeholder, targetId, std::move(fn), cache_duration, strategy);
    }


    /**
     * @brief [新] 注册异步上下文占位符（显式类型键版）
     * @param pluginName 插件名称
     * @param placeholder 占位符名称
     * @param typeKeyStr 目标类型的字符串键
     * @param replacer 替换函数，接受 void* 指针，返回 std::future<std::string>
     */
    PA_API void registerAsyncPlaceholderForTypeKey(
        const std::string&    pluginName,
        const std::string&    placeholder,
        const std::string&    typeKeyStr,
        AsyncAnyPtrReplacer&& replacer
    );

    /**
     * @brief [新] 注册异步上下文占位符（显式类型键版，带参数）
     * @param pluginName 插件名称
     * @param placeholder 占位符名称
     * @param typeKeyStr 目标类型的字符串键
     * @param replacer 替换函数，接受 void* 和 ParsedParams，返回 std::future<std::string>
     */
    PA_API void registerAsyncPlaceholderForTypeKeyWithParams(
        const std::string&              pluginName,
        const std::string&              placeholder,
        const std::string&              typeKeyStr,
        AsyncAnyPtrReplacerWithParams&& replacer
    );

    /**
     * @brief [新] 注册类型继承关系：Derived -> Base 的上行转换
     *
     * 告知占位符管理器 `Derived` 类型是 `Base` 类型的派生类，并提供一个静态转换函数。
     * 这样在查找上下文占位符时，如果传入 `Derived*`，也能匹配到 `Base*` 的占位符。
     * @tparam Derived 派生类类型
     * @tparam Base 基类类型
     */
    template <typename Derived, typename Base>
    void registerInheritance() {
        registerInheritanceByKeys(
            typeKey<Derived>(), // 获取派生类的类型键
            typeKey<Base>(),    // 获取基类的类型键
            +[](void* p) -> void* { return static_cast<Base*>(reinterpret_cast<Derived*>(p)); } // 静态转换函数
        );
    }

    /**
     * @brief [新] 注册类型继承关系（显式类型键版）
     *
     * 允许直接传入类型键字符串和转换函数来注册继承关系。
     * @param derivedKey 派生类的类型键字符串
     * @param baseKey 基类的类型键字符串
     * @param caster 上行转换函数指针
     */
    PA_API void registerInheritanceByKeys(const std::string& derivedKey, const std::string& baseKey, Caster caster);

    /**
     * @brief [新] 用于批量注册继承关系的数据结构
     */
    struct InheritancePair {
        std::string derivedKey;
        std::string baseKey;
        Caster      caster;
    };

    /**
     * @brief [新] 批量注册类型继承关系
     *
     * 一次性注册多个继承关系，以减少锁竞争。
     * @param pairs 继承关系对的向量
     */
    PA_API void registerInheritanceByKeysBatch(const std::vector<InheritancePair>& pairs);

    /**
     * @brief [新] 为一个类型注册一个稳定的、跨编译器的“类型别名”
     *
     * 建议插件在启动时为所有用到的上下文类型注册别名，例如 "mc:Player", "mc:Actor"。
     * 这个别名将用于调试和观测，并作为未来更稳定 API 的基础。
     * @tparam T 要注册别名的类型
     * @param alias 类型的稳定别名字符串
     */
    template <typename T>
    void registerTypeAlias(const std::string& alias) {
        registerTypeAlias(alias, typeKey<T>());
    }

    /**
     * @brief [新] 为一个类型注册一个稳定的、跨编译器的“类型别名”（显式类型键版）
     * @param alias 类型的稳定别名字符串
     * @param typeKeyStr 类型的内部键 (通常来自 typeKey<T>())
     */
    PA_API void registerTypeAlias(const std::string& alias, const std::string& typeKeyStr);

    /**
     * @brief 注销某个插件注册的所有占位符
     * @param pluginName 要注销的插件名称
     */
    PA_API void unregisterPlaceholders(const std::string& pluginName);

    /**
     * @brief [新] 注销某个插件注册的所有异步占位符
     * @param pluginName 要注销的插件名称
     */
    PA_API void unregisterAsyncPlaceholders(const std::string& pluginName);

    /**
     * @brief [新] 查询已注册的所有占位符
     *
     * 返回一个结构体，包含所有服务器级和上下文相关的占位符列表。
     */
    // --- 新：观测与文档 ---
    enum class PlaceholderCategory {
        Server,
        Context,
        Relational,
        List,
        ObjectList,
    };
    struct PlaceholderInfo {
        std::string           name;             // 占位符名称
        PlaceholderCategory   category;         // 占位符类别
        bool                  isAsync{false};   // 是否为异步
        std::string           targetType;       // 目标类型 (上下文/列表)
        std::string           relationalType;   // 关系类型 (关系型)
        std::vector<std::string> overloads; // 同名占位符的其他类型重载
    };

    struct AllPlaceholders {
        // 插件名 -> 占位符信息列表
        std::unordered_map<std::string, std::vector<PlaceholderInfo>> placeholders;
    };
    PA_API AllPlaceholders getAllPlaceholders() const;

    /**
     * @brief [新] 检查占位符是否存在
     *
     * @param pluginName 插件名称
     * @param placeholderName 占位符名称
     * @param typeKey 可选的类型键。如果提供，则检查可用于该类型的上下文占位符；否则，检查服务器占位符。
     * @return 如果占位符存在，则返回 true
     */
    PA_API bool hasPlaceholder(
        const std::string&             pluginName,
        const std::string&             placeholderName,
        const std::optional<std::string>& typeKey = std::nullopt
    ) const;

    /**
     * @brief 替换文本中的占位符（无上下文）
     * @param text 包含占位符的原始文本
     * @return 替换后的文本
     */
    PA_API std::string replacePlaceholders(const std::string& text);

    /**
     * @brief [旧] 保留的兼容接口：使用 std::any 作为上下文对象
     *
     * 尝试将 std::any 转换为已知的上下文类型（如 Player* 或 PlaceholderContext），然后进行替换。
     * @param text 包含占位符的原始文本
     * @param contextObject 任意类型的上下文对象
     * @return 替换后的文本
     */
    PA_API std::string replacePlaceholders(const std::string& text, std::any contextObject);

    /**
     * @brief [新] 支持多态的上下文版本，使用 PlaceholderContext 进行替换
     * @param text 包含占位符的原始文本
     * @param ctx 占位符上下文
     * @return 替换后的文本
     */
    PA_API std::string replacePlaceholders(const std::string& text, const PlaceholderContext& ctx);

    /**
     * @brief [便捷] 占位符上下文为 Player* 的替换接口
     * @param text 包含占位符的原始文本
     * @param player 玩家指针作为上下文
     * @return 替换后的文本
     */
    PA_API std::string replacePlaceholders(const std::string& text, Player* player);

    /**
     * @brief [新] 编译模板字符串为内部 Token 序列，以加速重复替换
     *
     * 将包含占位符的字符串解析成一系列 Token (字面量或占位符)，
     * 这样在多次替换时可以避免重复解析，提高性能。
     * @param text 原始模板字符串
     * @return 编译后的 CompiledTemplate 对象
     */
    PA_API CompiledTemplate compileTemplate(const std::string& text);

    /**
     * @brief [新] 使用已编译的模板进行替换
     *
     * 接受一个预编译的 CompiledTemplate 对象和上下文，进行高效的占位符替换。
     * @param tpl 已编译的模板
     * @param ctx 占位符上下文
     * @return 替换后的文本
     */
    PA_API std::string replacePlaceholders(const CompiledTemplate& tpl, const PlaceholderContext& ctx);

    /**
     * @brief [新] 异步替换占位符（推荐）
     *
     * 收集所有异步占位符并并发执行它们，最后组合结果。
     * @param text 包含占位符的原始文本
     * @param ctx 占位符上下文
     * @return 一个 future，其值为最终替换后的字符串
     */
    PA_API std::future<std::string> replacePlaceholdersAsync(const std::string& text, const PlaceholderContext& ctx);

    /**
     * @brief [新] 使用已编译的模板进行异步替换
     * @param tpl 已编译的模板
     * @param ctx 占位符上下文
     * @return 一个 future，其值为最终替换后的字符串
     */
    PA_API std::future<std::string>
    replacePlaceholdersAsync(const CompiledTemplate& tpl, const PlaceholderContext& ctx);

    /**
     * @brief [新] 批量替换多个已编译的模板
     *
     * 对多个模板使用相同的上下文进行替换，并共享一个临时的内部缓存，
     * 以减少在多个模板中重复计算相同占位符的开销。
     * @param tpls 已编译模板的向量
     * @param ctx 占位符上下文
     * @return 替换后字符串的向量
     */
    PA_API std::vector<std::string> replacePlaceholdersBatch(
        const std::vector<std::reference_wrapper<const CompiledTemplate>>& tpls,
        const PlaceholderContext&                                          ctx
    );


    /**
     * @brief [便捷] 任意类型指针（模板），按该类型键作为“动态类型”进行替换
     * @tparam T 上下文对象的类型
     * @param text 包含占位符的原始文本
     * @param obj 上下文对象指针
     * @return 替换后的文本
     */
    template <typename T>
    std::string replacePlaceholders(const std::string& text, T* obj) {
        return replacePlaceholders(text, makeContext(obj));
    }

    /**
     * @brief [便捷] 构造上下文（模板）
     *
     * 从任意类型指针构造 PlaceholderContext。
     * @tparam T 上下文对象的类型
     * @param ptr 上下文对象指针
     * @param rel_ctx 可选的关系型上下文
     * @return 构造的 PlaceholderContext
     */
    template <typename T>
    static PlaceholderContext makeContext(T* ptr, const PlaceholderContext* rel_ctx = nullptr) {
        return makeContextRaw(static_cast<void*>(ptr), typeKey<T>(), rel_ctx);
    }

    /**
     * @brief [便捷] 显式类型键构造上下文
     *
     * 允许直接传入 void* 指针和类型键字符串来构造 PlaceholderContext。
     * @param ptr 上下文对象指针
     * @param typeKeyStr 类型键字符串
     * @param rel_ctx 可选的关系型上下文
     * @return 构造的 PlaceholderContext
     */
    PA_API static PlaceholderContext makeContextRaw(void* ptr, const std::string& typeKeyStr, const PlaceholderContext* rel_ctx = nullptr);

    /**
     * @brief 注册上下文占位符（目标类型ID版）
     *
     * 内部使用的注册接口，直接使用类型ID。
     * @param pluginName 插件名称
     * @param placeholder 占位符名称
     * @param targetTypeId 目标类型ID
     * @param replacer 替换函数
     */
    PA_API void registerPlaceholderForTypeId(
        const std::string&           pluginName,
        const std::string&           placeholder,
        std::size_t                  targetTypeId,
        AnyPtrReplacer               replacer,
        std::optional<CacheDuration> cache_duration = std::nullopt,
        CacheKeyStrategy             strategy       = CacheKeyStrategy::Default
    );

    /**
     * @brief 注册上下文占位符（目标类型ID版，带参数）
     *
     * 内部使用的注册接口，直接使用类型ID。
     * @param pluginName 插件名称
     * @param placeholder 占位符名称
     * @param targetTypeId 目标类型ID
     * @param replacer 替换函数（带参数）
     */
    PA_API void registerPlaceholderForTypeId(
        const std::string&           pluginName,
        const std::string&           placeholder,
        std::size_t                  targetTypeId,
        AnyPtrReplacerWithParams&&   replacer,
        std::optional<CacheDuration> cache_duration = std::nullopt,
        CacheKeyStrategy             strategy       = CacheKeyStrategy::Default
    );

    /**
     * @brief 注册异步上下文占位符（目标类型ID版）
     */
    PA_API void registerAsyncPlaceholderForTypeId(
        const std::string&           pluginName,
        const std::string&           placeholder,
        std::size_t                  targetTypeId,
        AsyncAnyPtrReplacer&&        replacer,
        std::optional<CacheDuration> cache_duration = std::nullopt,
        CacheKeyStrategy             strategy       = CacheKeyStrategy::Default
    );

    /**
     * @brief 注册异步上下文占位符（目标类型ID版，带参数）
     */
    PA_API void registerAsyncPlaceholderForTypeId(
        const std::string&              pluginName,
        const std::string&              placeholder,
        std::size_t                     targetTypeId,
        AsyncAnyPtrReplacerWithParams&& replacer,
        std::optional<CacheDuration>    cache_duration = std::nullopt,
        CacheKeyStrategy                strategy       = CacheKeyStrategy::Default
    );

    /**
     * @brief 注册关系型上下文占位符（目标类型ID版）
     */
    PA_API void registerRelationalPlaceholderForTypeId(
        const std::string&                 pluginName,
        const std::string&                 placeholder,
        std::size_t                        targetTypeId,
        std::size_t                        relationalTypeId,
        AnyPtrRelationalReplacer&&         replacer,
        std::optional<CacheDuration>       cache_duration = std::nullopt,
        CacheKeyStrategy                   strategy       = CacheKeyStrategy::Default
    );

    /**
     * @brief 注册带参数的关系型上下文占位符（目标类型ID版）
     */
    PA_API void registerRelationalPlaceholderForTypeId(
        const std::string&                  pluginName,
        const std::string&                  placeholder,
        std::size_t                         targetTypeId,
        std::size_t                         relationalTypeId,
        AnyPtrRelationalReplacerWithParams&& replacer,
        std::optional<CacheDuration>        cache_duration = std::nullopt,
        CacheKeyStrategy                    strategy       = CacheKeyStrategy::Default
    );

    /**
     * @brief 注册列表型上下文占位符（目标类型ID版，无参数）
     */
    PA_API void registerListPlaceholderForTypeId(
        const std::string&           pluginName,
        const std::string&           placeholder,
        std::size_t                  targetTypeId,
        AnyPtrListReplacer&&         replacer,
        std::optional<CacheDuration> cache_duration = std::nullopt,
        CacheKeyStrategy             strategy       = CacheKeyStrategy::Default
    );

    /**
     * @brief 注册列表型上下文占位符（目标类型ID版，带参数）
     */
    PA_API void registerListPlaceholderForTypeId(
        const std::string&             pluginName,
        const std::string&             placeholder,
        std::size_t                    targetTypeId,
        AnyPtrListReplacerWithParams&& replacer,
        std::optional<CacheDuration>   cache_duration = std::nullopt,
        CacheKeyStrategy               strategy       = CacheKeyStrategy::Default
    );

    /**
     * @brief 注册对象列表型上下文占位符（目标类型ID版，无参数）
     */
    PA_API void registerObjectListPlaceholderForTypeId(
        const std::string&           pluginName,
        const std::string&           placeholder,
        std::size_t                  targetTypeId,
        AnyPtrObjectListReplacer&&   replacer,
        std::optional<CacheDuration> cache_duration = std::nullopt,
        CacheKeyStrategy             strategy       = CacheKeyStrategy::Default
    );

    /**
     * @brief 注册对象列表型上下文占位符（目标类型ID版，带参数）
     */
    PA_API void registerObjectListPlaceholderForTypeId(
        const std::string&                 pluginName,
        const std::string&                 placeholder,
        std::size_t                        targetTypeId,
        AnyPtrObjectListReplacerWithParams&& replacer,
        std::optional<CacheDuration>       cache_duration = std::nullopt,
        CacheKeyStrategy                   strategy       = CacheKeyStrategy::Default
    );

    /**
     * @brief 确保并获取给定类型键的唯一类型ID
     *
     * 如果类型键已注册，返回其ID；否则，分配一个新的ID并注册。
     * @param typeKeyStr 类型键字符串
     * @return 对应的类型ID
     */
    PA_API std::size_t ensureTypeId(const std::string& typeKeyStr);

    /**
     * @brief 类型系统“上行转换路径”查询
     *
     * 查找从 `fromTypeId` 到 `toTypeId` 的最短上行转换链。
     * @param fromTypeId 源类型ID
     * @param toTypeId 目标类型ID
     * @param outChain 输出的上行转换函数链
     * @return 如果找到路径则返回 true，否则返回 false
     */
    PA_API bool findUpcastChain(std::size_t fromTypeId, std::size_t toTypeId, std::vector<Caster>& outChain) const;

    /**
     * @brief 清空所有占位符缓存
     */
    PA_API void clearCache();

    /**
     * @brief 清空指定插件的所有占位符缓存
     * @param pluginName 插件名称
     */
    PA_API void clearCache(const std::string& pluginName);

    // 可选：配置解析行为

    /**
     * @brief 设置占位符替换的最大递归深度
     *
     * 用于防止无限递归或深度过大的嵌套占位符导致性能问题。
     * @param depth 最大递归深度，必须大于等于 0
     */
    PA_API void setMaxRecursionDepth(int depth);
    /**
     * @brief 获取当前占位符替换的最大递归深度
     * @return 最大递归深度
     */
    PA_API int getMaxRecursionDepth() const;
    /**
     * @brief 设置是否启用双大括号转义功能
     *
     * 如果启用，`{{` 会被解析为 `{`，`}}` 会被解析为 `}`，而不是作为占位符的开始/结束。
     * @param enable 是否启用
     */
    PA_API void setDoubleBraceEscape(bool enable);
    /**
     * @brief 获取当前双大括号转义功能是否启用
     * @return 如果启用则返回 true，否则返回 false
     */
    PA_API bool getDoubleBraceEscape() const;

private:
    // 辅助函数
    std::string buildCacheKey(
        const PlaceholderContext& ctx,
        std::string_view          pluginName,
        std::string_view          placeholderName,
        const std::string&        paramString,
        CacheKeyStrategy          strategy
    );
    // --- 内部结构 ---
    /**
     * @brief 服务器占位符的内部存储条目
     *
     * 存储服务器占位符的替换函数，可以是无参数或带参数的版本。
     */
    struct ServerReplacerEntry {
        std::variant<ServerReplacer, ServerReplacerWithParams> fn; // 替换函数变体
        std::optional<CacheDuration>                           cacheDuration;
        CacheKeyStrategy                                       strategy;
    };

    /**
     * @brief 列表型服务器占位符的内部存储条目
     */
    struct ServerListReplacerEntry {
        std::variant<ServerListReplacer, ServerListReplacerWithParams> fn;
        std::optional<CacheDuration>                                   cacheDuration;
        CacheKeyStrategy                                               strategy;
    };

    /**
     * @brief 对象列表型服务器占位符的内部存储条目
     */
    struct ServerObjectListReplacerEntry {
        std::variant<ServerObjectListReplacer, ServerObjectListReplacerWithParams> fn;
        std::optional<CacheDuration>                                               cacheDuration;
        CacheKeyStrategy                                                           strategy;
    };

    /**
     * @brief 异步服务器占位符的内部存储条目
     */
    struct AsyncServerReplacerEntry {
        std::variant<AsyncServerReplacer, AsyncServerReplacerWithParams> fn;
        std::optional<CacheDuration>                                     cacheDuration;
        CacheKeyStrategy                                                 strategy;
    };

    /**
     * @brief 上下文占位符的内部存储条目
     *
     * 存储上下文占位符的目标类型ID和替换函数。
     */
    struct TypedReplacer {
        std::size_t                                            targetTypeId{0}; // 目标类型ID
        std::variant<AnyPtrReplacer, AnyPtrReplacerWithParams> fn;              // 替换函数变体
        std::optional<CacheDuration>                           cacheDuration;
        CacheKeyStrategy                                       strategy;
    };

    /**
     * @brief 列表型上下文占位符的内部存储条目
     */
    struct TypedListReplacer {
        std::size_t                                                  targetTypeId{0};
        std::variant<AnyPtrListReplacer, AnyPtrListReplacerWithParams> fn;
        std::optional<CacheDuration>                                 cacheDuration;
        CacheKeyStrategy                                             strategy;
    };

    /**
     * @brief 对象列表型上下文占位符的内部存储条目
     */
    struct TypedObjectListReplacer {
        std::size_t                                                      targetTypeId{0};
        std::variant<AnyPtrObjectListReplacer, AnyPtrObjectListReplacerWithParams> fn;
        std::optional<CacheDuration>                                     cacheDuration;
        CacheKeyStrategy                                                 strategy;
    };

    /**
     * @brief 异步上下文占位符的内部存储条目
     */
    struct AsyncTypedReplacer {
        std::size_t                                                      targetTypeId{0};
        std::variant<AsyncAnyPtrReplacer, AsyncAnyPtrReplacerWithParams> fn;
        std::optional<CacheDuration>                                     cacheDuration;
        CacheKeyStrategy                                                 strategy;
    };

    /**
     * @brief 关系型上下文占位符的内部存储条目
     */
    struct RelationalTypedReplacer {
        std::size_t                                                        targetTypeId{0};
        std::size_t                                                        relationalTypeId{0};
        std::variant<AnyPtrRelationalReplacer, AnyPtrRelationalReplacerWithParams> fn;
        std::optional<CacheDuration>                                       cacheDuration;
        CacheKeyStrategy                                                   strategy;
    };

    /**
     * @brief 上行转换链 BFS 结果缓存条目
     *
     * 存储 BFS 查找上行转换路径的结果，包括是否成功和转换函数链。
     */
    struct UpcastCacheEntry {
        bool                success; // 是否成功找到路径
        std::vector<Caster> chain;   // 上行转换函数链
    };

    /**
     * @brief 全局缓存条目
     */
    struct CacheEntry {
        std::string                           result;    // 缓存的结果
        std::chrono::steady_clock::time_point expiresAt; // 过期时间点
    };

    // --- 新增辅助结构体和函数声明 ---

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
        SyncFallback // 用于异步占位符执行失败时回退到同步
    };

    struct ReplacerMatch {
        PlaceholderType              type{PlaceholderType::None};
        std::optional<CacheDuration> cacheDuration;
        CacheKeyStrategy             strategy{CacheKeyStrategy::Default};

        // 存储实际的替换器条目，使用 std::monostate 作为空状态
        std::variant<
            std::monostate,
            ServerReplacerEntry,
            TypedReplacer,
            RelationalTypedReplacer,
            ServerListReplacerEntry,
            TypedListReplacer,
            ServerObjectListReplacerEntry,
            TypedObjectListReplacer>
            entry;

        std::vector<Caster> chain;           // 主体对象的上行转换链
        std::vector<Caster> relationalChain; // 关系型对象的上行转换链
    };

    // 服务器占位符映射：插件名 -> (占位符名 -> 替换函数条目)
    std::unordered_map<std::string, std::unordered_map<std::string, ServerReplacerEntry>>      mServerPlaceholders;
    std::unordered_map<std::string, std::unordered_map<std::string, AsyncServerReplacerEntry>> mAsyncServerPlaceholders;

    // 上下文占位符（多态）映射：插件名 -> (占位符名 -> (目标类型ID -> [候选替换函数列表]))
    std::unordered_map<
        std::string,
        std::unordered_map<std::string, std::unordered_multimap<std::size_t, TypedReplacer>>>
        mContextPlaceholders;
    std::unordered_map<
        std::string,
        std::unordered_map<std::string, std::unordered_multimap<std::size_t, AsyncTypedReplacer>>>
        mAsyncContextPlaceholders;

    // 列表型占位符映射
    std::unordered_map<std::string, std::unordered_map<std::string, ServerListReplacerEntry>> mServerListPlaceholders;
    std::unordered_map<
        std::string,
        std::unordered_map<std::string, std::unordered_multimap<std::size_t, TypedListReplacer>>>
        mContextListPlaceholders;

    // 对象列表型占位符映射
    std::unordered_map<std::string, std::unordered_map<std::string, ServerObjectListReplacerEntry>>
        mServerObjectListPlaceholders;
    std::unordered_map<
        std::string,
        std::unordered_map<std::string, std::unordered_multimap<std::size_t, TypedObjectListReplacer>>>
        mContextObjectListPlaceholders;

    // 关系型上下文占位符映射
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<RelationalTypedReplacer>>>
        mRelationalPlaceholders;

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

    // 全局占位符结果缓存
    mutable LRUCache<std::string, CacheEntry> mGlobalCache;

    // 模板编译缓存
    mutable LRUCache<std::string, std::shared_ptr<CompiledTemplate>> mCompileCache;

    // 参数解析缓存
    mutable LRUCache<std::string, std::shared_ptr<Utils::ParsedParams>> mParamsCache;

    // 线程安全：使用读写锁保护内部数据结构
    mutable std::shared_mutex mMutex;

    // 全局线程池
    std::unique_ptr<ThreadPool> mCombinerThreadPool; // 用于合并异步结果
    std::unique_ptr<ThreadPool> mAsyncThreadPool;    // 用于执行异步占位符

    // 解析配置
    int  mMaxRecursionDepth{12};         // 最大递归深度，默认 12
    bool mEnableDoubleBraceEscape{true}; // 是否启用双大括号转义，默认启用

private:
    // 私有构造函数，确保单例模式
    PlaceholderManager();
    // 默认析构函数
    ~PlaceholderManager() = default;

    // 禁止拷贝构造和拷贝赋值，确保单例模式
    PlaceholderManager(const PlaceholderManager&)            = delete;
    PlaceholderManager& operator=(const PlaceholderManager&) = delete;

    /**
     * @brief 内部实现：带状态与深度控制的占位符替换
     *
     * 这是旧版替换逻辑的内部实现，处理嵌套、转义、默认值和参数。
     * @param text 包含占位符的文本
     * @param ctx 占位符上下文
     * @param st 替换状态，包含递归深度和缓存
     * @return 替换后的文本
     */
    struct ReplaceState {
        int depth{0}; // 当前递归深度
        // 同一轮替换内的简易缓存（键格式: ctxptr#typeId#plugin:ph|params）
        std::unordered_map<std::string, std::string> cache;
    };

    std::string replacePlaceholdersSync(const CompiledTemplate& tpl, const PlaceholderContext& ctx, int depth);

    /**
     * @brief [新] 私有辅助函数：查找最匹配的占位符替换器
     * @param pluginName 插件名称视图
     * @param placeholderName 占位符名称视图
     * @param ctx 占位符上下文
     * @return 包含找到的替换器信息的 ReplacerMatch 对象
     */
    ReplacerMatch findBestReplacer(
        std::string_view          pluginName,
        std::string_view          placeholderName,
        const PlaceholderContext& ctx
    );

    /**
     * @brief [新] 私有辅助函数：获取解析后的参数
     * @param paramString 参数字符串
     * @return 共享的 ParsedParams 智能指针
     */
    std::shared_ptr<Utils::ParsedParams> getParsedParams(const std::string& paramString);

    /**
     * @brief [新] 私有辅助函数：执行找到的替换器
     * @param match 替换器匹配信息
     * @param p 主体对象指针
     * @param p_rel 关系型对象指针
     * @param params 解析后的参数
     * @param allowEmpty 是否允许返回空字符串
     * @param st 替换状态 (用于递归调用)
     * @return 替换器的原始结果
     */
    std::string executeFoundReplacer(
        const ReplacerMatch&                 match,
        void*                                p,
        void*                                p_rel,
        const Utils::ParsedParams&           params,
        bool                                 allowEmpty,
        ReplaceState&                        st
    );

    /**
     * @brief [新] 私有辅助函数：应用格式化、处理缓存和日志记录
     * @param originalResult 替换器的原始结果
     * @param params 解析后的参数
     * @param defaultText 默认文本
     * @param allowEmpty 是否允许返回空字符串
     * @param cacheKey 缓存键
     * @param cacheDuration 缓存持续时间
     * @param type 占位符类型
     * @param startTime 开始时间
     * @param st 替换状态
     * @param pluginName 插件名称视图
     * @param placeholderName 占位符名称视图
     * @param paramString 参数字符串
     * @param ctx 占位符上下文
     * @param replaced 是否成功替换
     * @return 最终输出字符串
     */
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

    /**
     * @brief [新] 私有辅助函数：执行单个占位符的查找与替换 (同步版本)
     *
     * 根据插件名、占位符名、参数、默认文本和上下文，查找并执行对应的替换函数。
     * @param pluginName 插件名称视图
     * @param placeholderName 占位符名称视图
     * @param paramString 参数字符串
     * @param defaultText 默认文本
     * @param ctx 占位符上下文
     * @param st 替换状态
     * @return 替换后的字符串
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
     * @brief [新] 私有辅助函数：执行单个占位符的异步查找与替换
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
