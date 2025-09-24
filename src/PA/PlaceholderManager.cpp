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

// 构造函数现在使用新的注册API
PlaceholderManager::PlaceholderManager() {
    // --- 注册玩家相关的占位符 ---
    // 使用模板化的 registerPlaceholder<Player>
    registerPlaceholder<Player>("{player_name}", [](Player* player) -> std::string {
        return player ? player->getRealName() : "";
    });
    registerPlaceholder<Player>("{ping}", [](Player* player) -> std::string {
        if (player) {
            auto status = player->getNetworkStatus();
            return status ? std::to_string(status->mAveragePing) : "0";
        }
        return "0";
    });

    // --- 注册服务器相关的占位符 ---
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

// 这个非模板化的重载函数只是为了方便调用，它内部调用模板版本
std::string PlaceholderManager::replacePlaceholders(const std::string& text) {
    // 传递 nullptr 作为上下文对象，这样只会匹配服务器占位符
    return replacePlaceholders<std::nullptr_t>(text, nullptr);
}

} // namespace PA