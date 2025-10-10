#include "PlaceholderAPI.h"
#include "ll/api/service/Bedrock.h"
#include "mc/network/ServerNetworkHandler.h"
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
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<PlayerContext, void (*)(const PlayerContext&, std::string&)>>(
            "{player_name}",
            +[](const PlayerContext& c, std::string& out) {
                out.clear();
                if (c.player) out = c.player->getRealName();
            }
        ),
        owner
    );

    // {ping}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<PlayerContext, void (*)(const PlayerContext&, std::string&)>>(
            "{ping}",
            +[](const PlayerContext& c, std::string& out) {
                out = "0";
                if (c.player) {
                    if (auto ns = c.player->getNetworkStatus()) {
                        out = std::to_string(ns->mAveragePing);
                    }
                }
            }
        ),
        owner
    );

    // {can_fly}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<MobContext, void (*)(const MobContext&, std::string&)>>(
            "{can_fly}",
            +[](const MobContext& c, std::string& out) {
                bool can = false;
                if (c.mob) can = c.mob->canFly();
                out = can ? "true" : "false";
            }
        ),
        owner
    );

    // {health}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<MobContext, void (*)(const MobContext&, std::string&)>>(
            "{health}",
            +[](const MobContext& c, std::string& out) {
                out = "0";
                if (c.mob) {
                    auto h = ActorAttribute::getHealth(c.mob->getEntityContext());
                    out    = std::to_string(h);
                }
            }
        ),
        owner
    );

    // {online_players}
    svc->registerPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{online_players}",
            +[](std::string& out) {
                auto level = ll::service::getLevel();
                out        = level ? std::to_string(level->getActivePlayerCount()) : "0";
            }
        ),
        owner
    );

    // {max_players}
    svc->registerPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{max_players}",
            +[](std::string& out) {
                auto server = ll::service::getServerNetworkHandler();
                out         = server ? std::to_string(server->mMaxNumPlayers) : "0";
            }
        ),
        owner
    );

    // 时间类占位符
    svc->registerPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{time}",
            +[](std::string& out) {
                auto               now = std::chrono::system_clock::now();
                auto               tt  = std::chrono::system_clock::to_time_t(now);
                auto               tm  = local_tm(tt);
                std::ostringstream ss;
                ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
                out = ss.str();
            }
        ),
        owner
    );

    svc->registerPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{year}",
            +[](std::string& out) {
                auto tt = std::time(nullptr);
                auto tm = local_tm(tt);
                out     = std::to_string(tm.tm_year + 1900);
            }
        ),
        owner
    );

    svc->registerPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{month}",
            +[](std::string& out) {
                auto tt = std::time(nullptr);
                auto tm = local_tm(tt);
                out     = std::to_string(tm.tm_mon + 1);
            }
        ),
        owner
    );

    svc->registerPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{day}",
            +[](std::string& out) {
                auto tt = std::time(nullptr);
                auto tm = local_tm(tt);
                out     = std::to_string(tm.tm_mday);
            }
        ),
        owner
    );

    svc->registerPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{hour}",
            +[](std::string& out) {
                auto tt = std::time(nullptr);
                auto tm = local_tm(tt);
                out     = std::to_string(tm.tm_hour);
            }
        ),
        owner
    );

    svc->registerPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{minute}",
            +[](std::string& out) {
                auto tt = std::time(nullptr);
                auto tm = local_tm(tt);
                out     = std::to_string(tm.tm_min);
            }
        ),
        owner
    );

    svc->registerPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{second}",
            +[](std::string& out) {
                auto tt = std::time(nullptr);
                auto tm = local_tm(tt);
                out     = std::to_string(tm.tm_sec);
            }
        ),
        owner
    );
}

} // namespace PA
