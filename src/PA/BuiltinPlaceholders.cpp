#include "PA/BuiltinPlaceholders.h"
#include "PA/PlaceholderManager.h"

#include "ll/api/service/Bedrock.h"
#include "mc/server/ServerPlayer.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/Level.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace PA {

void registerBuiltinPlaceholders(PlaceholderManager& manager) {
    manager.registerPlayerPlaceholder("{player_name}", [](Player* player) -> std::string {
        return player ? player->getRealName() : "";
    });

    manager.registerServerPlaceholder("{online_players}", []() -> std::string {
        auto level = ll::service::getLevel();
        return level ? std::to_string(level->getActivePlayerCount()) : std::string("0");
    });

    manager.registerServerPlaceholder("{max_players}", []() -> std::string {
        auto level = ll::service::getLevel();
        return level ? std::to_string(level->getActivePlayerCount()) : std::string("0");
    });

    manager.registerPlayerPlaceholder("{ping}", [](Player* player) -> std::string {
        if (player) {
            if (auto networkStatus = player->getNetworkStatus()) {
                return std::to_string(networkStatus->mAveragePing);
            }
        }
        return std::string("0");
    });

    manager.registerServerPlaceholder("{time}", []() -> std::string {
        auto    now       = std::chrono::system_clock::now();
        auto    in_time_t = std::chrono::system_clock::to_time_t(now);
        std::tm buf;
#ifdef _WIN32
        localtime_s(&buf, &in_time_t);
#else
        localtime_r(&in_time_t, &buf);
#endif
        std::stringstream ss;
        ss << std::put_time(&buf, "%Y-%m-%d %H:%M:%S");
        return ss.str();
    });

    manager.registerServerPlaceholder("{year}", []() -> std::string {
        auto    now       = std::chrono::system_clock::now();
        auto    in_time_t = std::chrono::system_clock::to_time_t(now);
        std::tm buf;
#ifdef _WIN32
        localtime_s(&buf, &in_time_t);
#else
        localtime_r(&in_time_t, &buf);
#endif
        return std::to_string(buf.tm_year + 1900);
    });

    manager.registerServerPlaceholder("{month}", []() -> std::string {
        auto    now       = std::chrono::system_clock::now();
        auto    in_time_t = std::chrono::system_clock::to_time_t(now);
        std::tm buf;
#ifdef _WIN32
        localtime_s(&buf, &in_time_t);
#else
        localtime_r(&in_time_t, &buf);
#endif
        return std::to_string(buf.tm_mon + 1);
    });

    manager.registerServerPlaceholder("{day}", []() -> std::string {
        auto    now       = std::chrono::system_clock::now();
        auto    in_time_t = std::chrono::system_clock::to_time_t(now);
        std::tm buf;
#ifdef _WIN32
        localtime_s(&buf, &in_time_t);
#else
        localtime_r(&in_time_t, &buf);
#endif
        return std::to_string(buf.tm_mday);
    });

    manager.registerServerPlaceholder("{hour}", []() -> std::string {
        auto    now       = std::chrono::system_clock::now();
        auto    in_time_t = std::chrono::system_clock::to_time_t(now);
        std::tm buf;
#ifdef _WIN32
        localtime_s(&buf, &in_time_t);
#else
        localtime_r(&in_time_t, &buf);
#endif
        return std::to_string(buf.tm_hour);
    });

    manager.registerServerPlaceholder("{minute}", []() -> std::string {
        auto    now       = std::chrono::system_clock::now();
        auto    in_time_t = std::chrono::system_clock::to_time_t(now);
        std::tm buf;
#ifdef _WIN32
        localtime_s(&buf, &in_time_t);
#else
        localtime_r(&in_time_t, &buf);
#endif
        return std::to_string(buf.tm_min);
    });

    manager.registerServerPlaceholder("{second}", []() -> std::string {
        auto    now       = std::chrono::system_clock::now();
        auto    in_time_t = std::chrono::system_clock::to_time_t(now);
        std::tm buf;
#ifdef _WIN32
        localtime_s(&buf, &in_time_t);
#else
        localtime_r(&in_time_t, &buf);
#endif
        return std::to_string(buf.tm_sec);
    });
}

} // namespace PA
