#include "PlaceholderManager.h"
#include "ll/api/service/Bedrock.h"
#include "mc/network/ServerNetworkHandler.h"
#include "mc/server/ServerPlayer.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/Level.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace PA {

// 定义静态实例
PlaceholderManager& PlaceholderManager::getInstance() {
    static PlaceholderManager instance;
    return instance;
}

// 构造函数现在为空，所有注册逻辑已移至 registerBuiltinPlaceholders()
PlaceholderManager::PlaceholderManager() = default;

void PlaceholderManager::registerServerPlaceholder(
    const std::string& pluginName,
    const std::string& placeholder,
    ServerReplacer     replacer
) {
    mServerPlaceholders[pluginName][placeholder] = replacer;
}

void PlaceholderManager::unregisterPlaceholders(const std::string& pluginName) {
    mServerPlaceholders.erase(pluginName);
    mContextPlaceholders.erase(pluginName);
}

// 无上下文对象的版本，调用主函数并传入空的 std::any
std::string PlaceholderManager::replacePlaceholders(const std::string& text) {
    return replacePlaceholders(text, std::any{});
}

// 带有上下文对象的主替换函数
std::string PlaceholderManager::replacePlaceholders(const std::string& text, std::any contextObject) {
    if (text.find('{') == std::string::npos) {
        return text;
    }

    std::string result;
    result.reserve(text.length() * 1.5);

    size_t last_pos = 0;
    size_t find_pos;
    while ((find_pos = text.find('{', last_pos)) != std::string::npos) {
        result.append(text, last_pos, find_pos - last_pos);

        size_t end_pos = text.find('}', find_pos + 1);
        if (end_pos == std::string::npos) {
            last_pos = find_pos;
            break;
        }

        const std::string full_placeholder = text.substr(find_pos, end_pos - find_pos + 1);
        const std::string key              = text.substr(find_pos + 1, end_pos - find_pos - 1);
        size_t            colon_pos        = key.find(':');
        bool              replaced         = false;

        if (colon_pos != std::string::npos) {
            std::string pluginName      = key.substr(0, colon_pos);
            std::string placeholderName = key.substr(colon_pos + 1);

            // 1. 优先尝试匹配上下文相关的占位符
            if (contextObject.has_value()) {
                auto plugin_it = mContextPlaceholders.find(pluginName);
                if (plugin_it != mContextPlaceholders.end()) {
                    auto placeholder_it = plugin_it->second.find(placeholderName);
                    if (placeholder_it != plugin_it->second.end()) {
                        std::string replaced_val = placeholder_it->second(contextObject);
                        if (!replaced_val.empty()) {
                            result.append(replaced_val);
                            replaced = true;
                        }
                    }
                }
            }

            // 2. 如果上一步没有成功，尝试匹配服务器占位符
            if (!replaced) {
                auto plugin_it = mServerPlaceholders.find(pluginName);
                if (plugin_it != mServerPlaceholders.end()) {
                    auto placeholder_it = plugin_it->second.find(placeholderName);
                    if (placeholder_it != plugin_it->second.end()) {
                        result.append(placeholder_it->second());
                        replaced = true;
                    }
                }
            }
        }

        // 如果都失败了，保留原样
        if (!replaced) {
            result.append(full_placeholder);
        }

        last_pos = end_pos + 1;
    }

    if (last_pos < text.length()) {
        result.append(text, last_pos, std::string::npos);
    }
    return result;
}

// Player* 的便利重载版本
std::string PlaceholderManager::replacePlaceholders(const std::string& text, Player* player) {
    return replacePlaceholders(text, std::any{player});
}

} // namespace PA
