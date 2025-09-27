#pragma once

// 引入宏定义，例如 PA_API 用于导出符号
#include "Macros.h"
// 引入工具类，例如参数解析
#include "Utils.h"

// 标准库头文件
#include <any>          // 用于存储任意类型的数据
#include <functional>   // 用于 std::function
#include <future>       // 用于 std::future，实现异步操作
#include <optional>     // 用于可选值
#include <shared_mutex> // 用于读写锁，实现线程安全
#include <string>       // 用于字符串
#include <string_view>  // 用于字符串视图，避免拷贝
#include <type_traits>  // 用于类型特性
#include <unordered_map>// 用于哈希表
#include <variant>      // 用于变体类型
#include <vector>       // 用于动态数组
#include <memory>       // 用于智能指针，例如 unique_ptr

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
    std::string_view                  pluginName;      // 插件名称
    std::string_view                  placeholderName; // 占位符名称
    std::unique_ptr<CompiledTemplate> defaultTemplate; // 嵌套的默认值模板，用于占位符无值时提供默认内容
    std::unique_ptr<CompiledTemplate> paramsTemplate;  // 嵌套的参数模板，用于解析占位符的参数
};

// Token 类型可以是字面量或占位符
using Token = std::variant<LiteralToken, PlaceholderToken>;

/**
 * @brief 编译后的模板结构
 *
 * 存储原始模板字符串和解析后的 Token 序列，用于高效地重复替换。
 */
struct CompiledTemplate {
    std::string        source; // 原始模板字符串，用于保证 string_view 的生命周期
    std::vector<Token> tokens; // 解析后的 Token 序列

    // 为支持 unique_ptr 的移动语义，需要自定义移动构造函数和移动赋值运算符
    CompiledTemplate();                                 // 默认构造函数
    ~CompiledTemplate();                                // 默认析构函数
    CompiledTemplate(CompiledTemplate&&) noexcept;      // 移动构造函数
    CompiledTemplate& operator=(CompiledTemplate&&) noexcept; // 移动赋值运算符
};


class PlaceholderManager {
public:
    // 服务器占位符：不依赖任何上下文对象，直接返回字符串
    using ServerReplacer = std::function<std::string()>;
    // 服务器占位符（带参数）：不依赖上下文，但接受解析后的参数
    using ServerReplacerWithParams = std::function<std::string(const Utils::ParsedParams& params)>;

    // --- 新：异步占位符 ---
    // 异步服务器占位符
    using AsyncServerReplacer = std::function<std::future<std::string>()>;
    using AsyncServerReplacerWithParams = std::function<std::future<std::string>(const Utils::ParsedParams& params)>;

    // 异步上下文占位符
    using AsyncAnyPtrReplacer = std::function<std::future<std::string>(void*)>;
    using AsyncAnyPtrReplacerWithParams =
        std::function<std::future<std::string>(void*, const Utils::ParsedParams& params)>;


    // 上下文占位符：接受一个 void* 指针，并返回字符串。内部会进行类型转换。
    using AnyPtrReplacer = std::function<std::string(void*)>;
    // 上下文占位符（带参数）：接受一个 void* 指针和解析后的参数，并返回字符串。
    using AnyPtrReplacerWithParams = std::function<std::string(void*, const Utils::ParsedParams& params)>;

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
     */
    PA_API void
    registerServerPlaceholder(const std::string& pluginName, const std::string& placeholder, ServerReplacer replacer);

    /**
     * @brief [新] 注册一个带参数的服务器级占位符
     * @param pluginName 插件名称
     * @param placeholder 占位符名称
     * @param replacer 替换函数，接受 ParsedParams 参数，返回替换后的字符串
     */
    PA_API void registerServerPlaceholderWithParams(
        const std::string&         pluginName,
        const std::string&         placeholder,
        ServerReplacerWithParams&& replacer
    );

    /**
     * @brief [新] 注册一个异步的、不依赖上下文的服务器级占位符
     * @param pluginName 插件名称
     * @param placeholder 占位符名称
     * @param replacer 替换函数，返回 std::future<std::string>
     */
    PA_API void registerAsyncServerPlaceholder(
        const std::string& pluginName,
        const std::string& placeholder,
        AsyncServerReplacer&& replacer
    );

