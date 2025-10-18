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

    // {player_look:<inner_placeholder_spec>}
    svc->registerContextAlias(
        "player_look",
        PlayerContext::kTypeId,
        ActorContext::kTypeId,
        +[](const PA::IContext* fromCtx) -> void* {
            const auto* playerCtx = static_cast<const PlayerContext*>(fromCtx);
            if (!playerCtx || !playerCtx->player) {
                return nullptr;
            }
            // 默认 tMax = 5.5f, includeActor = true, includeBlock = false
            HitResult result = playerCtx->player->traceRay(5.5f, true, false);
            auto      actor  = result.getEntity();
            if (actor) {
                logger.info(
                    "Player {} is looking at entity type: {}",
                    playerCtx->player->getRealName(),
                    actor->getTypeName()
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
        +[](const PA::IContext* fromCtx) -> void* {
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
        +[](const PA::IContext* fromCtx) -> void* {
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
        +[](const PA::IContext* fromCtx) -> void* {
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
}

} // namespace PA
