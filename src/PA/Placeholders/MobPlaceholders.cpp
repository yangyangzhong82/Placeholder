#include "PA/Placeholders/MobPlaceholders.h"
#include "PA/Placeholders/CommonPlaceholderTemplates.h"

#include "mc/world/actor/Mob.h"
#include "mc/world/actor/provider/ActorAttribute.h" // Added for ActorAttribute
#include "mc/deps/ecs/gamerefs_entity/EntityContext.h" // Added for EntityContext

namespace PA {

void registerMobPlaceholders(IPlaceholderService* svc) {
    static int kBuiltinOwnerTag = 0;
    void*      owner            = &kBuiltinOwnerTag;

    // {mob_can_fly}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<MobContext, void (*)(const MobContext&, std::string&)>>(
            "{mob_can_fly}",
            +[](const MobContext& c, std::string& out) {
                bool can = false;
                if (c.mob) can = c.mob->canFly();
                out = can ? "true" : "false";
            }
        ),
        owner
    );

    // {mob_health}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<MobContext, void (*)(const MobContext&, std::string&)>>(
            "{mob_health}",
            +[](const MobContext& c, std::string& out) {
                out = "0";
                if (c.mob) {
                    auto h = ActorAttribute::getHealth(c.mob->getEntityContext());
                    out    = std::to_string(h);
                }
            }
        ),
        owner
    );

    // {mob_armor_value}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<MobContext, void (*)(const MobContext&, std::string&)>>(
            "{mob_armor_value}",
            +[](const MobContext& c, std::string& out) {
                out = "0";
                if (c.mob) {
                    out = std::to_string(c.mob->getArmorValue());
                }
            }
        ),
        owner
    );
}

} // namespace PA
