#include "PA/Placeholders/ContextAliasPlaceholders.h"
#include "PA/Placeholders/CommonPlaceholderTemplates.h"

#include "PA/Placeholders/BlockPlaceholders.h" 
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/ItemStackBase.h" 
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/BlockPos.h"    
#include "mc/world/level/BlockSource.h" 
#include "mc/world/level/block/Block.h" 
#include "mc/world/phys/HitResult.h"
#include "mc/world/level/block/BlockType.h" 
#include "mc/world/level/block/BlockProperty.h" 
#include "mc/world/level/material/Material.h" 
#include "mc/util/BlockUtils.h" 
#include "mc/world/actor/Actor.h" 
#include "PA/ParameterParser.h" 
#include "mc/world/actor/player/PlayerInventory.h" 
#include "mc/world/actor/player/Inventory.h" 
#include "mc/world/level/block/actor/BlockActor.h" 


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
                    if (fullOnly && !block.getBlockType().isSlabBlock()) {
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
            if (bl.isAir()
                || (bl.getBlockType().mProperties == BlockProperty::None
                    && bl.getBlockType().mMaterial.mType == MaterialType::Any)) {
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

    // {container_slot:<slot_index>:<inner_placeholder_spec>}
    svc->registerContextAlias(
        "container_slot",
        ContainerContext::kTypeId,
        ItemStackBaseContext::kTypeId,
        +[](const PA::IContext* fromCtx, const std::vector<std::string_view>& args) -> void* {
            const auto* containerCtx = static_cast<const ContainerContext*>(fromCtx);
            if (!containerCtx || !containerCtx->container) {
                return nullptr;
            }

            int slotIndex = 0; // 默认槽位为 0

            if (!args.empty()) {
                std::string_view full_arg = args[0];
                size_t           colon_pos = full_arg.find(':');
                std::string_view slot_arg = (colon_pos != std::string_view::npos) ? full_arg.substr(0, colon_pos) : full_arg;

                int parsedValue;
                if (auto [ptr, ec] = std::from_chars(slot_arg.data(), slot_arg.data() + slot_arg.size(), parsedValue);
                    ec == std::errc()) {
                    slotIndex = parsedValue;
                }
            }

            if (slotIndex >= 0 && slotIndex < containerCtx->container->getContainerSize()) {
                // getItemNonConst returns ItemStack&, we need to return ItemStackBase*
                const ItemStack& item = containerCtx->container->getItemNonConst(slotIndex);
                if (item.isNull()) { // Check if the item stack is empty
                    return nullptr;
                }
                return (void*)&item;
            }
            return nullptr;
        },
        owner
    );

    // {item_block:<inner_placeholder_spec>}
    svc->registerContextAlias(
        "item_block",
        ItemStackBaseContext::kTypeId,
        BlockContext::kTypeId,
        +[](const PA::IContext* fromCtx, const std::vector<std::string_view>&) -> void* {
            const auto* itemStackBaseCtx = static_cast<const ItemStackBaseContext*>(fromCtx);
            if (!itemStackBaseCtx || !itemStackBaseCtx->itemStackBase) {
                return nullptr;
            }

            const ItemStackBase* itemStackBase = itemStackBaseCtx->itemStackBase;
            if (!itemStackBase || itemStackBase->isNull()) {
                return nullptr;
            }

            // 直接从 ItemStackBase 获取 Block
            const Block* block = itemStackBase->mBlock;
            if (block && !block->isAir()) { // 检查是否是有效的方块
                return (void*)block;
            }
            return nullptr;
        },
        owner
    );

    // {item_block:<inner_placeholder_spec>}
    svc->registerContextAlias(
        "item_block",
        ItemStackBaseContext::kTypeId,
        BlockContext::kTypeId,
        +[](const PA::IContext* fromCtx, const std::vector<std::string_view>&) -> void* {
            const auto* itemStackBaseCtx = static_cast<const ItemStackBaseContext*>(fromCtx);
            if (!itemStackBaseCtx || !itemStackBaseCtx->itemStackBase) {
                return nullptr;
            }

            const ItemStackBase* itemStackBase = itemStackBaseCtx->itemStackBase;
            if (!itemStackBase || itemStackBase->isNull()) {
                return nullptr;
            }

            // 直接从 ItemStackBase 获取 Block
            const Block* block = itemStackBase->mBlock;
            if (block && !block->isAir()) { // 检查是否是有效的方块
                return (void*)block;
            }
            return nullptr;
        },
        owner
    );

    // {player_inventory:<inner_placeholder_spec>}
    svc->registerContextAlias(
        "player_inventory",
        PlayerContext::kTypeId,
        ContainerContext::kTypeId,
        +[](const PA::IContext* fromCtx, const std::vector<std::string_view>&) -> void* {
            const auto* playerCtx = static_cast<const PlayerContext*>(fromCtx);
            if (!playerCtx || !playerCtx->player) {
                return nullptr;
            }
            // 获取玩家背包容器
            auto* inventory = playerCtx->player->mInventory->mInventory.get();
            return (void*)inventory;
        },
        owner
    );

    // {player_enderchest:<inner_placeholder_spec>}
    svc->registerContextAlias(
        "player_enderchest",
        PlayerContext::kTypeId,
        ContainerContext::kTypeId,
        +[](const PA::IContext* fromCtx, const std::vector<std::string_view>&) -> void* {
            const auto* playerCtx = static_cast<const PlayerContext*>(fromCtx);
            if (!playerCtx || !playerCtx->player) {
                return nullptr;
            }
            // 获取玩家末影箱容器
            return (void*)playerCtx->player->getEnderChestContainer();
        },
        owner
    );

    // {player_look_block_actor:<inner_placeholder_spec>}
    svc->registerContextAlias(
        "player_look_block_actor",
        PlayerContext::kTypeId,
        BlockActorContext::kTypeId,
        +[](const PA::IContext* fromCtx, const std::vector<std::string_view>& args) -> void* {
            const auto* playerCtx = static_cast<const PlayerContext*>(fromCtx);
            if (!playerCtx || !playerCtx->player) {
                return nullptr;
            }

            Player* player = playerCtx->player;

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

            HitResult res = player->traceRay(
                maxDistance,
                false, // includeEntities
                true,  // includeBlocks
                [&solidOnly, &fullOnly, &includeLiquid](BlockSource const&, Block const& block, bool) {
                    if (solidOnly && !block.mCachedComponentData->mIsSolid) {
                        return false;
                    }
                    if (fullOnly && !block.getBlockType().isSlabBlock()) {
                        return false;
                    }
                    if (!includeLiquid && BlockUtils::isLiquidSource(block)) {
                        return false;
                    }
                    return true;
                }
            );

            if (res.mType == HitResultType::NoHit || res.mType != HitResultType::Tile) {
                return nullptr;
            }

            BlockPos bp;
            if (includeLiquid && res.mIsHitLiquid) {
                bp = res.mLiquid;
            } else {
                bp = res.mBlock;
            }

            BlockSource& bs = player->getDimensionBlockSource();
            return (void*)bs.getBlockEntity(bp);
        },
        owner
    );

    // {player_world_coordinate:<inner_placeholder_spec>}
    svc->registerContextAlias(
        "player_world_coordinate",
        PlayerContext::kTypeId,
        WorldCoordinateContext::kTypeId,
        +[](const PA::IContext* fromCtx, const std::vector<std::string_view>&) -> void* {
            const auto* playerCtx = static_cast<const PlayerContext*>(fromCtx);
            if (!playerCtx || !playerCtx->player) {
                return nullptr;
            }

            // 创建一个 WorldCoordinateData 实例，并返回其指针
            // 注意：这里使用静态变量，存在线程安全问题，但为了匹配 ContextResolverFn 的签名，暂时如此处理
            // 更好的做法是 ContextResolverFn 返回 std::shared_ptr<WorldCoordinateData>，但需要修改 ContextResolverFn 的签名
            static WorldCoordinateData worldCoordData;
            worldCoordData.pos        = playerCtx->player->getPosition();
            worldCoordData.dimensionId = playerCtx->player->getDimensionId();

            return (void*)&worldCoordData;
        },
        owner
    );
}

} // namespace PA
