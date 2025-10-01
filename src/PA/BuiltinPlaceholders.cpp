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
    manager.registerPlaceholder("{player_name}", [](Player* player) -> std::string {
        return player ? player->getRealName() : "";
    });

    manager.registerPlaceholder("{online_players}", [](Player* /*player*/) -> std::string {
        auto level = ll::service::getLevel();
        return level ? std::to_string(level->getActivePlayerCount()) : std::string("0");
    });

    manager.registerPlaceholder("{max_players}", [](Player* /*player*/) -> std::string {
        auto level = ll::service::getLevel();
        return level ? std::to_string(level->getActivePlayerCount()) : std::string("0");
    });

    manager.registerPlaceholder("{ping}", [](Player* player) -> std::string {
        if (player) {
            if (auto networkStatus = player->getNetworkStatus()) {
                return std::to_string(networkStatus->mAveragePing);
            }
        }
        return std::string("0");
    });

    manager.registerPlaceholder("{time}", [](Player* /*player*/) -> std::string {
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

    manager.registerPlaceholder("{year}", [](Player* /*player*/) -> std::string {
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

    manager.registerPlaceholder("{month}", [](Player* /*player*/) -> std::string {
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

    manager.registerPlaceholder("{day}", [](Player* /*player*/) -> std::string {
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

    manager.registerPlaceholder("{hour}", [](Player* /*player*/) -> std::string {
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

    manager.registerPlaceholder("{minute}", [](Player* /*player*/) -> std::string {
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

    manager.registerPlaceholder("{second}", [](Player* /*player*/) -> std::string {
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
