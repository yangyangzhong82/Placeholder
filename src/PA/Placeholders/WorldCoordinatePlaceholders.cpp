#include "PA/Placeholders/WorldCoordinatePlaceholders.h"
#include "PA/Placeholders/CommonPlaceholderTemplates.h"

#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/actor/BlockActor.h"
#include "mc/world/level/dimension/Dimension.h"
#include "ll/api/service/Bedrock.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/BlockPos.h"

namespace PA {

void registerWorldCoordinatePlaceholders(IPlaceholderService* svc) {
    static int kBuiltinOwnerTag = 0;
    void*      owner            = &kBuiltinOwnerTag;

    // {world_pos}
    PA_SIMPLE(svc, owner, WorldCoordinateContext, "{world_pos}", {
        if (c.data) out = c.data->pos.toString();
    });

    // {world_pos_x}
    PA_SIMPLE(svc, owner, WorldCoordinateContext, "{world_pos_x}", {
        if (c.data) out = std::to_string(c.data->pos.x);
    });

    // {world_pos_y}
    PA_SIMPLE(svc, owner, WorldCoordinateContext, "{world_pos_y}", {
        if (c.data) out = std::to_string(c.data->pos.y);
    });

    // {world_pos_z}
    PA_SIMPLE(svc, owner, WorldCoordinateContext, "{world_pos_z}", {
        if (c.data) out = std::to_string(c.data->pos.z);
    });

    // {world_dimension_id}
    PA_SIMPLE(svc, owner, WorldCoordinateContext, "{world_dimension_id}", {
        if (c.data) out = std::to_string(static_cast<int>(c.data->dimensionId));
    });

    // {world_dimension_name}
    PA_SIMPLE(svc, owner, WorldCoordinateContext, "{world_dimension_name}", {
        if (!c.data) {
            out = "Invalid WorldCoordinateData";
            return;
        }
        auto level = ll::service::getLevel();
        if (level) {
            auto dimRef = level->getDimension(c.data->dimensionId);
            if (!dimRef.expired()) {
                auto dim = dimRef.lock();
                if (dim) {
                    out = dim->mName;
                } else {
                    out = "Invalid Dimension";
                }
            } else {
                out = "Invalid Dimension";
            }
        } else {
            out = "Level Not Available";
        }
    });

    // {world_block_type_name}
    PA_SIMPLE(svc, owner, WorldCoordinateContext, "{world_block_type_name}", {
        out = "N/A";
        if (!c.data) return;
        auto level = ll::service::getLevel();
        if (level) {
            auto dimRef = level->getDimension(c.data->dimensionId);
            if (!dimRef.expired()) {
                auto dim = dimRef.lock();
                if (dim) {
                    BlockSource& bs    = dim->getBlockSourceFromMainChunkSource();
                    BlockPos     bp    = BlockPos(c.data->pos);
                    const Block& block = bs.getBlock(bp);
                    if (!block.isAir()) {
                        out = block.getTypeName();
                    }
                }
            }
        }
    });

    // {world_block_actor_type_name}
    PA_SIMPLE(svc, owner, WorldCoordinateContext, "{world_block_actor_type_name}", {
        out = "N/A";
        if (!c.data) return;
        auto level = ll::service::getLevel();
        if (level) {
            auto dimRef = level->getDimension(c.data->dimensionId);
            if (!dimRef.expired()) {
                auto dim = dimRef.lock();
                if (dim) {
                    BlockSource& bs = dim->getBlockSourceFromMainChunkSource();
                    BlockPos     bp = BlockPos(c.data->pos);
                    BlockActor*  blockActor = bs.getBlockEntity(bp);
                    if (blockActor) {
                        out = blockActor->getName();
                    }
                }
            }
        }
    });

    // 注册上下文别名：{block:...}
    svc->registerContextAlias(
        "block",
        WorldCoordinateContext::kTypeId,
        BlockContext::kTypeId,
        +[](const IContext* ctx, const std::vector<std::string_view>&) -> void* {
                if (ctx->typeId() == WorldCoordinateContext::kTypeId) {
                    const auto* worldCtx = static_cast<const WorldCoordinateContext*>(ctx);
                    if (!worldCtx->data) return nullptr;
                    auto        level    = ll::service::getLevel();
                    if (level) {
                        auto dimRef = level->getDimension(worldCtx->data->dimensionId);
                        if (!dimRef.expired()) {
                            auto dim = dimRef.lock();
                            if (dim) {
                                BlockSource& bs    = dim->getBlockSourceFromMainChunkSource();
                                BlockPos     bp    = BlockPos(worldCtx->data->pos);
                                const Block& block = bs.getBlock(bp);
                                return (void*)&block;
                            } else {
                                logger.debug("Dimension is expired or invalid for dimensionId {}.", static_cast<int>(worldCtx->data->dimensionId));
                            }
                        } else {
                            logger.debug("Dimension WeakRef expired for dimensionId {}.", static_cast<int>(worldCtx->data->dimensionId));
                        }
                    } else {
                        logger.debug("Level not available.");
                    }
                } else {
                    logger.debug("Context data is null.");
                }
            return nullptr;
        },
        owner
    );

    // 注册上下文别名：{block_actor:...}
    svc->registerContextAlias(
        "block_actor",
        WorldCoordinateContext::kTypeId,
        BlockActorContext::kTypeId,
        +[](const IContext* ctx, const std::vector<std::string_view>&) -> void* {
                if (ctx->typeId() == WorldCoordinateContext::kTypeId) {
                    const auto* worldCtx = static_cast<const WorldCoordinateContext*>(ctx);
                    if (!worldCtx->data) return nullptr;
                    auto        level    = ll::service::getLevel();
                    if (level) {
                        auto dimRef = level->getDimension(worldCtx->data->dimensionId);
                        if (!dimRef.expired()) {
                            auto dim = dimRef.lock();
                            if (dim) {
                                BlockSource& bs = dim->getBlockSourceFromMainChunkSource();
                                BlockPos     bp = BlockPos(worldCtx->data->pos);
                                return (void*)bs.getBlockEntity(bp);
                            }
                        }
                    }
                }
            return nullptr;
        },
        owner
    );
}

} // namespace PA
