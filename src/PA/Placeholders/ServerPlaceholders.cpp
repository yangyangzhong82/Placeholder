#include "PA/Placeholders/ServerPlaceholders.h"
#include "PA/Placeholders/CommonPlaceholderTemplates.h"

#include "ll/api/Versions.h" // 引入 Versions.h 以获取服务器版本信息
#include "ll/api/service/Bedrock.h"
#include "mc/network/ServerNetworkHandler.h"
#include "mc/world/level/Level.h"
#include "mc/deps/ecs/gamerefs_entity/EntityRegistry.h" // 提供 EntityRegistry 的完整定义
#include "mc/deps/ecs/gamerefs_entity/GameRefsEntity.h" // 确保 EntityContext 尽早被完全定义

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
    svc->registerPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{total_entities}",
            +[](std::string& out) {
                auto level = ll::service::getLevel();
                out        = level ? std::to_string(level->getEntities().size()) : "0";
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
}

} // namespace PA
