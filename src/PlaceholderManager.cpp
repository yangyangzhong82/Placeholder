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

// 构造函数现在只负责注册内置的占位符
PlaceholderManager::PlaceholderManager() {
    // --- 注册玩家占位符 ---
    registerPlayerPlaceholder("{player_name}", [](Player* player) -> std::string {
        return player ? player->getRealName() : "";
    });
    registerPlayerPlaceholder("{ping}", [](Player* player) -> std::string {
        if (player) {
            auto status = player->getNetworkStatus();
            return status ? std::to_string(status->mAveragePing) : "0";
        }
        return "0";
    });

    // --- 注册服务器占位符 ---
    registerServerPlaceholder("{online_players}", []() -> std::string {
        auto level = ll::service::getLevel();
        return level ? std::to_string(level->getActivePlayerCount()) : "0";
    });
    registerServerPlaceholder("{max_players}", []() -> std::string {
        auto server = ll::service::getServerNetworkHandler();
        return server ? std::to_string(server->mMaxNumPlayers) : "0";
    });

    auto getTimeComponent = [](const char* format) -> std::string {
        auto    now       = std::chrono::system_clock::now();
        auto    in_time_t = std::chrono::system_clock::to_time_t(now);
        std::tm buf;
#ifdef _WIN32
        localtime_s(&buf, &in_time_t);
#else
        localtime_r(&in_time_t, &buf);
#endif
        std::stringstream ss;
        ss << std::put_time(&buf, format);
        return ss.str();
    };

    registerServerPlaceholder("{time}", [getTimeComponent]() { return getTimeComponent("%Y-%m-%d %H:%M:%S"); });
    registerServerPlaceholder("{year}", [getTimeComponent]() { return getTimeComponent("%Y"); });
    registerServerPlaceholder("{month}", [getTimeComponent]() { return getTimeComponent("%m"); });
    registerServerPlaceholder("{day}", [getTimeComponent]() { return getTimeComponent("%d"); });
    registerServerPlaceholder("{hour}", [getTimeComponent]() { return getTimeComponent("%H"); });
    registerServerPlaceholder("{minute}", [getTimeComponent]() { return getTimeComponent("%M"); });
    registerServerPlaceholder("{second}", [getTimeComponent]() { return getTimeComponent("%S"); });
}

void PlaceholderManager::registerServerPlaceholder(const std::string& placeholder, ServerReplacer replacer) {
    mPlaceholders[placeholder] = replacer;
}

void PlaceholderManager::registerPlayerPlaceholder(const std::string& placeholder, PlayerReplacer replacer) {
    mPlaceholders[placeholder] = replacer;
}

// 统一的占位符替换实现
std::string PlaceholderManager::replacePlaceholders(const std::string& text, Player* player) {
    if (text.find('{') == std::string::npos) {
        return text;
    }

    std::string result;
    result.reserve(text.length() * 1.5); // 预留空间以提高性能

    size_t last_pos = 0;
    size_t find_pos;
    while ((find_pos = text.find('{', last_pos)) != std::string::npos) {
        result.append(text, last_pos, find_pos - last_pos);

        size_t end_pos = text.find('}', find_pos + 1);
        if (end_pos == std::string::npos) {
            // 没有找到匹配的 '}'，停止解析
            last_pos = find_pos;
            break;
        }

        const std::string placeholder(text, find_pos, end_pos - find_pos + 1);
        auto              it = mPlaceholders.find(placeholder);

        if (it != mPlaceholders.end()) {
            // 找到了占位符，使用 std::visit 来调用正确的 replacer
            std::visit(
                [&](auto&& arg) {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, ServerReplacer>) {
                        result.append(arg()); // 调用服务器 replacer
                    } else if constexpr (std::is_same_v<T, PlayerReplacer>) {
                        if (player) {
                            result.append(arg(player)); // 如果有 player 对象，调用玩家 replacer
                        } else {
                            result.append(placeholder); // 没有 player 对象，无法替换玩家占位符，保留原样
                        }
                    }
                },
                it->second
            );
        } else {
            result.append(placeholder); // 未找到，保留原样
        }
        last_pos = end_pos + 1;
    }

    if (last_pos < text.length()) {
        result.append(text, last_pos, std::string::npos);
    }
    return result;
}

} // namespace PA