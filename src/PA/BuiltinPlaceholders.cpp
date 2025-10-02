#include "PlaceholderAPI.h"
#include "ll/api/service/Bedrock.h"
#include "mc/server/ServerPlayer.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/Mob.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/actor/provider/ActorAttribute.h"
#include "mc/world/level/Level.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>


namespace {

// 泛型占位符实现（上下文型）
template <typename Ctx, typename Fn>
class TypedLambdaPlaceholder final : public PA::IPlaceholder {
public:
    TypedLambdaPlaceholder(std::string token, Fn fn) : token_(std::move(token)), fn_(std::move(fn)) {}

    std::string_view token() const noexcept override { return token_; }
    uint64_t         contextTypeId() const noexcept override { return Ctx::kTypeId; }

    void evaluate(const PA::IContext* ctx, std::string& out) const override {
        const auto* c = static_cast<const Ctx*>(ctx);
        fn_(*c, out);
    }

private:
    std::string token_;
    Fn          fn_;
};

// 服务器占位符实现（无上下文）
template <typename Fn>
class ServerLambdaPlaceholder final : public PA::IPlaceholder {
public:
    ServerLambdaPlaceholder(std::string token, Fn fn) : token_(std::move(token)), fn_(std::move(fn)) {}

    std::string_view token() const noexcept override { return token_; }
    uint64_t         contextTypeId() const noexcept override { return PA::kServerContextId; }

    void evaluate(const PA::IContext*, std::string& out) const override { fn_(out); }

private:
    std::string token_;
    Fn          fn_;
};

// time 工具
inline std::tm local_tm(std::time_t t) {
    std::tm buf{};
#ifdef _WIN32
    localtime_s(&buf, &t);
#else
    localtime_r(&t, &buf);
#endif
    return buf;
}

} // anonymous namespace

namespace PA {

// 注册内置占位符
// 注意：owner 指针用于跨模块卸载时反注册。建议使用模块内唯一地址作为 owner。
void registerBuiltinPlaceholders(IPlaceholderService* svc) {
    static int kBuiltinOwnerTag = 0;
    void*      owner            = &kBuiltinOwnerTag;

    // {player_name}
    static TypedLambdaPlaceholder<PlayerContext, void (*)(const PlayerContext&, std::string&)> PH_PLAYER_NAME(
        "{player_name}",
        +[](const PlayerContext& c, std::string& out) {
            out.clear();
            if (c.player) out = c.player->getRealName();
        }
    );
    svc->registerPlaceholder(&PH_PLAYER_NAME, owner);

    // {ping}
    static TypedLambdaPlaceholder<PlayerContext, void (*)(const PlayerContext&, std::string&)> PH_PING(
        "{ping}",
        +[](const PlayerContext& c, std::string& out) {
            out = "0";
            if (c.player) {
                if (auto ns = c.player->getNetworkStatus()) {
                    out = std::to_string(ns->mAveragePing);
                }
            }
        }
    );
    svc->registerPlaceholder(&PH_PING, owner);

    // {can_fly}
    static TypedLambdaPlaceholder<MobContext, void (*)(const MobContext&, std::string&)> PH_CAN_FLY(
        "{can_fly}",
        +[](const MobContext& c, std::string& out) {
            bool can = false;
            if (c.mob) can = c.mob->canFly();
            out = can ? "true" : "false";
        }
    );
    svc->registerPlaceholder(&PH_CAN_FLY, owner);

    // {health}
    static TypedLambdaPlaceholder<MobContext, void (*)(const MobContext&, std::string&)> PH_HEALTH(
        "{health}",
        +[](const MobContext& c, std::string& out) {
            out = "0";
            if (c.mob) {
                auto h = ActorAttribute::getHealth(c.mob->getEntityContext());
                out    = std::to_string(h);
            }
        }
    );
    svc->registerPlaceholder(&PH_HEALTH, owner);

    // {online_players}
    static ServerLambdaPlaceholder<void (*)(std::string&)> PH_ONLINE(
        "{online_players}",
        +[](std::string& out) {
            auto level = ll::service::getLevel();
            out        = level ? std::to_string(level->getActivePlayerCount()) : "0";
        }
    );
    svc->registerPlaceholder(&PH_ONLINE, owner);

    // {max_players}（示例中仍返回当前激活数，如有真实上限 API 可替换）
    static ServerLambdaPlaceholder<void (*)(std::string&)> PH_MAX(
        "{max_players}",
        +[](std::string& out) {
            auto level = ll::service::getLevel();
            out        = level ? std::to_string(level->getActivePlayerCount()) : "0";
        }
    );
    svc->registerPlaceholder(&PH_MAX, owner);

    // 时间类占位符
    static ServerLambdaPlaceholder<void (*)(std::string&)> PH_TIME(
        "{time}",
        +[](std::string& out) {
            auto               now = std::chrono::system_clock::now();
            auto               tt  = std::chrono::system_clock::to_time_t(now);
            auto               tm  = local_tm(tt);
            std::ostringstream ss;
            ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
            out = ss.str();
        }
    );
    svc->registerPlaceholder(&PH_TIME, owner);

    static ServerLambdaPlaceholder<void (*)(std::string&)> PH_YEAR(
        "{year}",
        +[](std::string& out) {
            auto tt = std::time(nullptr);
            auto tm = local_tm(tt);
            out     = std::to_string(tm.tm_year + 1900);
        }
    );
    svc->registerPlaceholder(&PH_YEAR, owner);

    static ServerLambdaPlaceholder<void (*)(std::string&)> PH_MONTH(
        "{month}",
        +[](std::string& out) {
            auto tt = std::time(nullptr);
            auto tm = local_tm(tt);
            out     = std::to_string(tm.tm_mon + 1);
        }
    );
    svc->registerPlaceholder(&PH_MONTH, owner);

    static ServerLambdaPlaceholder<void (*)(std::string&)> PH_DAY(
        "{day}",
        +[](std::string& out) {
            auto tt = std::time(nullptr);
            auto tm = local_tm(tt);
            out     = std::to_string(tm.tm_mday);
        }
    );
    svc->registerPlaceholder(&PH_DAY, owner);

    static ServerLambdaPlaceholder<void (*)(std::string&)> PH_HOUR(
        "{hour}",
        +[](std::string& out) {
            auto tt = std::time(nullptr);
            auto tm = local_tm(tt);
            out     = std::to_string(tm.tm_hour);
        }
    );
    svc->registerPlaceholder(&PH_HOUR, owner);

    static ServerLambdaPlaceholder<void (*)(std::string&)> PH_MINUTE(
        "{minute}",
        +[](std::string& out) {
            auto tt = std::time(nullptr);
            auto tm = local_tm(tt);
            out     = std::to_string(tm.tm_min);
        }
    );
    svc->registerPlaceholder(&PH_MINUTE, owner);

    static ServerLambdaPlaceholder<void (*)(std::string&)> PH_SECOND(
        "{second}",
        +[](std::string& out) {
            auto tt = std::time(nullptr);
            auto tm = local_tm(tt);
            out     = std::to_string(tm.tm_sec);
        }
    );
    svc->registerPlaceholder(&PH_SECOND, owner);
}

} // namespace PA
