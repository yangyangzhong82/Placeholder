#pragma once

#include "Macros.h"
#include <any> // 使用 std::any
#include <functional>
#include <string>
#include <type_traits> // 用于 SFINAE 或 if constexpr
#include <unordered_map>


class Player; // 前向声明

namespace PA {

class PlaceholderManager {
public:
    // 使用一个 map 存储所有占位符，值是 std::any
    std::unordered_map<std::string, std::any> mPlaceholders;
    // 获取全局唯一的实例（单例模式）
    PA_API static PlaceholderManager& getInstance();

    // 为服务器占位符定义清晰的类型别名
    using ServerReplacer = std::function<std::string()>;

    // 注册一个服务器占位符（与任何对象无关）
    PA_API void registerServerPlaceholder(const std::string& placeholder, ServerReplacer replacer);

    /**
     * @brief 模板化的函数，用于注册与特定类型相关的占位符
     * @tparam T 上下文对象的类型 (e.g., Player, Vehicle)
     * @param placeholder 占位符字符串 (e.g., "{player_name}")
     * @param replacer 一个接收 T* 指针并返回 std::string 的函数
     */
    template <typename T>
    PA_API void registerPlaceholder(const std::string& placeholder, std::function<std::string(T*)> replacer) {
        // 将特定类型的 replacer 存入 std::any
        mPlaceholders[placeholder] = replacer;
    }

    /**
     * @brief [重载] 替换占位符，只处理服务器占位符
     * @param text 包含占位符的原始文本
     * @return 替换后的文本
     */
    PA_API std::string replacePlaceholders(const std::string& text);

    /**
     * @brief [模板化重载] 替换占位符，处理服务器和特定于上下文对象的占位符
     * @tparam T 上下文对象的类型 (e.g., Player, Vehicle)
     * @param text 包含占位符的原始文本
     * @param contextObject 指向上下文对象的指针
     * @return 替换后的文本
     */
    template <typename T>
    PA_API std::string replacePlaceholders(const std::string& text, T* contextObject) {
        if (text.find('{') == std::string::npos) {
            return text;
        }

        std::string result;
        result.reserve(text.length() * 1.5); // 预留空间

        size_t last_pos = 0;
        size_t find_pos;
        while ((find_pos = text.find('{', last_pos)) != std::string::npos) {
            result.append(text, last_pos, find_pos - last_pos);

            size_t end_pos = text.find('}', find_pos + 1);
            if (end_pos == std::string::npos) {
                last_pos = find_pos;
                break;
            }

            const std::string placeholder(text, find_pos, end_pos - find_pos + 1);
            auto              it = mPlaceholders.find(placeholder);

            if (it != mPlaceholders.end()) {
                bool replaced = false;

                // 1. 优先尝试匹配特定类型的占位符 (如果上下文对象存在)
                if (contextObject) {
                    // 定义目标函数类型
                    using ContextReplacer = std::function<std::string(T*)>;
                    // 尝试从 std::any 中转换
                    if (auto* replacer = std::any_cast<ContextReplacer>(&it->second)) {
                        result.append((*replacer)(contextObject));
                        replaced = true;
                    }
                }

                // 2. 如果上一步没有成功，尝试匹配服务器占位符
                if (!replaced) {
                    if (auto* replacer = std::any_cast<ServerReplacer>(&it->second)) {
                        result.append((*replacer)());
                        replaced = true;
                    }
                }

                // 如果两种类型都转换失败，则保留原样
                if (!replaced) {
                    result.append(placeholder);
                }

            } else {
                result.append(placeholder); // 未找到占位符，保留原样
            }
            last_pos = end_pos + 1;
        }

        if (last_pos < text.length()) {
            result.append(text, last_pos, std::string::npos);
        }
        return result;
    }


private:
    // 私有化构造函数和析构函数，以实现单例模式
    PlaceholderManager();
    ~PlaceholderManager() = default;

    // 禁用拷贝和赋值
    PlaceholderManager(const PlaceholderManager&)            = delete;
    PlaceholderManager& operator=(const PlaceholderManager&) = delete;


};

} // namespace PA