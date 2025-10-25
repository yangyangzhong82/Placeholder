#include "PA/Placeholders/BlockActorPlaceholders.h"
#include "PA/Placeholders/CommonPlaceholderTemplates.h"

#include "mc/world/level/block/actor/BlockActor.h"
#include "mc/world/level/BlockSource.h" // For BlockSource
#include "mc/world/Container.h" // For Container

namespace PA {

void registerBlockActorPlaceholders(IPlaceholderService* svc) {
    static int kBuiltinOwnerTag = 0;
    void*      owner            = &kBuiltinOwnerTag;

    // {block_actor_pos}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<BlockActorContext, void (*)(const BlockActorContext&, std::string&)>>(
            "{block_actor_pos}",
            +[](const BlockActorContext& c, std::string& out) {
                out = "0";
                if (c.blockActor) out = (*c.blockActor->mPosition).toString();
            }
        ),
        owner
    );

    // {block_actor_pos_x}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<BlockActorContext, void (*)(const BlockActorContext&, std::string&)>>(
            "{block_actor_pos_x}",
            +[](const BlockActorContext& c, std::string& out) {
                out = "0";
                if (c.blockActor) out = std::to_string((*c.blockActor->mPosition).x);
            }
        ),
        owner
    );

    // {block_actor_pos_y}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<BlockActorContext, void (*)(const BlockActorContext&, std::string&)>>(
            "{block_actor_pos_y}",
            +[](const BlockActorContext& c, std::string& out) {
                out = "0";
                if (c.blockActor) out = std::to_string((*c.blockActor->mPosition).y);
            }
        ),
        owner
    );

    // {block_actor_pos_z}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<BlockActorContext, void (*)(const BlockActorContext&, std::string&)>>(
            "{block_actor_pos_z}",
            +[](const BlockActorContext& c, std::string& out) {
                out = "0";
                if (c.blockActor) out = std::to_string((*c.blockActor->mPosition).z);
            }
        ),
        owner
    );

    // {block_actor_type_name}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<BlockActorContext, void (*)(const BlockActorContext&, std::string&)>>(
            "{block_actor_type_name}",
            +[](const BlockActorContext& c, std::string& out) {
                out = "N/A";
                if (c.blockActor) out = c.blockActor->getName(); // getName() 是一个方法，不需要解引用
            }
        ),
        owner
    );

    // {block_actor_custom_name}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<BlockActorContext, void (*)(const BlockActorContext&, std::string&)>>(
            "{block_actor_custom_name}",
            +[](const BlockActorContext& c, std::string& out) {
                out = "N/A";
                if (c.blockActor) out = c.blockActor->mCustomName->mUnredactedString; 
            }
        ),
        owner
    );



    // {block_actor_repair_cost}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<BlockActorContext, void (*)(const BlockActorContext&, std::string&)>>(
            "{block_actor_repair_cost}",
            +[](const BlockActorContext& c, std::string& out) {
                out = "0";
                if (c.blockActor) out = std::to_string(c.blockActor->mRepairCost); 
            }
        ),
        owner
    );

    // {block_actor_has_container}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<BlockActorContext, void (*)(const BlockActorContext&, std::string&)>>(
            "{block_actor_has_container}",
            +[](const BlockActorContext& c, std::string& out) {
                bool hasContainer = false;
                if (c.blockActor) hasContainer = (c.blockActor->getContainer() != nullptr);
                out = hasContainer ? "true" : "false";
            }
        ),
        owner
    );
}

} // namespace PA