    /**
     * @brief [新] 注册一个带参数的异步服务器级占位符
     * @param pluginName 插件名称
     * @param placeholder 占位符名称
     * @param replacer 替换函数，接受 ParsedParams，返回 std::future<std::string>
     */
    PA_API void registerAsyncServerPlaceholderWithParams(
        const std::string&           pluginName,
        const std::string&           placeholder,
        AsyncServerReplacerWithParams&& replacer
    );

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
        std::function<std::string(T*)>&& replacer
    ) {
        auto           targetId = ensureTypeId(typeKey<T>()); // 获取目标类型的内部ID
        AnyPtrReplacer fn       = [r = std::move(replacer)](void* p) -> std::string {
            if (!p) return std::string{}; // 空指针检查
            return r(reinterpret_cast<T*>(p)); // 转换为 T* 并调用替换函数
        };
        registerPlaceholderForTypeId(pluginName, placeholder, targetId, std::move(fn));
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
        const std::string&                     pluginName,
        const std::string&                     placeholder,
        std::function<std::future<std::string>(T*)>&& replacer
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
        registerAsyncPlaceholderForTypeId(pluginName, placeholder, targetId, std::move(fn));
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
    [[deprecated]]  void registerPlaceholderWithParams(
        const std::string&                                 pluginName,
        const std::string&                                 placeholder,
        std::function<std::string(T*, std::string_view)>&& replacer
    ) {
        auto                     targetId = ensureTypeId(typeKey<T>()); // 获取目标类型的内部ID
        AnyPtrReplacerWithParams fn       = [r = std::move(replacer)](void* p, const Utils::ParsedParams& params) -> std::string {
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
        const std::string&                                     pluginName,
        const std::string&                                     placeholder,
        std::function<std::string(T*, const Utils::ParsedParams&)>&& replacer
    ) {
        auto                     targetId = ensureTypeId(typeKey<T>());
        AnyPtrReplacerWithParams fn       = [r = std::move(replacer)](void* p, const Utils::ParsedParams& params) -> std::string {
            if (!p) return std::string{};
            return r(reinterpret_cast<T*>(p), params);
        };
        registerPlaceholderForTypeId(pluginName, placeholder, targetId, std::move(fn));
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
        const std::string&                                           pluginName,
        const std::string&                                           placeholder,
        std::function<std::future<std::string>(T*, const Utils::ParsedParams&)>&& replacer
    ) {
        auto                     targetId = ensureTypeId(typeKey<T>());
        AsyncAnyPtrReplacerWithParams fn =
            [r = std::move(replacer)](void* p, const Utils::ParsedParams& params) -> std::future<std::string> {
            if (!p) {
                std::promise<std::string> promise;
                promise.set_value({});
                return promise.get_future();
            }
            return r(reinterpret_cast<T*>(p), params);
        };
        registerAsyncPlaceholderForTypeId(pluginName, placeholder, targetId, std::move(fn));
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
        const std::string&           pluginName,
        const std::string&           placeholder,
        const std::string&           typeKeyStr,
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
    struct AllPlaceholders {
        // 插件名 -> 占位符名称列表
        std::unordered_map<std::string, std::vector<std::string>> serverPlaceholders;
        std::unordered_map<std::string, std::vector<std::string>> contextPlaceholders;
    };
    PA_API AllPlaceholders getAllPlaceholders() const;

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
    PA_API std::future<std::string>
    replacePlaceholdersAsync(const std::string& text, const PlaceholderContext& ctx);

    /**
     * @brief [新] 使用已编译的模板进行异步替换
     * @param tpl 已编译的模板
     * @param ctx 占位符上下文
     * @return 一个 future，其值为最终替换后的字符串
     */
    PA_API std::future<std::string>
    replacePlaceholdersAsync(const CompiledTemplate& tpl, const PlaceholderContext& ctx);


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
     * @return 构造的 PlaceholderContext
     */
    template <typename T>
    static PlaceholderContext makeContext(T* ptr) {
        return makeContextRaw(static_cast<void*>(ptr), typeKey<T>());
    }

    /**
     * @brief [便捷] 显式类型键构造上下文
     *
     * 允许直接传入 void* 指针和类型键字符串来构造 PlaceholderContext。
     * @param ptr 上下文对象指针
     * @param typeKeyStr 类型键字符串
     * @return 构造的 PlaceholderContext
     */
    PA_API static PlaceholderContext makeContextRaw(void* ptr, const std::string& typeKeyStr);

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
        const std::string& pluginName,
        const std::string& placeholder,
        std::size_t        targetTypeId,
        AnyPtrReplacer     replacer
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
        const std::string&         pluginName,
        const std::string&         placeholder,
        std::size_t                targetTypeId,
        AnyPtrReplacerWithParams&& replacer
    );

    /**
     * @brief 注册异步上下文占位符（目标类型ID版）
     */
    PA_API void registerAsyncPlaceholderForTypeId(
        const std::string&    pluginName,
        const std::string&    placeholder,
        std::size_t           targetTypeId,
        AsyncAnyPtrReplacer&& replacer
    );

    /**
     * @brief 注册异步上下文占位符（目标类型ID版，带参数）
     */
    PA_API void registerAsyncPlaceholderForTypeId(
        const std::string&           pluginName,
        const std::string&           placeholder,
        std::size_t                  targetTypeId,
        AsyncAnyPtrReplacerWithParams&& replacer
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
    PA_API int  getMaxRecursionDepth() const;
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
    // --- 内部结构 ---
    /**
     * @brief 服务器占位符的内部存储条目
     *
     * 存储服务器占位符的替换函数，可以是无参数或带参数的版本。
     */
    struct ServerReplacerEntry {
        std::variant<ServerReplacer, ServerReplacerWithParams> fn; // 替换函数变体
    };

    /**
     * @brief 异步服务器占位符的内部存储条目
     */
    struct AsyncServerReplacerEntry {
        std::variant<AsyncServerReplacer, AsyncServerReplacerWithParams> fn;
    };

    /**
     * @brief 上下文占位符的内部存储条目
     *
     * 存储上下文占位符的目标类型ID和替换函数。
     */
    struct TypedReplacer {
        std::size_t                                            targetTypeId{0}; // 目标类型ID
        std::variant<AnyPtrReplacer, AnyPtrReplacerWithParams> fn;            // 替换函数变体
    };

    /**
     * @brief 异步上下文占位符的内部存储条目
     */
    struct AsyncTypedReplacer {
        std::size_t                                                  targetTypeId{0};
        std::variant<AsyncAnyPtrReplacer, AsyncAnyPtrReplacerWithParams> fn;
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

    // 服务器占位符映射：插件名 -> (占位符名 -> 替换函数条目)
    std::unordered_map<std::string, std::unordered_map<std::string, ServerReplacerEntry>> mServerPlaceholders;
    std::unordered_map<std::string, std::unordered_map<std::string, AsyncServerReplacerEntry>>
        mAsyncServerPlaceholders;

    // 上下文占位符（多态）映射：插件名 -> (占位符名 -> [候选替换函数列表])
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<TypedReplacer>>> mContextPlaceholders;
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<AsyncTypedReplacer>>>
        mAsyncContextPlaceholders;

    // 类型系统映射：类型键字符串 <-> 类型ID
    std::unordered_map<std::string, std::size_t> mTypeKeyToId; // 类型键到ID的映射
    std::unordered_map<std::size_t, std::string> mIdToTypeKey; // ID到类型键的映射
    std::size_t                                  mNextTypeId{1}; // 下一个可用的类型ID，0 保留为“无类型”

    // 继承图：派生类ID -> (基类ID -> 上行转换函数)
    std::unordered_map<std::size_t, std::unordered_map<std::size_t, Caster>> mUpcastEdges;

    // 上行链缓存：用于存储已计算过的上行转换路径，避免重复计算
    // 缓存键由 fromTypeId 和 toTypeId 组合而成
    mutable std::unordered_map<uint64_t, UpcastCacheEntry> mUpcastCache;

    // 线程安全：使用读写锁保护内部数据结构
    mutable std::shared_mutex mMutex;

    // 全局线程池
    std::unique_ptr<ThreadPool> mThreadPool;

    // 解析配置
    int  mMaxRecursionDepth{12};     // 最大递归深度，默认 12
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

    std::string replacePlaceholdersImpl(const std::string& text, const PlaceholderContext& ctx, ReplaceState& st);

    /**
     * @brief [新] 私有辅助函数：执行单个占位符的查找与替换
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
        std::string_view        pluginName,
        std::string_view        placeholderName,
        const std::string&      paramString,
        const std::string&      defaultText,
        const PlaceholderContext& ctx,
        ReplaceState&           st
    );

    /**
     * @brief [新] 私有辅助函数：执行单个占位符的异步查找与替换
     */
    std::future<std::string> executePlaceholderAsync(
        std::string_view        pluginName,
        std::string_view        placeholderName,
        const std::string&      paramString,
        const std::string&      defaultText,
        const PlaceholderContext& ctx,
        ReplaceState&           st
    );
};

} // namespace PA
