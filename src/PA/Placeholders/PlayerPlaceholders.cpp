#include "PA/Placeholders/PlayerPlaceholders.h"
#include "PA/Placeholders/CommonPlaceholderTemplates.h"

#include "ll/api/service/Bedrock.h"
#include "mc/deps/core/platform/BuildPlatform.h"
#include "mc/network/ServerNetworkHandler.h"
#include "mc/platform/UUID.h"
#include "mc/server/ServerPlayer.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/GameType.h"
#include "mc/world/level/Level.h"
#include "mc/world/scores/Objective.h"
#include <magic_enum.hpp>
#include "mc/world/scores/ScoreInfo.h"
#include "mc/world/scores/Scoreboard.h"
#include "mc/world/scores/ScoreboardId.h"
#include "mc/world/attribute/AttributeInstance.h"
#include "mc/world/actor/provider/ActorEquipment.h"
#include "mc/world/Container.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/ItemStackBase.h"
#include "mc/deps/ecs/gamerefs_entity/EntityContext.h"

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
    PA_SIMPLE(svc, owner, PlayerContext, "{llmoney}", {
        if (c.player && sLLMoney_Get) {
            out = std::to_string(sLLMoney_Get(c.player->getXuid()));
        } else {
            out = "0"; // LegacyMoney.dll 未加载或玩家无效时返回 0
        }
    });
#endif // _WIN32

    // {score}
    PA_WITH_ARGS(svc, owner, ActorContext, "{score}", {
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
    });

    // {player_realname}
    PA_SIMPLE(svc, owner, PlayerContext, "{player_realname}", {
        out.clear();
        if (c.player) out = c.player->getRealName();
    });

    // {player_average_ping}
    PA_SIMPLE(svc, owner, PlayerContext, "{player_average_ping}", {
        out = "0";
        if (c.player) {
            if (auto ns = c.player->getNetworkStatus()) {
                out = std::to_string(ns->mAveragePing);
            }
        }
    });

    // {player_ping}
    PA_SIMPLE(svc, owner, PlayerContext, "{player_ping}", {
        out = "0";
        if (c.player) {
            if (auto ns = c.player->getNetworkStatus()) {
                out = std::to_string(ns->mCurrentPing);
            }
        }
    });

    // {player_avgpacketloss}
    PA_SIMPLE(svc, owner, PlayerContext, "{player_avgpacketloss}", {
        out = "0";
        if (c.player) {
            if (auto ns = c.player->getNetworkStatus()) {
                out = std::to_string(ns->mCurrentPacketLoss);
            }
        }
    });

    // {player_averagepacketloss}
    PA_SIMPLE(svc, owner, PlayerContext, "{player_averagepacketloss}", {
        out = "0";
        if (c.player) {
            if (auto ns = c.player->getNetworkStatus()) {
                out = std::to_string(ns->mAveragePacketLoss);
            }
        }
    });

    // {player_locale_code}
    PA_SIMPLE(svc, owner, PlayerContext, "{player_locale_code}", {
        out.clear();
        if (c.player) out = c.player->getLocaleCode();
    });

    // {player_os}
    PA_SIMPLE(svc, owner, PlayerContext, "{player_os}", {
        out = "Unknown";
        if (c.player) {
            out = magic_enum::enum_name(c.player->mBuildPlatform);
        }
    });

    // {player_uuid}
    PA_SIMPLE(svc, owner, PlayerContext, "{player_uuid}", {
        out.clear();
        if (c.player) out = c.player->getUuid().asString();
    });

    // {player_xuid}
    PA_SIMPLE(svc, owner, PlayerContext, "{player_xuid}", {
        out.clear();
        if (c.player) out = c.player->getXuid();
    });

    // {player_hunger}
    PA_SIMPLE(svc, owner, PlayerContext, "{player_hunger}", {
        out.clear();
        if (c.player) out = std::to_string(c.player->getAttribute(Player::HUNGER()).mCurrentValue);
    });

    // {player_max_hunger}
    PA_SIMPLE(svc, owner, PlayerContext, "{player_max_hunger}", {
        out.clear();
        if (c.player) out = std::to_string(c.player->getAttribute(Player::HUNGER()).mCurrentMaxValue);
    });

    // {player_saturation}
    PA_SIMPLE(svc, owner, PlayerContext, "{player_saturation}", {
        out.clear();
        if (c.player) out = std::to_string(c.player->getAttribute(Player::SATURATION()).mCurrentValue);
    });

    // {player_max_saturation}
    PA_SIMPLE(svc, owner, PlayerContext, "{player_max_saturation}", {
        out.clear();
        if (c.player) out = std::to_string(c.player->getAttribute(Player::SATURATION()).mCurrentMaxValue);
    });

    // {player_gametype}
    PA_SIMPLE(svc, owner, PlayerContext, "{player_gametype}", {
        out.clear();
        if (c.player) out = magic_enum::enum_name(c.player->getPlayerGameType());
    });

    // {player_ip}
    PA_SIMPLE(svc, owner, PlayerContext, "{player_ip}", {
        out.clear();
        if (c.player) out = c.player->getIPAndPort();
    });

    // {player_level}
    PA_SIMPLE(svc, owner, PlayerContext, "{player_level}", {
        out.clear();
        if (c.player) out = std::to_string(c.player->getAttribute(Player::LEVEL()).mCurrentValue);
    });

    // {player_offhand_item:<inner_placeholder_spec>}
    svc->registerContextAlias(
        "player_offhand_item",
        PlayerContext::kTypeId,
        ItemStackBaseContext::kTypeId,
        +[](const PA::IContext* fromCtx, const std::vector<std::string_view>&) -> void* {
            const auto* playerCtx = static_cast<const PlayerContext*>(fromCtx);
            if (!playerCtx || !playerCtx->player) {
                return nullptr;
            }
            // getOffhandSlot() 返回 ItemStack const&，需要转换为 const ItemStackBase*
            return (void*)&playerCtx->player->getOffhandSlot();
        },
        owner
    );

    // {player_armor_container:<inner_placeholder_spec>}
    svc->registerContextAlias(
        "player_armor_container",
        PlayerContext::kTypeId,
        ContainerContext::kTypeId,
        +[](const PA::IContext* fromCtx, const std::vector<std::string_view>&) -> void* {
            const auto* playerCtx = static_cast<const PlayerContext*>(fromCtx);
            if (!playerCtx || !playerCtx->player) {
                return nullptr;
            }
            // 获取玩家盔甲容器
            // ActorEquipment::getArmorContainer 返回 SimpleContainer&，SimpleContainer 继承自 Container
            return (void*)&ActorEquipment::getArmorContainer(playerCtx->player->getEntityContext());
        },
        owner
    );
}

} // namespace PA
