#include "PA/Placeholders/MobPlaceholders.h"
#include "PA/Placeholders/CommonPlaceholderTemplates.h"

#include "mc/world/actor/Mob.h"
#include "mc/world/actor/provider/ActorAttribute.h"
#include "mc/deps/ecs/gamerefs_entity/EntityContext.h"

namespace PA {

void registerMobPlaceholders(IPlaceholderService* svc) {
    static int kBuiltinOwnerTag = 0;
    void*      owner            = &kBuiltinOwnerTag;

    // {mob_can_fly}
    PA_SIMPLE(svc, owner, MobContext, "{mob_can_fly}", {
        bool can = false;
        if (c.mob) can = c.mob->canFly();
        out = can ? "true" : "false";
    });

    // {mob_health}
    PA_SIMPLE(svc, owner, MobContext, "{mob_health}", {
        out = "0";
        if (c.mob) {
            auto h = ActorAttribute::getHealth(c.mob->getEntityContext());
            out    = std::to_string(h);
        }
    });

    // {mob_armor_value}
    PA_SIMPLE(svc, owner, MobContext, "{mob_armor_value}", {
        out = "0";
        if (c.mob) {
            out = std::to_string(c.mob->getArmorValue());
        }
    });
}

} // namespace PA
