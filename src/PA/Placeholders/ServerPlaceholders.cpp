#include "PA/Placeholders/ServerPlaceholders.h"
#include "PA/Placeholders/CommonPlaceholderTemplates.h"

#include "ll/api/Versions.h"
#include "ll/api/service/Bedrock.h"
#include "mc/deps/ecs/gamerefs_entity/EntityRegistry.h"
#include "mc/deps/ecs/gamerefs_entity/GameRefsEntity.h"
#include "mc/network/ServerNetworkHandler.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/ActorType.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/storage/LevelData.h"
#include "mc/server/PropertiesSettings.h"


namespace PA {

void registerServerPlaceholders(IPlaceholderService* svc) {
    static int kBuiltinOwnerTag = 0;
    void*      owner            = &kBuiltinOwnerTag;

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

    // {total_entities}
    // 允许通过参数选择是否排除掉落物
    svc->registerPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&, const std::vector<std::string_view>&)>>(
            "{total_entities}",
            +[](std::string& out, const std::vector<std::string_view>& args) {
                auto level = ll::service::getLevel();
                if (!level) {
                    out = "0";
                    return;
                }

                bool excludeDrops  = false;
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
            }
        ),
        owner
    );

    // 服务器版本占位符 (缓存 5 分钟)
    svc->registerCachedPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{server_version}",
            +[](std::string& out) { out = ll::getGameVersion().to_string(); },
            300
        ),
        owner,
        300
    );

    // 服务器协议版本占位符 (缓存 5 分钟)
    svc->registerCachedPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{server_protocol_version}",
            +[](std::string& out) { out = std::to_string(ll::getNetworkProtocolVersion()); },
            300
        ),
        owner,
        300
    );

    svc->registerCachedPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{loader_version}",
            +[](std::string& out) { out = ll::getLoaderVersion().to_string(); },
            300
        ),
        owner,
        300
    );

    // {level_seed}
    svc->registerCachedPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{level_seed}",
            +[](std::string& out) {
                auto settings = ll::service::getPropertiesSettings();
                out           = settings ? settings->mLevelSeed : "";
            },
            300
        ),
        owner,
        300
    );

    // {level_name}
    svc->registerCachedPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{level_name}",
            +[](std::string& out) {
                auto settings = ll::service::getPropertiesSettings();
                out           = settings ? settings->mLevelName : "";
            },
            300
        ),
        owner,
        300
    );

    // {language}
    svc->registerCachedPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{language}",
            +[](std::string& out) {
                auto settings = ll::service::getPropertiesSettings();
                out           = settings ? settings->mLanguage : "";
            },
            300
        ),
        owner,
        300
    );

    // {server_name}
    svc->registerCachedPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{server_name}",
            +[](std::string& out) {
                auto settings = ll::service::getPropertiesSettings();
                out           = settings ? settings->mServerName : "";
            },
            300
        ),
        owner,
        300
    );

    // {server_port}
    svc->registerCachedPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{server_port}",
            +[](std::string& out) {
                auto settings = ll::service::getPropertiesSettings();
                out           = settings ? std::to_string(settings->mServerPort) : "0";
            },
            300
        ),
        owner,
        300
    );

    // {server_portv6}
    svc->registerCachedPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{server_portv6}",
            +[](std::string& out) {
                auto settings = ll::service::getPropertiesSettings();
                out           = settings ? std::to_string(settings->mServerPortv6) : "0";
            },
            300
        ),
        owner,
        300
    );
}

} // namespace PA
