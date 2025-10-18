#include "PA/Placeholders/PlayerPlaceholders.h"
#include "PA/Placeholders/CommonPlaceholderTemplates.h"

#include "ll/api/service/Bedrock.h"
#include "mc/deps/core/platform/BuildPlatform.h"
#include "mc/network/ServerNetworkHandler.h"
#include "mc/platform/UUID.h" // Added for Player UUID
#include "mc/server/ServerPlayer.h"
#include "mc/world/actor/Actor.h"         // Added for ActorContext
#include "mc/world/actor/player/Player.h" // Added for Player XUID
#include "mc/world/level/GameType.h" // Added for Player GameType
#include "mc/world/level/Level.h"
#include "mc/world/scores/Objective.h"
#include "mc/world/scores/ScoreInfo.h"
#include "mc/world/scores/Scoreboard.h"
#include "mc/world/scores/ScoreboardId.h"
#include "mc/world/attribute/AttributeInstance.h"

#if defined(_WIN32)
#include <windows.h>

// 定义 LegacyMoney API 的函数指针类型
typedef long long (*LLMoney_Get_t)(std::string xuid);
typedef bool (*LLMoney_Set_t)(std::string xuid, long long money);
typedef bool (*LLMoney_Trans_t)(std::string from, std::string to, long long val, std::string const& note);
typedef bool (*LLMoney_Add_t)(std::string xuid, long long money);
typedef bool (*LLMoney_Reduce_t)(std::string xuid, long long money);

// DLL 句柄和函数指针
static HMODULE          sLegacyMoneyDll = nullptr;
static LLMoney_Get_t    sLLMoney_Get    = nullptr;
static LLMoney_Set_t    sLLMoney_Set    = nullptr;
static LLMoney_Trans_t  sLLMoney_Trans  = nullptr;
static LLMoney_Add_t    sLLMoney_Add    = nullptr;
static LLMoney_Reduce_t sLLMoney_Reduce = nullptr;

// 尝试加载 LegacyMoney.dll 并获取函数指针
static void loadLegacyMoneyDll() {
    if (sLegacyMoneyDll == nullptr) {
        sLegacyMoneyDll = LoadLibraryA("LegacyMoney.dll");
        if (sLegacyMoneyDll) {
            sLLMoney_Get    = (LLMoney_Get_t)GetProcAddress(sLegacyMoneyDll, "LLMoney_Get");
            sLLMoney_Set    = (LLMoney_Set_t)GetProcAddress(sLegacyMoneyDll, "LLMoney_Set");
            sLLMoney_Trans  = (LLMoney_Trans_t)GetProcAddress(sLegacyMoneyDll, "LLMoney_Trans");
            sLLMoney_Add    = (LLMoney_Add_t)GetProcAddress(sLegacyMoneyDll, "LLMoney_Add");
            sLLMoney_Reduce = (LLMoney_Reduce_t)GetProcAddress(sLegacyMoneyDll, "LLMoney_Reduce");
            // 检查所有函数是否都已加载
            if (!sLLMoney_Get || !sLLMoney_Set || !sLLMoney_Trans || !sLLMoney_Add || !sLLMoney_Reduce) {
                FreeLibrary(sLegacyMoneyDll);
                sLegacyMoneyDll = nullptr;
            }
        }
    }
}

// 卸载 LegacyMoney.dll
static void unloadLegacyMoneyDll() {
    if (sLegacyMoneyDll) {
        FreeLibrary(sLegacyMoneyDll);
        sLegacyMoneyDll = nullptr;
        sLLMoney_Get    = nullptr;
        sLLMoney_Set    = nullptr;
        sLLMoney_Trans  = nullptr;
        sLLMoney_Add    = nullptr;
        sLLMoney_Reduce = nullptr;
    }
}

#endif // _WIN32

