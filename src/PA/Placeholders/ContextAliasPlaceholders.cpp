#include "PA/Placeholders/ContextAliasPlaceholders.h"
#include "PA/Placeholders/CommonPlaceholderTemplates.h"

#include "PA/Placeholders/BlockPlaceholders.h" // Added for BlockContext
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/BlockPos.h"    // Added for BlockPos
#include "mc/world/level/BlockSource.h" // Added for BlockSource
#include "mc/world/level/block/Block.h" // Added for Block
#include "mc/world/phys/HitResult.h"
#include "mc/world/level/block/BlockLegacy.h" // For BlockLegacy
#include "mc/world/level/block/BlockProperty.h" // For BlockProperty
#include "mc/world/level/material/Material.h" // For MaterialType
#include "mc/util/BlockUtils.h" // For BlockUtils::isLiquidSource
#include "mc/world/actor/Actor.h" // For Actor::traceRay
#include "PA/ParameterParser.h" // For parsing arguments


namespace PA {

void registerContextAliasPlaceholders(IPlaceholderService* svc) {
    static int kBuiltinOwnerTag = 0;
    void*      owner            = &kBuiltinOwnerTag;

    // {actor_look:<inner_placeholder_spec>}
    svc->registerContextAlias(
        "actor_look",
        ActorContext::kTypeId, // 更改为 ActorContext::kTypeId
        ActorContext::kTypeId,
        +[](const PA::IContext* fromCtx, const std::vector<std::string_view>& args) -> void* {
            const auto* actorCtx = static_cast<const ActorContext*>(fromCtx);
            if (!actorCtx || !actorCtx->actor) {
                return nullptr;
            }

            Actor* actor = actorCtx->actor; // 使用 actorCtx->actor

            float maxDistance = 5.5f; // 默认值

            // 解析参数
            for (const auto& arg : args) {
                size_t separatorPos = arg.find('=');
                if (separatorPos != std::string_view::npos) {
                    std::string_view key   = arg.substr(0, separatorPos);
                    std::string_view value = arg.substr(separatorPos + 1);

                    if (key == "maxDistance") {
                        float parsedValue;
                        if (auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), parsedValue);
                            ec == std::errc()) {
                            maxDistance = parsedValue;
                        }
                    }
                }
            }

            HitResult result = actor->traceRay(maxDistance, true, false); // 使用 actor->traceRay
            auto      targetActor  = result.getEntity();
            if (targetActor) {
                logger.info(
                    "Actor {} is looking at entity type: {}",
                    actor->getTypeName(), // 使用 actor->getTypeName()
                    targetActor->getTypeName()
                );
            }
            if (result.mType == HitResultType::Entity) {
                return result.getEntity();
            }
            return nullptr;
        },
        owner
    );

    // {player_riding:<inner_placeholder_spec>}
    svc->registerContextAlias(
        "player_riding",
        PlayerContext::kTypeId,
        ActorContext::kTypeId,
        +[](const PA::IContext* fromCtx, const std::vector<std::string_view>&) -> void* {
            const auto* playerCtx = static_cast<const PlayerContext*>(fromCtx);
            if (!playerCtx || !playerCtx->player) {
                return nullptr;
            }
            if (playerCtx->player->isRiding()) {
                return playerCtx->player->getVehicle();
            }
            return nullptr;
        },
        owner
    );

    // {player_block:<inner_placeholder_spec>}
    svc->registerContextAlias(
        "player_block",
        PlayerContext::kTypeId,
        BlockContext::kTypeId,
        +[](const PA::IContext* fromCtx, const std::vector<std::string_view>&) -> void* {
            const auto* playerCtx = static_cast<const PlayerContext*>(fromCtx);
            if (!playerCtx || !playerCtx->player) {
                return nullptr;
            }
            BlockPos blockPos = BlockPos(playerCtx->player->getPosition());
            logger.debug("pos {}", blockPos.toString());
            BlockSource& bs    = playerCtx->player->getDimensionBlockSource();
            const Block& block = bs.getBlock(blockPos);
            logger.debug("block type {}", block.getTypeName());
            return (void*)&block; // 直接返回 const Block*
        },
        owner
    );

    // {entity_look_block:<inner_placeholder_spec>}
    svc->registerContextAlias(
        "entity_look_block",
        ActorContext::kTypeId,
        BlockContext::kTypeId,
        +[](const PA::IContext* fromCtx, const std::vector<std::string_view>& args) -> void* {
            const auto* actorCtx = static_cast<const ActorContext*>(fromCtx);
            if (!actorCtx || !actorCtx->actor) {
                return nullptr;
            }

            Actor* actor = actorCtx->actor;

            // 默认射线检测参数
            float maxDistance   = 5.25f;
            bool  includeLiquid = false;
            bool  solidOnly     = false;
            bool  fullOnly      = false;

            // 解析参数
            for (const auto& arg : args) {
                size_t separatorPos = arg.find('=');
                if (separatorPos != std::string_view::npos) {
                    std::string_view key   = arg.substr(0, separatorPos);
                    std::string_view value = arg.substr(separatorPos + 1);

                    if (key == "maxDistance") {
                        float parsedValue;
                        if (auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), parsedValue);
                            ec == std::errc()) {
                            maxDistance = parsedValue;
                        }
                    } else if (key == "includeLiquid") {
                        includeLiquid = (value == "true");
                    } else if (key == "solidOnly") {
                        solidOnly = (value == "true");
                    } else if (key == "fullOnly") {
                        fullOnly = (value == "true");
                    }
                }
            }

            HitResult res = actor->traceRay(
                maxDistance,
                false, // includeEntities
                true,  // includeBlocks
                [&solidOnly, &fullOnly, &includeLiquid](BlockSource const&, Block const& block, bool) {
                    if (solidOnly && !block.mCachedComponentData->mIsSolid) {
                        return false;
                    }
                    if (fullOnly && !block.isSlabBlock()) {
                        return false;
                    }
                    if (!includeLiquid && BlockUtils::isLiquidSource(block)) {
                        return false;
                    }
                    return true;
                }
            );

            if (res.mType == HitResultType::NoHit) {
                return nullptr;
            }

            BlockPos bp;
            if (includeLiquid && res.mIsHitLiquid) {
                bp = res.mLiquidPos;
            } else {
                bp = res.mBlock;
            }

            Block const& bl = actor->getDimensionBlockSource().getBlock(bp);
            // 检查是否是空气或无效方块
            if (bl.isAir() || (bl.getLegacyBlock().mProperties == BlockProperty::None && bl.getLegacyBlock().mMaterial.mType == MaterialType::Any)) {
                return nullptr;
            }
            
            return (void*)&bl; // 返回 const Block*
        },
        owner
    );

    // {player_hand:<inner_placeholder_spec>}
    svc->registerContextAlias(
        "player_hand",
        PlayerContext::kTypeId,
        ItemStackBaseContext::kTypeId, // 目标上下文类型为 ItemStackBaseContext
        +[](const PA::IContext* fromCtx, const std::vector<std::string_view>&) -> void* {
            const auto* playerCtx = static_cast<const PlayerContext*>(fromCtx);
            if (!playerCtx || !playerCtx->player) {
                return nullptr;
            }
            // getSelectedItem() 返回 ItemStack const&，需要转换为 const ItemStackBase*
            return (void*)&playerCtx->player->getSelectedItem();
        },
        owner
    );
}

} // namespace PA
