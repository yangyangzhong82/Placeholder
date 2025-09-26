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
#include <ctime>
#include <iomanip>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace PA {

static std::optional<double> evalExpr(std::string_view expr) {
    // 去空白
    std::string s;
    s.reserve(expr.size());
    for (char c : expr)
        if (!PA::Utils::isSpace((unsigned char)c)) s.push_back(c);
    if (s.empty()) return std::nullopt;

    // Shunting-yard
    std::vector<double> val;
    std::vector<char>   op;

    auto apply = [&](char o) {
        if (o == '~') {
            if (val.empty()) return false;
            double a = val.back();
            val.pop_back();
            val.push_back(-a);
            return true;
        }
        if (val.size() < 2) return false;
        double b = val.back();
        val.pop_back();
        double a = val.back();
        val.pop_back();
        switch (o) {
        case '+':
            val.push_back(a + b);
            break;
        case '-':
            val.push_back(a - b);
            break;
        case '*':
            val.push_back(a * b);
            break;
        case '/':
            val.push_back(b == 0.0 ? 0.0 : a / b);
            break;
        default:
            return false;
        }
        return true;
    };

    auto prec = [&](char o) -> int {
        if (o == '~') return 3;
        if (o == '*' || o == '/') return 2;
        if (o == '+' || o == '-') return 1;
        return 0;
    };

    bool mayUnary = true;
    for (size_t i = 0; i < s.size();) {
        char c = s[i];
        if (std::isdigit((unsigned char)c) || c == '.') {
            size_t j = i + 1;
            while (j < s.size() && (std::isdigit((unsigned char)s[j]) || s[j] == '.')) ++j;
            try {
                double v = std::stod(s.substr(i, j - i));
                val.push_back(v);
            } catch (...) {
                return std::nullopt;
            }
            i        = j;
            mayUnary = false;
        } else if (c == '(') {
            op.push_back(c);
            ++i;
            mayUnary = true;
        } else if (c == ')') {
            while (!op.empty() && op.back() != '(') {
                if (!apply(op.back())) return std::nullopt;
                op.pop_back();
            }
            if (op.empty() || op.back() != '(') return std::nullopt;
            op.pop_back();
            ++i;
            mayUnary = false;
        } else {
            char curOp = c;
            if ((c == '+' || c == '-') && mayUnary) {
                if (c == '-') curOp = '~'; // 一元负号
                else {                     // 一元正号，忽略
                    ++i;
                    continue;
                }
            } else if (c == '+' || c == '-' || c == '*' || c == '/') {
                // 二元
            } else {
                return std::nullopt;
            }
            while (!op.empty() && op.back() != '(' && prec(op.back()) >= prec(curOp)) {
                if (!apply(op.back())) return std::nullopt;
                op.pop_back();
            }
            op.push_back(curOp);
            ++i;
            mayUnary = true;
        }
    }
    while (!op.empty()) {
        if (op.back() == '(') return std::nullopt;
        if (!apply(op.back())) return std::nullopt;
        op.pop_back();
    }
    if (val.size() != 1) return std::nullopt;
    return val.back();
}

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

    // --- 新增：服务器启动至今秒数 ---
    static const auto startSteady = std::chrono::steady_clock::now();
    manager.registerServerPlaceholder("pa", "uptime_seconds", []() -> std::string {
        auto now = std::chrono::steady_clock::now();
        auto sec = std::chrono::duration_cast<std::chrono::seconds>(now - startSteady).count();
        return std::to_string((long long)sec);
    });

    // --- 新增：随机数 ---
    // 用法：{pa:random|min=1;max=10}，支持小数与整数（输出仍由 decimals/round 等参数控制）
    manager.registerServerPlaceholderWithParams("pa", "random", [](std::string_view params) -> std::string {
        auto   m  = PA::Utils::parseParams(std::string(params));
        double lo = 0.0, hi = 1.0;
        if (auto it = m.find("min"); it != m.end())
            if (auto v = PA::Utils::parseDouble(it->second)) lo = *v;
        if (auto it = m.find("max"); it != m.end())
            if (auto v = PA::Utils::parseDouble(it->second)) hi = *v;
        if (hi < lo) std::swap(hi, lo);
        static thread_local std::mt19937_64    rng{std::random_device{}()};
        std::uniform_real_distribution<double> dist(lo, hi);
        double                                 v = dist(rng);
        return std::to_string(v);
    });

    // --- 新增：表达式计算 ---
    // 用法：{pa:calc|expr=1+2*(3-4)}；支持在 expr 中嵌套占位符（先求值）
    manager.registerServerPlaceholderWithParams("pa", "calc", [&](std::string_view params) -> std::string {
        auto m  = PA::Utils::parseParams(std::string(params));
        auto it = m.find("expr");
        if (it == m.end()) return "";
        auto expr = it->second;
        // 先把 expr 中的占位符在“无上下文”下展开（如需上下文版本可使用上下文版 calc）
        auto& mgr = PlaceholderManager::getInstance();
        expr      = mgr.replacePlaceholders(expr);
        if (auto v = evalExpr(expr)) return std::to_string(*v);
        return "";
    });

    // 如需上下文版 calc（可在 expr 中访问玩家/实体占位符），提供 Mob 基类版本
    manager.registerPlaceholderWithParams<Mob>("pa", "calc", [&](Mob* mob, std::string_view params) -> std::string {
        (void)mob;
        auto m  = PA::Utils::parseParams(std::string(params));
        auto it = m.find("expr");
        if (it == m.end()) return "";
        auto  expr = it->second;
        auto& mgr  = PlaceholderManager::getInstance();
        // 传入上下文（多态）展开 expr 内的占位符
        expr = mgr.replacePlaceholders(expr, mob);
        if (auto v = evalExpr(expr)) return std::to_string(*v);
        return "";
    });
}

} // namespace PA
