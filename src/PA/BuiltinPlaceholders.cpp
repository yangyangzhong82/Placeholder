#include "BuiltinPlaceholders.h"
#include "PlaceholderManager.h"
#include "ll/api/service/Bedrock.h"
#include "mc/network/ServerNetworkHandler.h"
#include "mc/server/ServerPlayer.h"
#include "mc/world/actor/Mob.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/actor/provider/ActorAttribute.h"
#include "mc/world/level/Level.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace PA {

void registerBuiltinPlaceholders() {
    auto& manager = PlaceholderManager::getInstance();

    // --- 注册玩家相关的占位符 ---
    manager.registerPlaceholder<Player>("pa", "player_name", [](Player* player) -> std::string {
        return player ? player->getRealName() : "";
    });
    manager.registerPlaceholder<Player>("pa", "ping", [](Player* player) -> std::string {
        if (player) {
            auto status = player->getNetworkStatus();
            return status ? std::to_string(status->mAveragePing) : "0";
        }
        return "0";
    });
    manager.registerPlaceholder<Mob>("pa", "health", [](Mob* entity) -> std::string {
        if (entity) {
            auto health = ActorAttribute::getHealth(entity->getEntityContext());
            return std::to_string(health);
        }
        return "0";
    });
    manager.registerPlaceholder<Mob>("pa", "max_health", [](Mob* entity) -> std::string {
        if (entity) {
            int MaxHealth = entity->getMaxHealth();
            return std::to_string(MaxHealth);
        }
        return "0";
    });
    manager.registerPlaceholder<Mob>("pa", "can_fly", [](Mob* entity) -> std::string {
        if (entity) {
            bool can = entity->canFly();
            return can ? "true" : "false"; // 便于与 trueText/falseText 映射
        }
        return "false";
    });
    // --- 注册服务器相关的占位符 ---
    manager.registerServerPlaceholder("pa", "online_players", []() -> std::string {
        auto level = ll::service::getLevel();
        return level ? std::to_string(level->getActivePlayerCount()) : "0";
    });
    manager.registerServerPlaceholder("pa", "max_players", []() -> std::string {
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

    manager.registerServerPlaceholder("pa", "time", [getTimeComponent]() {
        return getTimeComponent("%Y-%m-%d %H:%M:%S");
    });
    manager.registerServerPlaceholder("pa", "year", [getTimeComponent]() { return getTimeComponent("%Y"); });
    manager.registerServerPlaceholder("pa", "month", [getTimeComponent]() { return getTimeComponent("%m"); });
    manager.registerServerPlaceholder("pa", "day", [getTimeComponent]() { return getTimeComponent("%d"); });
    manager.registerServerPlaceholder("pa", "hour", [getTimeComponent]() { return getTimeComponent("%H"); });
    manager.registerServerPlaceholder("pa", "minute", [getTimeComponent]() { return getTimeComponent("%M"); });
    manager.registerServerPlaceholder("pa", "second", [getTimeComponent]() { return getTimeComponent("%S"); });
}

} // namespace PA
