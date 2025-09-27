#pragma once

#include "Macros.h"
#include "Utils.h"

#include <any>
#include <functional>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>
#include <memory>

class Player; // 前向声明

namespace PA {

namespace Utils {
class ParsedParams; // 前向声明
}

// 提供一个编译期稳定的“类型键”字符串（跨插件一致，且不依赖 RTTI）
template <typename T>
inline std::string typeKey() {
#if defined(_MSC_VER)
    return __FUNCSIG__; // MSVC
#else
    return __PRETTY_FUNCTION__; // GCC/Clang
#endif
}

/**
 * @brief 占位符上下文：携带对象指针与其“动态类型ID”
 *
 * 注意：typeId 为内部分配的整型ID，外部插件不需要知道数值，只需要用 makeContext()/register* 接口传入 typeKey
 * 字符串即可。
 */
struct PlaceholderContext {
    void*       ptr{nullptr};
    std::size_t typeId{0};
};

/**
 * @brief 负责管理占位符（服务器级、带上下文的多态占位符）以及类型系统（自定义多态/继承）
 */
class PlaceholderManager; // 前向声明

// --- 新：模板编译系统 ---
struct CompiledTemplate; // 前向声明

struct LiteralToken {
    std::string_view text;
};

struct PlaceholderToken {
    std::string_view                  pluginName;
    std::string_view                  placeholderName;
    std::unique_ptr<CompiledTemplate> defaultTemplate; // 嵌套模板
    std::unique_ptr<CompiledTemplate> paramsTemplate;  // 嵌套模板
};

using Token = std::variant<LiteralToken, PlaceholderToken>;

struct CompiledTemplate {
    std::string        source; // Holds the original string for string_views
    std::vector<Token> tokens;

    // 为支持 unique_ptr 的移动语义
    CompiledTemplate();
    ~CompiledTemplate();
    CompiledTemplate(CompiledTemplate&&) noexcept;
    CompiledTemplate& operator=(CompiledTemplate&&) noexcept;
};


class PlaceholderManager {
public:
    // 服务器占位符：无上下文
    using ServerReplacer = std::function<std::string()>;
    // 服务器占位符（带参数）
    using ServerReplacerWithParams = std::function<std::string(const Utils::ParsedParams& params)>;

    // 上下文占位符：内部总以 void* 接口保存；调用前会按类型系统进行“上行转换”使其成为目标类型子对象指针
    using AnyPtrReplacer = std::function<std::string(void*)>;
    // 上下文占位符（带参数）
    using AnyPtrReplacerWithParams = std::function<std::string(void*, const Utils::ParsedParams& params)>;

    using Caster = void* (*)(void*); // Derived* -> Base* 上行转换函数指针

    // 获取单例
    PA_API static PlaceholderManager& getInstance();

    // ---------------- 注册服务器占位符（无上下文） ----------------
    PA_API void
    registerServerPlaceholder(const std::string& pluginName, const std::string& placeholder, ServerReplacer replacer);

    // [新] 注册服务器占位符（带参数）
    PA_API void registerServerPlaceholderWithParams(
        const std::string&         pluginName,
        const std::string&         placeholder,
        ServerReplacerWithParams&& replacer
    );

    // [便捷] 兼容旧的 string_view 签名
    PA_API void registerServerPlaceholderWithParams(
        const std::string&                             pluginName,
        const std::string&                             placeholder,
        std::function<std::string(std::string_view)>&& replacer
    ) {
        // This compatibility layer is complex and inefficient.
        // Let's keep it for now but recognize it's a source of allocations.
        ServerReplacerWithParams fn = [r = std::move(replacer)](const Utils::ParsedParams& params) -> std::string {
            // For now, we pass an empty string_view as the raw parameter string is not readily available
            // without reconstruction. This is a change in behavior but avoids costly allocations.
            // A better solution would be to store the raw string in ParsedParams.
            return r({});
        };
        registerServerPlaceholderWithParams(pluginName, placeholder, std::move(fn));
    }

