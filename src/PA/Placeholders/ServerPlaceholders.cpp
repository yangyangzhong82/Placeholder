#include "PA/Placeholders/ServerPlaceholders.h"
#include "PA/Placeholders/CommonPlaceholderTemplates.h"

#include "ll/api/Versions.h"
#include "ll/api/mod/ModManagerRegistry.h"
#include "ll/api/service/Bedrock.h"
#include "mc/deps/ecs/gamerefs_entity/EntityRegistry.h"
#include "mc/deps/ecs/gamerefs_entity/GameRefsEntity.h"
#include "mc/network/ServerNetworkHandler.h"
#include "mc/server/PropertiesSettings.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/ActorType.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/storage/LevelData.h"


namespace PA {

void registerServerPlaceholders(IPlaceholderService* svc) {
    static int kBuiltinOwnerTag = 0;
    void*      owner            = &kBuiltinOwnerTag;

    // {online_players}
    PA_SERVER(svc, owner, "{online_players}", {
        auto level = ll::service::getLevel();
        out        = level ? std::to_string(level->getActivePlayerCount()) : "0";
    });

    // {max_players}
    PA_SERVER(svc, owner, "{max_players}", {
        auto server = ll::service::getServerNetworkHandler();
        out         = server ? std::to_string(server->mMaxNumPlayers) : "0";
    });

    // {total_entities} - 允许通过参数选择是否排除掉落物
    PA_SERVER_WITH_ARGS(svc, owner, "{total_entities}", {
        auto level = ll::service::getLevel();
        if (!level) {
            out = "0";
            return;
        }

        bool excludeDrops   = false;
        bool excludePlayers = false;
        for (const auto& arg : args) {
            if (arg == "exclude_drops") {
                excludeDrops = true;
            } else if (arg == "exclude_players") {
                excludePlayers = true;
            }
        }

        size_t total = 0;
        for (auto entityOwnerPtr : level->getEntities()) {
            if (auto* actor = Actor::tryGetFromEntity(*entityOwnerPtr, false)) {
                if (excludeDrops && actor->getEntityTypeId() == ActorType::ItemEntity) {
                    continue;
                }
                if (excludePlayers && actor->isPlayer()) {
                    continue;
                }
                ++total;
            }
        }
        out = std::to_string(total);
    });

    // 服务器版本占位符 (缓存 5 分钟)
    PA_SERVER_CACHED(svc, owner, "{server_version}", 300, { out = ll::getGameVersion().to_string(); });

    // 服务器协议版本占位符 (缓存 5 分钟)
    PA_SERVER_CACHED(svc, owner, "{server_protocol_version}", 300, {
        out = std::to_string(ll::getNetworkProtocolVersion());
    });

    // 加载器版本占位符 (缓存 5 分钟)
    PA_SERVER_CACHED(svc, owner, "{loader_version}", 300, { out = ll::getLoaderVersion().to_string(); });

    // {level_seed}
    PA_SERVER_CACHED(svc, owner, "{level_seed}", 300, {
        auto settings = ll::service::getPropertiesSettings();
        out           = settings ? settings->mLevelSeed : "";
    });

    // {level_name}
    PA_SERVER_CACHED(svc, owner, "{level_name}", 300, {
        auto settings = ll::service::getPropertiesSettings();
        out           = settings ? settings->mLevelName : "";
    });

    // {language}
    PA_SERVER_CACHED(svc, owner, "{language}", 300, {
        auto settings = ll::service::getPropertiesSettings();
        out           = settings ? settings->mLanguage : "";
    });

    // {server_name}
    PA_SERVER_CACHED(svc, owner, "{server_name}", 300, {
        auto settings = ll::service::getPropertiesSettings();
        out           = settings ? settings->mServerName : "";
    });

    // {server_port}
    PA_SERVER_CACHED(svc, owner, "{server_port}", 300, {
        auto settings = ll::service::getPropertiesSettings();
        out           = settings ? std::to_string(settings->mServerPort) : "0";
    });

    // {server_portv6}
    PA_SERVER_CACHED(svc, owner, "{server_portv6}", 300, {
        auto settings = ll::service::getPropertiesSettings();
        out           = settings ? std::to_string(settings->mServerPortv6) : "0";
    });

    // {server_mod_count} - 缓存 1 分钟
    PA_SERVER_CACHED(svc, owner, "{server_mod_count}", 60, {
        size_t totalModCount = 0;
        for (auto& manager : ll::mod::ModManagerRegistry::getInstance().managers()) {
            totalModCount += manager.getModCount();
        }
        out = std::to_string(totalModCount);
    });
}

} // namespace PA
