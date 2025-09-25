#pragma once

#include "Macros.h"
#include <any>
#include <functional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

class Player; // 前向声明

namespace PA {

// 提供一个编译期稳定的“类型键”字符串（跨插件一致，且不依赖 RTTI）
template <typename T>
inline std::string typeKey() {
#if defined(_MSC_VER)
    return __FUNCSIG__; // 在 MSVC 下同一类型在不同模块也一致
#else
    return __PRETTY_FUNCTION__; // 在 GCC/Clang 下同一类型在不同模块也一致
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
class PlaceholderManager {
public:
    // 服务器占位符：无上下文
    using ServerReplacer = std::function<std::string()>;
    // 上下文占位符：内部总以 void* 接口保存；调用前会按类型系统进行“上行转换”使其成为目标类型子对象指针
    using AnyPtrReplacer = std::function<std::string(void*)>;
    using Caster         = void* (*)(void*); // Derived* -> Base* 上行转换函数指针

    // 获取单例
    PA_API static PlaceholderManager& getInstance();

    // 注册服务器占位符（无上下文）
    PA_API void
    registerServerPlaceholder(const std::string& pluginName, const std::string& placeholder, ServerReplacer replacer);

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
        // 目标类型ID
        auto targetId = ensureTypeId(typeKey<T>());
        // 存储为 void* -> T* 的适配器，要求传入的是 T 子对象指针（我们会在派发前做上行转换，确保这里安全）
        AnyPtrReplacer fn = [r = std::move(replacer)](void* p) -> std::string {
            if (!p) return std::string{};
            return r(reinterpret_cast<T*>(p));
        };
        registerPlaceholderForTypeId(pluginName, placeholder, targetId, std::move(fn));
    }

    /**
     * @brief [新] 注册上下文占位符（显式类型键版，便于其他插件用自定义类型名）
     * @param typeKeyStr 任意保证在此类型维度内唯一且跨模块一致的字符串（建议用 typeKey<T>() 或自己约定的稳定字面量）
     * @param replacer   接收“已上行转换到该类型子对象指针”的 void*，需 reinterpret_cast 为该具体类型
     */
    PA_API void registerPlaceholderForTypeKey(
        const std::string& pluginName,
        const std::string& placeholder,
        const std::string& typeKeyStr,
        AnyPtrReplacer     replacer
    );

    /**
     * @brief [新] 注册类型继承关系：Derived -> Base 的上行转换
     * 无需 RTTI，使用静态转换构造出上行转换函数指针。
     */
    template <typename Derived, typename Base>
    void registerInheritance() {
        registerInheritanceByKeys(
            typeKey<Derived>(),
            typeKey<Base>(),
            +[](void* p) -> void* {
                // 先恢复 Derived*，再静态上行到 Base*
                return static_cast<Base*>(reinterpret_cast<Derived*>(p));
            }
        );
    }

    /**
     * @brief [新] 注册类型继承关系（显式类型键版）
     * @param derivedKey e.g. typeKey<Derived>()
     * @param baseKey    e.g. typeKey<Base>()
     * @param caster     上行转换函数指针：Derived* -> Base*
     */
    PA_API void registerInheritanceByKeys(const std::string& derivedKey, const std::string& baseKey, Caster caster);

    // 注销某插件的所有占位符
    PA_API void unregisterPlaceholders(const std::string& pluginName);

    // 替换占位符（无上下文）
    PA_API std::string replacePlaceholders(const std::string& text);

    /**
     * @brief [旧] 保留的兼容接口：std::any
     * - 若 any 内部恰好是我们新的 PlaceholderContext，则直接走多态派发
     * - 若 any 恰好是常见内置类型指针（如 Player*），也会自动转为新 Context（仅作为示例；自定义类型请使用新接口）
     * - 其他情况：仅能匹配精确类型（历史行为），建议迁移到新接口
     */
    PA_API std::string replacePlaceholders(const std::string& text, std::any contextObject);

    /**
     * @brief [新] 支持多态的上下文版本
     */
    PA_API std::string replacePlaceholders(const std::string& text, const PlaceholderContext& ctx);

    /**
     * @brief [便捷] 替换，占位符上下文为 Player*（保留）
     */
    PA_API std::string replacePlaceholders(const std::string& text, Player* player);

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
     * @brief [便捷] 显式类型键构造上下文（当变量静态类型与动态类型不一致时，建议指定动态类型键）
     * 例如：只有 Base* 变量，但运行时其实是 Derived*，可传 Derived 的 typeKey。
     */
    PA_API static PlaceholderContext makeContextRaw(void* ptr, const std::string& typeKeyStr);

private:
    // 内部：注册上下文占位符（目标类型ID版）
    void registerPlaceholderForTypeId(
        const std::string& pluginName,
        const std::string& placeholder,
        std::size_t        targetTypeId,
        AnyPtrReplacer     replacer
    );

    // 内部：确保/获取类型ID
    std::size_t ensureTypeId(const std::string& typeKeyStr);

    // 内部：类型系统“上行转换路径”查询（from -> to），返回是否存在；out 为转换函数序列（最短路径）
    bool findUpcastChain(std::size_t fromTypeId, std::size_t toTypeId, std::vector<Caster>& outChain) const;

private:
    // 服务器占位符：插件名 -> (占位符 -> 替换函数)
    std::unordered_map<std::string, std::unordered_map<std::string, ServerReplacer>> mServerPlaceholders;

    // 上下文占位符（多态）：插件名 -> (占位符 -> [目标类型ID -> 函数])
    struct TypedReplacer {
        std::size_t    targetTypeId{0};
        AnyPtrReplacer fn;
    };
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<TypedReplacer>>> mContextPlaceholders;

    // 类型系统：typeKeyStr <-> typeId
    std::unordered_map<std::string, std::size_t> mTypeKeyToId;
    std::unordered_map<std::size_t, std::string> mIdToTypeKey;
    std::size_t                                  mNextTypeId{1}; // 0 保留为“无类型”

    // 继承图：派生ID -> (基类ID -> 上行转换函数)
    std::unordered_map<std::size_t, std::unordered_map<std::size_t, Caster>> mUpcastEdges;

private:
    PlaceholderManager();
    ~PlaceholderManager() = default;

    PlaceholderManager(const PlaceholderManager&)            = delete;
    PlaceholderManager& operator=(const PlaceholderManager&) = delete;
};

} // namespace PA