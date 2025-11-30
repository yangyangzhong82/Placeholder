#include "PA/Placeholders/ActorPlaceholders.h"
#include "PA/Placeholders/CommonPlaceholderTemplates.h"

#include "mc/deps/ecs/gamerefs_entity/EntityContext.h"
#include "mc/deps/ecs/gamerefs_entity/EntityRegistry.h"
#include "mc/deps/ecs/gamerefs_entity/GameRefsEntity.h"
#include "mc/deps/game_refs/GameRefs.h"
#include "mc/deps/game_refs/OwnerPtr.h"
#include "mc/legacy/ActorRuntimeID.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/effect/MobEffect.h"
#include "mc/world/effect/MobEffectInstance.h"

#include "mc/server/commands/CommandUtils.h"

namespace PA {

void registerActorPlaceholders(IPlaceholderService* svc) {
    static int kBuiltinOwnerTag = 0;
    void*      owner            = &kBuiltinOwnerTag;

    // {actor_is_on_ground}
    PA_SIMPLE(svc, owner, ActorContext, "{actor_is_on_ground}", {
        out = (c.actor && c.actor->isOnGround()) ? "true" : "false";
    });

    // {actor_is_alive}
    PA_SIMPLE(svc, owner, ActorContext, "{actor_is_alive}", {
        out = (c.actor && c.actor->isAlive()) ? "true" : "false";
    });

    // {actor_is_invisible}
    PA_SIMPLE(svc, owner, ActorContext, "{actor_is_invisible}", {
        out = (c.actor && c.actor->isInvisible()) ? "true" : "false";
    });

    // {actor_type_id}
    PA_SIMPLE(svc, owner, ActorContext, "{actor_type_id}", {
        out = c.actor ? std::to_string(static_cast<int>(c.actor->getEntityTypeId())) : "0";
    });

    // {actor_type_name}
    PA_SIMPLE(svc, owner, ActorContext, "{actor_type_name}", { out = c.actor ? c.actor->getTypeName() : "N/A"; });

    // {actor_pos}
    PA_SIMPLE(svc, owner, ActorContext, "{actor_pos}", {
        out = c.actor ? c.actor->getPosition().toString() : "0,0,0";
    });

    // {actor_pos_x}
    PA_SIMPLE(svc, owner, ActorContext, "{actor_pos_x}", {
        out = c.actor ? std::to_string(c.actor->getPosition().x) : "0";
    });

    // {actor_pos_y}
    PA_SIMPLE(svc, owner, ActorContext, "{actor_pos_y}", {
        out = c.actor ? std::to_string(c.actor->getPosition().y) : "0";
    });

    // {actor_pos_z}
    PA_SIMPLE(svc, owner, ActorContext, "{actor_pos_z}", {
        out = c.actor ? std::to_string(c.actor->getPosition().z) : "0";
    });

    // {actor_rotation}
    PA_SIMPLE(svc, owner, ActorContext, "{actor_rotation}", {
        out = c.actor ? c.actor->getRotation().toString() : "0,0";
    });

    // {actor_rotation_x}
    PA_SIMPLE(svc, owner, ActorContext, "{actor_rotation_x}", {
        out = c.actor ? std::to_string(c.actor->getRotation().x) : "0";
    });

    // {actor_rotation_y}
    PA_SIMPLE(svc, owner, ActorContext, "{actor_rotation_y}", {
        out = c.actor ? std::to_string(c.actor->getRotation().y) : "0";
    });

    // {actor_unique_id}
    PA_SIMPLE(svc, owner, ActorContext, "{actor_unique_id}", {
        out = c.actor ? std::to_string(c.actor->getOrCreateUniqueID().rawID) : "0";
    });

    // {actor_is_baby}
    PA_SIMPLE(svc, owner, ActorContext, "{actor_is_baby}", {
        out = (c.actor && c.actor->isBaby()) ? "true" : "false";
    });

    // {actor_is_riding}
    PA_SIMPLE(svc, owner, ActorContext, "{actor_is_riding}", {
        out = (c.actor && c.actor->isRiding()) ? "true" : "false";
    });

    // {actor_is_tame}
    PA_SIMPLE(svc, owner, ActorContext, "{actor_is_tame}", {
        out = (c.actor && c.actor->isTame()) ? "true" : "false";
    });

    // {actor_runtimeid}
    PA_SIMPLE(svc, owner, ActorContext, "{actor_runtimeid}", {
        out = c.actor ? std::to_string(c.actor->getRuntimeID().rawID) : "0";
    });

    // {actor_effects}
    PA_WITH_ARGS(svc, owner, ActorContext, "{actor_effects}", {
        out.clear();
        if (!c.actor) {
            out = "N/A";
            return;
        }

        auto& effects = c.actor->_getAllEffectsNonConst();
        if (effects.empty()) {
            out = "无药水效果";
            return;
        }

        if (args.empty()) {
            // 无参数，列出所有药水效果的显示名称
            std::ostringstream oss;
            for (const auto& effect : effects) {
                oss << effect.getDisplayName() << "; ";
            }
            out = oss.str();
        } else {
            // 有参数，查找特定药水效果
            std::string              effect_name_arg(args[0]);
            const MobEffectInstance* target_effect = nullptr;
            for (const auto& effect : effects) {
                if (effect.getDisplayName() == effect_name_arg) {
                    target_effect = &effect;
                    break;
                }
            }

            if (!target_effect) {
                out = PA_COLOR_RED "未找到药水效果: " + effect_name_arg + PA_COLOR_RESET;
                return;
            }

            if (args.size() == 1) {
                // 只提供了药水名称，返回详细信息
                std::ostringstream oss;
                oss << target_effect->getDisplayName() << " (等级: " << target_effect->mAmplifier
                    << ", 持续时间: " << (int)target_effect->mDuration->mValue << "秒)";
                out = oss.str();
            } else {
                // 提供了药水名称和属性
                std::string attribute_arg(args[1]);
                if (attribute_arg == "level") {
                    out = std::to_string(target_effect->mAmplifier);
                } else if (attribute_arg == "duration") {
                    out = std::to_string(target_effect->mDuration->mValue);
                } else if (attribute_arg == "id") {
                    out = std::to_string(target_effect->mId);
                } else if (attribute_arg == "display_name") {
                    out = target_effect->getDisplayName();
                } else {
                    out = PA_COLOR_RED "无效的属性: " + attribute_arg + PA_COLOR_RESET;
                }
            }
        }
    });

    // {actor_max_health}
    PA_SIMPLE(svc, owner, ActorContext, "{actor_max_health}", {
        out = c.actor ? std::to_string(c.actor->getMaxHealth()) : "0";
    });

    // {actor_name}
    PA_SIMPLE(svc, owner, ActorContext, "{actor_name}", {
        out = c.actor ? CommandUtils::getActorName(*c.actor) : "N/A";
    });
}

} // namespace PA
