#include "PA/Placeholders/WorldCoordinatePlaceholders.h"
#include "PA/Placeholders/CommonPlaceholderTemplates.h"

#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/actor/BlockActor.h"
#include "mc/world/level/dimension/Dimension.h" // For DimensionType to string conversion
#include "ll/api/service/Bedrock.h"             // For ll::service::getLevel()
#include "mc/world/level/Level.h"               // For Level class
#include "mc/world/level/BlockSource.h"         // Added for BlockSource
#include "mc/world/level/BlockPos.h"            // Added for BlockPos

namespace PA {

void registerWorldCoordinatePlaceholders(IPlaceholderService* svc) {
    static int kBuiltinOwnerTag = 0;
    void*      owner            = &kBuiltinOwnerTag;

    // {world_pos}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<WorldCoordinateContext, void (*)(const WorldCoordinateContext&, std::string&)>>(
            "{world_pos}",
            +[](const WorldCoordinateContext& c, std::string& out) {
                if (c.data) out = c.data->pos.toString();
            }
        ),
        owner
    );

    // {world_pos_x}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<WorldCoordinateContext, void (*)(const WorldCoordinateContext&, std::string&)>>(
            "{world_pos_x}",
            +[](const WorldCoordinateContext& c, std::string& out) {
                if (c.data) out = std::to_string(c.data->pos.x);
            }
        ),
        owner
    );

    // {world_pos_y}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<WorldCoordinateContext, void (*)(const WorldCoordinateContext&, std::string&)>>(
            "{world_pos_y}",
            +[](const WorldCoordinateContext& c, std::string& out) {
                if (c.data) out = std::to_string(c.data->pos.y);
            }
        ),
        owner
    );

    // {world_pos_z}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<WorldCoordinateContext, void (*)(const WorldCoordinateContext&, std::string&)>>(
            "{world_pos_z}",
            +[](const WorldCoordinateContext& c, std::string& out) {
                if (c.data) out = std::to_string(c.data->pos.z);
            }
        ),
        owner
    );

    // {world_dimension_id}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<WorldCoordinateContext, void (*)(const WorldCoordinateContext&, std::string&)>>(
            "{world_dimension_id}",
            +[](const WorldCoordinateContext& c, std::string& out) {
                if (c.data) out = std::to_string(static_cast<int>(c.data->dimensionId));
            }
        ),
        owner
    );

    // {world_dimension_name}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<WorldCoordinateContext, void (*)(const WorldCoordinateContext&, std::string&)>>(
            "{world_dimension_name}",
            +[](const WorldCoordinateContext& c, std::string& out) {
                if (!c.data) {
                    out = "Invalid WorldCoordinateData";
                    return;
                }
                auto level = ll::service::getLevel();
                if (level) {
                    auto dimRef = level->getDimension(c.data->dimensionId);
                    if (!dimRef.expired()) { // Check if WeakRef is not expired
                        auto dim = dimRef.lock();
                        if (dim) { // Check if shared_ptr is valid
                            out = dim->mName; // Access the std::string directly
                        } else {
                            out = "Invalid Dimension";
                        }
                    } else {
                        out = "Invalid Dimension";
                    }
                } else {
                    out = "Level Not Available";
                }
            }
        ),
        owner
    );

    // {world_block_type_name}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<WorldCoordinateContext, void (*)(const WorldCoordinateContext&, std::string&)>>(
            "{world_block_type_name}",
            +[](const WorldCoordinateContext& c, std::string& out) {
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
            }
        ),
        owner
    );

    // {world_block_actor_type_name}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<WorldCoordinateContext, void (*)(const WorldCoordinateContext&, std::string&)>>(
            "{world_block_actor_type_name}",
            +[](const WorldCoordinateContext& c, std::string& out) {
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
            }
        ),
        owner
    );

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
