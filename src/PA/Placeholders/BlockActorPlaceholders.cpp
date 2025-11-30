#include "PA/Placeholders/BlockActorPlaceholders.h"
#include "PA/Placeholders/CommonPlaceholderTemplates.h"

#include "mc/world/level/block/actor/BlockActor.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/Container.h"

namespace PA {

void registerBlockActorPlaceholders(IPlaceholderService* svc) {
    static int kBuiltinOwnerTag = 0;
    void*      owner            = &kBuiltinOwnerTag;

    // {block_actor_pos}
    PA_SIMPLE(svc, owner, BlockActorContext, "{block_actor_pos}", {
        out = "0";
        if (c.blockActor) out = (*c.blockActor->mPosition).toString();
    });

    // {block_actor_pos_x}
    PA_SIMPLE(svc, owner, BlockActorContext, "{block_actor_pos_x}", {
        out = "0";
        if (c.blockActor) out = std::to_string((*c.blockActor->mPosition).x);
    });

    // {block_actor_pos_y}
    PA_SIMPLE(svc, owner, BlockActorContext, "{block_actor_pos_y}", {
        out = "0";
        if (c.blockActor) out = std::to_string((*c.blockActor->mPosition).y);
    });

    // {block_actor_pos_z}
    PA_SIMPLE(svc, owner, BlockActorContext, "{block_actor_pos_z}", {
        out = "0";
        if (c.blockActor) out = std::to_string((*c.blockActor->mPosition).z);
    });

    // {block_actor_type_name}
    PA_SIMPLE(svc, owner, BlockActorContext, "{block_actor_type_name}", {
        out = "N/A";
        if (c.blockActor) out = c.blockActor->getName();
    });

    // {block_actor_custom_name}
    PA_SIMPLE(svc, owner, BlockActorContext, "{block_actor_custom_name}", {
        out = "N/A";
        if (c.blockActor) out = c.blockActor->mCustomName->mUnredactedString;
    });

    // {block_actor_repair_cost}
    PA_SIMPLE(svc, owner, BlockActorContext, "{block_actor_repair_cost}", {
        out = "0";
        if (c.blockActor) out = std::to_string(c.blockActor->mRepairCost);
    });

    // {block_actor_has_container}
    PA_SIMPLE(svc, owner, BlockActorContext, "{block_actor_has_container}", {
        bool hasContainer = false;
        if (c.blockActor) hasContainer = (c.blockActor->getContainer() != nullptr);
        out = hasContainer ? "true" : "false";
    });
}

} // namespace PA
