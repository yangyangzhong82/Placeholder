#include "BuiltinPlaceholders.h"
#include "PlaceholderManager.h"
#include "Utils.h"

#include "ll/api/service/Bedrock.h"
#include "mc/network/ServerNetworkHandler.h"
#include "mc/server/ServerPlayer.h"
#include "mc/world/actor/Mob.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/actor/provider/ActorAttribute.h"
#include "mc/world/level/Level.h"

#include <chrono>
#include <cmath>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <format> // C++20 for std::format
#include <string_view>
#include <vector>

namespace PA {

void registerBuiltinPlaceholders() {
    auto& manager = PlaceholderManager::getInstance();

    // 声明内置类型的继承关系（无 RTTI 环境下实现多态的关键）
    // Player : public Mob
    manager.registerInheritance<Player, Mob>();

    // --- 注册玩家/实体相关占位符（多态） ---
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
            return can ? "true" : "false";
        }
        return "false";
    });

    // --- 注册服务器占位符（无上下文） ---
    manager.registerServerPlaceholder("pa", "online_players", []() -> std::string {
        auto level = ll::service::getLevel();
        return level ? std::to_string(level->getActivePlayerCount()) : "0";
    });
    manager.registerServerPlaceholder("pa", "max_players", []() -> std::string {
        auto server = ll::service::getServerNetworkHandler();
        return server ? std::to_string(server->mMaxNumPlayers) : "0";
    });

    // 时间类
    manager.registerServerPlaceholderWithParams(
        "pa",
        "time",
        std::function<std::string(const Utils::ParsedParams&)>([](const Utils::ParsedParams& params) -> std::string {
            auto format_str = params.get("format").value_or("%Y-%m-%d %H:%M:%S");
            auto tz_str     = params.get("tz").value_or("");

            if (tz_str.empty()) {
                // 使用本地时区
                auto time_point = std::chrono::current_zone()->to_local(std::chrono::system_clock::now());
                return std::vformat(format_str, std::make_format_args(time_point));
            }

            int offset_hours = 0, offset_minutes = 0;
            if (sscanf_s(std::string(tz_str).c_str(), "UTC%d:%d", &offset_hours, &offset_minutes) >= 1) {
                auto offset      = std::chrono::hours(offset_hours) + std::chrono::minutes(offset_minutes);
                auto utc_now     = std::chrono::utc_clock::now();
                auto target_time = utc_now + offset;
                return std::vformat(format_str, std::make_format_args(target_time));
            }

            if (tz_str == "UTC") {
                auto time_point = std::chrono::utc_clock::now();
                return std::vformat(format_str, std::make_format_args(time_point));
            }

            // 尝试解析命名时区，例如 "Asia/Shanghai"
            try {
                auto tz = std::chrono::locate_zone(tz_str);
                auto zt = std::chrono::zoned_time(tz, std::chrono::system_clock::now());
                return std::vformat(format_str, std::make_format_args(zt));
            } catch (const std::runtime_error&) {
                // 如果时区无效，则回退到本地时间
                auto time_point = std::chrono::current_zone()->to_local(std::chrono::system_clock::now());
                return std::vformat(format_str, std::make_format_args(time_point));
            }
        })
    );

    // 兼容旧版
    auto getTimeComponent = [](const char* format_str) -> std::string {
        auto time_point = std::chrono::current_zone()->to_local(std::chrono::system_clock::now());
        return std::vformat(format_str, std::make_format_args(time_point));
    };
    manager.registerServerPlaceholder("pa", "year", [getTimeComponent]() { return getTimeComponent("%Y"); });
    manager.registerServerPlaceholder("pa", "month", [getTimeComponent]() { return getTimeComponent("%m"); });
    manager.registerServerPlaceholder("pa", "day", [getTimeComponent]() { return getTimeComponent("%d"); });
    manager.registerServerPlaceholder("pa", "hour", [getTimeComponent]() { return getTimeComponent("%H"); });
    manager.registerServerPlaceholder("pa", "minute", [getTimeComponent]() { return getTimeComponent("%M"); });
    manager.registerServerPlaceholder("pa", "second", [getTimeComponent]() { return getTimeComponent("%S"); });

    // Unix 时间戳
    manager.registerServerPlaceholder("pa", "unix_timestamp", []() -> std::string {
        auto now = std::chrono::system_clock::now();
        return std::to_string(std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count());
    });
    manager.registerServerPlaceholder("pa", "unix_timestamp_ms", []() -> std::string {
        auto now = std::chrono::system_clock::now();
        return std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
    });


    // --- 新增：服务器启动至今秒数 ---
    static const auto startSteady = std::chrono::steady_clock::now();
    manager.registerServerPlaceholder("pa", "uptime_seconds", []() -> std::string {
        auto now = std::chrono::steady_clock::now();
        auto sec = std::chrono::duration_cast<std::chrono::seconds>(now - startSteady).count();
        return std::to_string((long long)sec);
    });

    // --- 新增：随机数 ---
    // 用法：{pa:random|min=1;max=10}，支持小数与整数（输出仍由 decimals/round 等参数控制）
    manager.registerServerPlaceholderWithParams(
        "pa",
        "random",
        std::function<std::string(const Utils::ParsedParams&)>([](const Utils::ParsedParams& params) -> std::string {
            double lo = params.getDouble("min").value_or(0.0);
            double hi = params.getDouble("max").value_or(1.0);

            if (hi < lo) std::swap(hi, lo);
            static thread_local std::mt19937_64    rng{std::random_device{}()};
            std::uniform_real_distribution<double> dist(lo, hi);
            double                                 v = dist(rng);
            return std::to_string(v);
        })
    );

    // --- 新增：表达式计算 ---
    // 用法：{pa:calc|expr=1+2*(3-4)}；支持在 expr 中嵌套占位符（先求值）
    manager.registerServerPlaceholderWithParams(
        "pa",
        "calc",
        std::function<std::string(const Utils::ParsedParams&)>(
            [&](const Utils::ParsedParams& params) -> std::string {
                auto exprOpt = params.get("expr");
                if (!exprOpt) return "";

                auto expr = std::string(*exprOpt);
                // 先把 expr 中的占位符在“无上下文”下展开（如需上下文版本可使用上下文版 calc）
                auto& mgr = PlaceholderManager::getInstance();
                expr      = mgr.replacePlaceholders(expr);
                if (auto v = PA::Utils::evalMathExpression(expr, params)) return std::to_string(*v);
                return "";
            }
        )
    );

    // 如需上下文版 calc（可在 expr 中访问玩家/实体占位符），提供 Mob 基类版本
    manager.registerPlaceholderWithParams<Mob>(
        "pa",
        "calc",
        std::function<std::string(Mob*, const Utils::ParsedParams&)>(
            [&](Mob* mob, const Utils::ParsedParams& params) -> std::string {
                auto exprOpt = params.get("expr");
                if (!exprOpt) return "";

                auto  expr = std::string(*exprOpt);
                auto& mgr  = PlaceholderManager::getInstance();
                // 传入上下文（多态）展开 expr 内的占位符
                expr = mgr.replacePlaceholders(expr, mob);
                if (auto v = PA::Utils::evalMathExpression(expr, params)) return std::to_string(*v);
                return "";
            }
        )
    );
}

} // namespace PA
