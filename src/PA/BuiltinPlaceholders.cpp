#include "PlaceholderAPI.h"
#include "ll/api/service/Bedrock.h"
#include "mc/network/ServerNetworkHandler.h"
#include "mc/server/ServerPlayer.h"
#include "mc/server/commands/CommandUtils.h"
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
    // {actor_is_on_ground}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_is_on_ground}",
            +[](const ActorContext& c, std::string& out) {
                bool onGround = false;
                if (c.actor) onGround = c.actor->isOnGround();
                out = onGround ? "true" : "false";
            }
        ),
        owner
    );

    // {actor_is_alive}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_is_alive}",
            +[](const ActorContext& c, std::string& out) {
                bool alive = false;
                if (c.actor) alive = c.actor->isAlive();
                out = alive ? "true" : "false";
            }
        ),
        owner
    );

    // {actor_is_invisible}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_is_invisible}",
            +[](const ActorContext& c, std::string& out) {
                bool invisible = false;
                if (c.actor) invisible = c.actor->isInvisible();
                out = invisible ? "true" : "false";
            }
        ),
        owner
    );

    // {actor_dimension_id}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_dimension_id}",
            +[](const ActorContext& c, std::string& out) {
                out = "0";
                if (c.actor) out = std::to_string(static_cast<int>(c.actor->getDimensionId()));
            }
        ),
        owner
    );

    // {actor_type_id}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_type_id}",
            +[](const ActorContext& c, std::string& out) {
                out = "0";
                if (c.actor) out = std::to_string(static_cast<int>(c.actor->getEntityTypeId()));
            }
        ),
        owner
    );
    // {actor_type_name}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_type_name}",
            +[](const ActorContext& c, std::string& out) {
                out = "0";
                if (c.actor) out = (c.actor->getTypeName());
            }
        ),
        owner
    );
    // {actor_pos}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_pos}",
            +[](const ActorContext& c, std::string& out) {
                out = "0";
                if (c.actor) out = (c.actor->getPosition().toString());
            }
        ),
        owner
    );
    // {actor_pos}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_pos_x}",
            +[](const ActorContext& c, std::string& out) {
                out = "0";
                if (c.actor) out = std::to_string(c.actor->getPosition().x);
            }
        ),
        owner
    );
    // {actor_pos_y}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_pos_y}",
            +[](const ActorContext& c, std::string& out) {
                out = "0";
                if (c.actor) out = std::to_string(c.actor->getPosition().y);
            }
        ),
        owner
    );
    // {actor_pos_z}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_pos_z}",
            +[](const ActorContext& c, std::string& out) {
                out = "0";
                if (c.actor) out = std::to_string(c.actor->getPosition().z);
            }
        ),
        owner
    );
    // {actor_unique_id}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_unique_id}",
            +[](const ActorContext& c, std::string& out) {
                out = "0";
                if (c.actor) out = std::to_string(c.actor->getOrCreateUniqueID().rawID);
            }
        ),
        owner
    );

    // {actor_is_baby}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_is_baby}",
            +[](const ActorContext& c, std::string& out) {
                bool isBaby = false;
                if (c.actor) isBaby = c.actor->isBaby();
                out = isBaby ? "true" : "false";
            }
        ),
        owner
    );

    // {actor_is_riding}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_is_riding}",
            +[](const ActorContext& c, std::string& out) {
                bool isRiding = false;
                if (c.actor) isRiding = c.actor->isRiding();
                out = isRiding ? "true" : "false";
            }
        ),
        owner
    );

    // {actor_is_tame}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_is_tame}",
            +[](const ActorContext& c, std::string& out) {
                bool isTame = false;
                if (c.actor) isTame = c.actor->isTame();
                out = isTame ? "true" : "false";
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