namespace PA {

// Helper function to convert GameType to string
static std::string gameTypeToString(GameType type) {
    switch (type) {
    case GameType::Survival:
        return "Survival";
    case GameType::Creative:
        return "Creative";
    case GameType::Adventure:
        return "Adventure";
    case GameType::Spectator:
        return "Spectator";
    case GameType::Default:
        return "Default";
    case GameType::Undefined:
    default:
        return "Undefined";
    }
}

void registerPlayerPlaceholders(IPlaceholderService* svc) {
    static int kBuiltinOwnerTag = 0;
    void*      owner            = &kBuiltinOwnerTag;

#if defined(_WIN32)
    loadLegacyMoneyDll(); // 尝试加载 LegacyMoney.dll
    static bool atexit_registered = false;
    if (!atexit_registered) {
        atexit(unloadLegacyMoneyDll);
        atexit_registered = true;
    }

    // {llmoney}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<PlayerContext, void (*)(const PlayerContext&, std::string&)>>(
            "{llmoney}",
            +[](const PlayerContext& c, std::string& out) {
                if (c.player && sLLMoney_Get) {
                    out = std::to_string(sLLMoney_Get(c.player->getXuid()));
                } else {
                    out = "0"; // LegacyMoney.dll 未加载或玩家无效时返回 0
                }
            }
        ),
        owner
    );
#endif // _WIN32

    // {score}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<
            ActorContext,
            void (*)(const ActorContext&, const std::vector<std::string_view>&, std::string&)>>(
            "{score}",
            +[](const ActorContext& c, const std::vector<std::string_view>& args, std::string& out) {
                if (c.actor && !args.empty()) {
                    std::string score_name(args[0]);
                    Scoreboard& scoreboard = ll::service::getLevel()->getScoreboard();
                    Objective*  obj        = scoreboard.getObjective(score_name);
                    if (!obj) {
                        out = "0";
                        return;
                    }
                    const ScoreboardId& id = scoreboard.getScoreboardId(*c.actor);
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

    // {player_uuid}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<PlayerContext, void (*)(const PlayerContext&, std::string&)>>(
            "{player_uuid}",
            +[](const PlayerContext& c, std::string& out) {
                out.clear();
                if (c.player) out = c.player->getUuid().asString();
            }
        ),
        owner
    );

    // {player_xuid}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<PlayerContext, void (*)(const PlayerContext&, std::string&)>>(
            "{player_xuid}",
            +[](const PlayerContext& c, std::string& out) {
                out.clear();
                if (c.player) out = c.player->getXuid();
            }
        ),
        owner
    );
    // {player_hunger}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<PlayerContext, void (*)(const PlayerContext&, std::string&)>>(
            "{player_hunger}",
            +[](const PlayerContext& c, std::string& out) {
                out.clear();
                if (c.player) out = std::to_string(c.player->getAttribute(Player::HUNGER()).mCurrentValue);
            }
        ),
        owner
    );
    // {player_max_hunger}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<PlayerContext, void (*)(const PlayerContext&, std::string&)>>(
            "{player_max_hunger}",
            +[](const PlayerContext& c, std::string& out) {
                out.clear();
                if (c.player) out = std::to_string(c.player->getAttribute(Player::HUNGER()).mCurrentMaxValue);
            }
        ),
        owner
    );
    // {player_saturation}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<PlayerContext, void (*)(const PlayerContext&, std::string&)>>(
            "{player_saturation}",
            +[](const PlayerContext& c, std::string& out) {
                out.clear();
                if (c.player) out = std::to_string(c.player->getAttribute(Player::SATURATION()).mCurrentValue);
            }
        ),
        owner
    );
    // {player_max_saturation}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<PlayerContext, void (*)(const PlayerContext&, std::string&)>>(
            "{player_max_saturation}",
            +[](const PlayerContext& c, std::string& out) {
                out.clear();
                if (c.player) out = std::to_string(c.player->getAttribute(Player::SATURATION()).mCurrentMaxValue);
            }
        ),
        owner
    );
    // {player_gametype}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<PlayerContext, void (*)(const PlayerContext&, std::string&)>>(
            "{player_gametype}",
            +[](const PlayerContext& c, std::string& out) {
                out.clear();
                if (c.player) out = gameTypeToString(c.player->getPlayerGameType());
            }
        ),
        owner
    );

    // {player_ip}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<PlayerContext, void (*)(const PlayerContext&, std::string&)>>(
            "{player_ip}",
            +[](const PlayerContext& c, std::string& out) {
                out.clear();
                if (c.player) out = c.player->getIPAndPort();
            }
        ),
        owner
    );
}

} // namespace PA
