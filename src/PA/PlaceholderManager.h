#pragma once

#include "Macros.h"
#include <any> 
#include <functional>
#include <string>
#include <type_traits> 
#include <unordered_map>


class Player; // 前向声明

namespace PA {

class PlaceholderManager {
public:
    // 为服务器占位符定义清晰的类型别名
    using ServerReplacer = std::function<std::string()>;

    // 获取全局唯一的实例（单例模式）
    PA_API static PlaceholderManager& getInstance();

    // 注册一个服务器占位符（与任何对象无关）
    PA_API void registerServerPlaceholder(const std::string& placeholder, ServerReplacer replacer);

    /**
     * @brief 模板化的函数，用于注册与特定类型相关的占位符
     * @tparam T 上下文对象的类型 (e.g., Player, Vehicle)
     * @param placeholder 占位符字符串 (e.g., "{player_name}")
     * @param replacer 一个接收 T* 指针并返回 std::string 的函数
     */
    template <typename T>
    void registerPlaceholder(const std::string& placeholder, std::function<std::string(T*)> replacer) {
        // 将特定类型的 replacer 包装成一个接受 std::any 的通用函数
        mContextPlaceholders[placeholder] = [replacer](std::any context) -> std::string {
            // 检查 context 是否为空
            if (!context.has_value()) {
                return "";
            }
            // 尝试将 context 转换为 T*
            try {
                T* obj = std::any_cast<T*>(context);
                if (obj) {
                    return replacer(obj);
                }
            } catch (const std::bad_any_cast&) {
                // 类型不匹配，说明此占位符不适用于给定的上下文对象
                // 在这种情况下，我们不应返回任何内容，让后续的查找（如服务器占位符）继续
            }
            return ""; // 返回空字符串表示此上下文占位符处理失败
        };
    }

    /**
     * @brief 替换占位符（无上下文对象，仅服务器占位符）
     * @param text 包含占位符的原始文本
     * @return 替换后的文本
     */
    PA_API std::string replacePlaceholders(const std::string& text);

    /**
     * @brief 替换占位符（带有上下文对象）
     * @param text 包含占位符的原始文本
     * @param contextObject 上下文对象，包装在 std::any 中 (e.g., std::any{player_ptr})
     * @return 替换后的文本
     */
    PA_API std::string replacePlaceholders(const std::string& text, std::any contextObject);

    /**
     * @brief [重载] 替换占位符（为 Player* 提供便利的重载版本）
     * @param text 包含占位符的原始文本
     * @param player 指向 Player 对象的指针
     * @return 替换后的文本
     */
    PA_API std::string replacePlaceholders(const std::string& text, Player* player);


private:
    // 使用类型擦除后的通用函数来存储上下文相关的占位符
    using ContextReplacer = std::function<std::string(std::any)>;

    // 分开存储不同类型的占位符
    std::unordered_map<std::string, ServerReplacer>  mServerPlaceholders;
    std::unordered_map<std::string, ContextReplacer> mContextPlaceholders;

    // 私有化构造函数和析构函数，以实现单例模式
    PlaceholderManager();
    ~PlaceholderManager() = default;

    // 禁用拷贝和赋值
    PlaceholderManager(const PlaceholderManager&)            = delete;
    PlaceholderManager& operator=(const PlaceholderManager&) = delete;
};

} // namespace PA