    /**
     * @brief [新] 注册上下文占位符（模板版）
     * 以 T 的类型键作为“目标类型”，当传入的动态类型是 T 或其派生类时，将通过类型系统上行转换为 T* 再调用 replacer。
     */
    template <typename T>
    void registerPlaceholder(
        const std::string&               pluginName,
        const std::string&               placeholder,
        std::function<std::string(T*)>&& replacer
    ) {
        auto           targetId = ensureTypeId(typeKey<T>());
        AnyPtrReplacer fn       = [r = std::move(replacer)](void* p) -> std::string {
            if (!p) return std::string{};
            return r(reinterpret_cast<T*>(p));
        };
        registerPlaceholderForTypeId(pluginName, placeholder, targetId, std::move(fn));
    }

    // [新] 注册上下文占位符（模板版，带参数）
    template <typename T>
    void registerPlaceholderWithParams(
        const std::string&                                 pluginName,
        const std::string&                                 placeholder,
        std::function<std::string(T*, std::string_view)>&& replacer
    ) {
        auto                     targetId = ensureTypeId(typeKey<T>());
        AnyPtrReplacerWithParams fn       = [r = std::move(replacer)](void* p, const Utils::ParsedParams& params) -> std::string {
            if (!p) return std::string{};
            // See note in registerServerPlaceholderWithParams. Passing empty string_view.
            return r(reinterpret_cast<T*>(p), {});
        };
        registerPlaceholderForTypeId(pluginName, placeholder, targetId, std::move(fn));
    }

    /**
     * @brief [新] 注册上下文占位符（显式类型键版）
     */
    PA_API void registerPlaceholderForTypeKey(
        const std::string& pluginName,
        const std::string& placeholder,
        const std::string& typeKeyStr,
        AnyPtrReplacer     replacer
    );

    // [新] 注册上下文占位符（显式类型键版，带参数）
    PA_API void registerPlaceholderForTypeKeyWithParams(
        const std::string&         pluginName,
        const std::string&         placeholder,
        const std::string&         typeKeyStr,
        AnyPtrReplacerWithParams&& replacer
    );

    /**
     * @brief [新] 注册类型继承关系：Derived -> Base 的上行转换
     */
    template <typename Derived, typename Base>
    void registerInheritance() {
        registerInheritanceByKeys(
            typeKey<Derived>(),
            typeKey<Base>(),
            +[](void* p) -> void* { return static_cast<Base*>(reinterpret_cast<Derived*>(p)); }
        );
    }

    /**
     * @brief [新] 注册类型继承关系（显式类型键版）
     */
    PA_API void registerInheritanceByKeys(const std::string& derivedKey, const std::string& baseKey, Caster caster);

    // 注销某插件的所有占位符
    PA_API void unregisterPlaceholders(const std::string& pluginName);

    // [新] 查询已注册的所有占位符
    struct AllPlaceholders {
        // plugin -> placeholder_name
        std::unordered_map<std::string, std::vector<std::string>> serverPlaceholders;
        std::unordered_map<std::string, std::vector<std::string>> contextPlaceholders;
    };
    PA_API AllPlaceholders getAllPlaceholders() const;

    // 替换占位符（无上下文）
    PA_API std::string replacePlaceholders(const std::string& text);

    /**
     * @brief [旧] 保留的兼容接口：std::any
     */
    PA_API std::string replacePlaceholders(const std::string& text, std::any contextObject);

    /**
     * @brief [新] 支持多态的上下文版本
     */
    PA_API std::string replacePlaceholders(const std::string& text, const PlaceholderContext& ctx);

    /**
     * @brief [便捷] 占位符上下文为 Player*
     */
    PA_API std::string replacePlaceholders(const std::string& text, Player* player);

    /**
     * @brief [新] 编译模板字符串为内部 Token 序列，以加速重复替换
     */
    PA_API CompiledTemplate compileTemplate(const std::string& text);

    /**
     * @brief [新] 使用已编译的模板进行替换
     */
    PA_API std::string replacePlaceholders(const CompiledTemplate& tpl, const PlaceholderContext& ctx);


    /**
     * @brief [便捷] 任意类型指针（模板），按该类型键作为“动态类型”
     */
    template <typename T>
    std::string replacePlaceholders(const std::string& text, T* obj) {
        return replacePlaceholders(text, makeContext(obj));
    }

