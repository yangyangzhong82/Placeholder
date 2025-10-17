#include "PA/Placeholders/PlayerPlaceholders.h"
#include "PA/Placeholders/CommonPlaceholderTemplates.h"

#include "ll/api/service/Bedrock.h"
#include "mc/deps/core/platform/BuildPlatform.h"
#include "mc/network/ServerNetworkHandler.h"
#include "mc/server/ServerPlayer.h"
#include "mc/world/level/Level.h"
#include "mc/world/scores/Objective.h"
#include "mc/world/scores/ScoreInfo.h"
#include "mc/world/scores/Scoreboard.h"
#include "mc/world/scores/ScoreboardId.h"
#include "mc/world/actor/Actor.h" // Added for ActorContext

namespace PA {

void registerPlayerPlaceholders(IPlaceholderService* svc) {
    static int kBuiltinOwnerTag = 0;
    void*      owner            = &kBuiltinOwnerTag;

    // {score}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<
            ActorContext,
            void (*)(const ActorContext&, const std::vector<std::string_view>&, std::string&)>>(
            "{score}",
            +[](const ActorContext& c, const std::vector<std::string_view>& args, std::string& out) {
                if (c.actor && !args.empty()) {
                    std::string score_name (args[0]);
                    Scoreboard& scoreboard = ll::service::getLevel()->getScoreboard();
                    Objective*  obj        = scoreboard.getObjective(score_name);
                    if (!obj) {
                        out = "0";
                        return;
                    }
                    const ScoreboardId& id         = scoreboard.getScoreboardId(*c.actor);
                    if (id.mRawID == ScoreboardId::INVALID().mRawID) {
                        // 如果玩家没有记分板ID，则分数默认为0
                        out = "0";
                        return;
                    }
                    out = std::to_string(obj->getPlayerScore(id).mValue);
                } else {
                    out = PA_COLOR_RED "Usage: {score:objective_name}" PA_COLOR_RESET; // 如果没有提供参数，返回使用说明
                }
            }
        ),
        owner
    );

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

    // {player_average_ping}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<PlayerContext, void (*)(const PlayerContext&, std::string&)>>(
            "{player_average_ping}",
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
    // {player_ping}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<PlayerContext, void (*)(const PlayerContext&, std::string&)>>(
            "{player_ping}",
            +[](const PlayerContext& c, std::string& out) {
                out = "0";
                if (c.player) {
                    if (auto ns = c.player->getNetworkStatus()) {
                        out = std::to_string(ns->mCurrentPing);
                    }
                }
            }
        ),
        owner
    );
    // {player_avgpacketloss}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<PlayerContext, void (*)(const PlayerContext&, std::string&)>>(
            "{player_avgpacketloss}",
            +[](const PlayerContext& c, std::string& out) {
                out = "0";
                if (c.player) {
                    if (auto ns = c.player->getNetworkStatus()) {
                        out = std::to_string(ns->mCurrentPacketLoss);
                    }
                }
            }
        ),
        owner
    );
    // {player_averagepacketloss}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<PlayerContext, void (*)(const PlayerContext&, std::string&)>>(
            "{player_averagepacketloss}",
            +[](const PlayerContext& c, std::string& out) {
                out = "0";
                if (c.player) {
                    if (auto ns = c.player->getNetworkStatus()) {
                        out = std::to_string(ns->mAveragePacketLoss);
                    }
                }
            }
        ),
        owner
    );

    // {player_locale_code}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<PlayerContext, void (*)(const PlayerContext&, std::string&)>>(
            "{player_locale_code}",
            +[](const PlayerContext& c, std::string& out) {
                out.clear();
                if (c.player) out = c.player->getLocaleCode();
            }
        ),
        owner
    );

    // {player_os}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<PlayerContext, void (*)(const PlayerContext&, std::string&)>>(
            "{player_os}",
            +[](const PlayerContext& c, std::string& out) {
                out = "Unknown";
                if (c.player) {
                    switch (c.player->mBuildPlatform) {
                    case BuildPlatform::Google:
                        out = "Android";
                        break;
                    case BuildPlatform::IOS:
                        out = "iOS";
                        break;
                    case BuildPlatform::Osx:
                        out = "macOS";
                        break;
                    case BuildPlatform::Amazon:
                        out = "Amazon";
                        break;
                    case BuildPlatform::Uwp:
                        out = "UWP";
                        break;
                    case BuildPlatform::Win32:
                        out = "Windows";
                        break;
                    case BuildPlatform::Dedicated:
                        out = "Dedicated";
                        break;
                    case BuildPlatform::Sony:
                        out = "PlayStation";
                        break;
                    case BuildPlatform::Nx:
                        out = "Nintendo Switch";
                        break;
                    case BuildPlatform::Xbox:
                        out = "Xbox";
                        break;
                    case BuildPlatform::Linux:
                        out = "Linux";
                        break;
                    default:
                        break;
                    }
                }
            }
        ),
        owner
    );
}

} // namespace PA