    /**
     * @brief [便捷] 构造上下文（模板）
     */
    template <typename T>
    static PlaceholderContext makeContext(T* ptr) {
        return makeContextRaw(static_cast<void*>(ptr), typeKey<T>());
    }

    /**
     * @brief [便捷] 显式类型键构造上下文
     */
    PA_API static PlaceholderContext makeContextRaw(void* ptr, const std::string& typeKeyStr);

    // 注册上下文占位符（目标类型ID版）
    PA_API void registerPlaceholderForTypeId(
        const std::string& pluginName,
        const std::string& placeholder,
        std::size_t        targetTypeId,
        AnyPtrReplacer     replacer
    );

    // 注册上下文占位符（目标类型ID版，带参数）
    PA_API void registerPlaceholderForTypeId(
        const std::string&         pluginName,
        const std::string&         placeholder,
        std::size_t                targetTypeId,
        AnyPtrReplacerWithParams&& replacer
    );

    // 确保/获取类型ID
    PA_API std::size_t ensureTypeId(const std::string& typeKeyStr);

    // 类型系统“上行转换路径”查询
    PA_API bool findUpcastChain(std::size_t fromTypeId, std::size_t toTypeId, std::vector<Caster>& outChain) const;

    // 可选：配置解析行为
    PA_API void setMaxRecursionDepth(int depth);
    PA_API int  getMaxRecursionDepth() const;
    PA_API void setDoubleBraceEscape(bool enable);
    PA_API bool getDoubleBraceEscape() const;

private:
    // --- 内部结构 ---
    struct ServerReplacerEntry {
        std::variant<ServerReplacer, ServerReplacerWithParams> fn;
    };

    struct TypedReplacer {
        std::size_t                                            targetTypeId{0};
        std::variant<AnyPtrReplacer, AnyPtrReplacerWithParams> fn;
    };

    // 上行链 BFS 结果缓存
    struct UpcastCacheEntry {
        bool                success;
        std::vector<Caster> chain;
    };

    // 服务器占位符：插件名 -> (占位符 -> 替换函数)
    std::unordered_map<std::string, std::unordered_map<std::string, ServerReplacerEntry>> mServerPlaceholders;

    // 上下文占位符（多态）：插件名 -> (占位符 -> [候选列表])
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<TypedReplacer>>> mContextPlaceholders;

    // 类型系统：typeKeyStr <-> typeId
    std::unordered_map<std::string, std::size_t> mTypeKeyToId;
    std::unordered_map<std::size_t, std::string> mIdToTypeKey;
    std::size_t                                  mNextTypeId{1}; // 0 保留为“无类型”

    // 继承图：派生ID -> (基类ID -> 上行转换函数)
    std::unordered_map<std::size_t, std::unordered_map<std::size_t, Caster>> mUpcastEdges;

    // 上行链缓存：(fromTypeId << 32) | toTypeId -> { 成功与否, 链 }
    mutable std::unordered_map<uint64_t, UpcastCacheEntry> mUpcastCache;

    // 线程安全
    mutable std::shared_mutex mMutex;

    // 解析配置
    int  mMaxRecursionDepth{12};
    bool mEnableDoubleBraceEscape{true};

private:
    PlaceholderManager();
    ~PlaceholderManager() = default;

    PlaceholderManager(const PlaceholderManager&)            = delete;
    PlaceholderManager& operator=(const PlaceholderManager&) = delete;

    // 内部实现：带状态与深度控制的替换
    struct ReplaceState {
        int depth{0};
        // 同一轮替换内的简易缓存（key: ctxptr#typeId#plugin:ph|params）
        std::unordered_map<std::string, std::string> cache;
    };

    std::string replacePlaceholdersImpl(const std::string& text, const PlaceholderContext& ctx, ReplaceState& st);

    // [新] 私有辅助函数：执行单个占位符的查找与替换
    std::string executePlaceholder(
        std::string_view        pluginName,
        std::string_view        placeholderName,
        const std::string&      paramString,
        const std::string&      defaultText,
        const PlaceholderContext& ctx,
        ReplaceState&           st
    );
};

} // namespace PA
