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
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_is_on_ground}",
            +[](const ActorContext& c, std::string& out) {
                bool onGround = false;
                if (c.actor) onGround = c.actor->isOnGround();
                out = onGround ? "true" : "false";
            }
        ),
        owner
    );

    // {actor_is_alive}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_is_alive}",
            +[](const ActorContext& c, std::string& out) {
                bool alive = false;
                if (c.actor) alive = c.actor->isAlive();
                out = alive ? "true" : "false";
            }
        ),
        owner
    );

    // {actor_is_invisible}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_is_invisible}",
            +[](const ActorContext& c, std::string& out) {
                bool invisible = false;
                if (c.actor) invisible = c.actor->isInvisible();
                out = invisible ? "true" : "false";
            }
        ),
        owner
    );


    // {actor_type_id}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_type_id}",
            +[](const ActorContext& c, std::string& out) {
                out = "0";
                if (c.actor) out = std::to_string(static_cast<int>(c.actor->getEntityTypeId()));
            }
        ),
        owner
    );
    // {actor_type_name}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_type_name}",
            +[](const ActorContext& c, std::string& out) {
                out = "0";
                if (c.actor) out = (c.actor->getTypeName());
            }
        ),
        owner
    );
    // {actor_pos}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_pos}",
            +[](const ActorContext& c, std::string& out) {
                out = "0";
                if (c.actor) out = (c.actor->getPosition().toString());
            }
        ),
        owner
    );
    // {actor_pos_x}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_pos_x}",
            +[](const ActorContext& c, std::string& out) {
                out = "0";
                if (c.actor) out = std::to_string(c.actor->getPosition().x);
            }
        ),
        owner
    );
    // {actor_pos_y}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_pos_y}",
            +[](const ActorContext& c, std::string& out) {
                out = "0";
                if (c.actor) out = std::to_string(c.actor->getPosition().y);
            }
        ),
        owner
    );
    // {actor_pos_z}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_pos_z}",
            +[](const ActorContext& c, std::string& out) {
                out = "0";
                if (c.actor) out = std::to_string(c.actor->getPosition().z);
            }
        ),
        owner
    );
    // {actor_rotation}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_rotation}",
            +[](const ActorContext& c, std::string& out) {
                out = "0";
                if (c.actor) out = c.actor->getRotation().toString();
            }
        ),
        owner
    );
    // {actor_rotation_x}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_rotation_x}",
            +[](const ActorContext& c, std::string& out) {
                out = "0";
                if (c.actor) out = std::to_string(c.actor->getRotation().x);
            }
        ),
        owner
    );
    // {actor_rotation_y}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_rotation_y}",
            +[](const ActorContext& c, std::string& out) {
                out = "0";
                if (c.actor) out = std::to_string(c.actor->getRotation().y);
            }
        ),
        owner
    );
    // {actor_unique_id}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_unique_id}",
            +[](const ActorContext& c, std::string& out) {
                out = "0";
                if (c.actor) out = std::to_string(c.actor->getOrCreateUniqueID().rawID);
            }
        ),
        owner
    );

    // {actor_is_baby}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_is_baby}",
            +[](const ActorContext& c, std::string& out) {
                bool isBaby = false;
                if (c.actor) isBaby = c.actor->isBaby();
                out = isBaby ? "true" : "false";
            }
        ),
        owner
    );

    // {actor_is_riding}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_is_riding}",
            +[](const ActorContext& c, std::string& out) {
                bool isRiding = false;
                if (c.actor) isRiding = c.actor->isRiding();
                out = isRiding ? "true" : "false";
            }
        ),
        owner
    );

    // {actor_is_tame}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_is_tame}",
            +[](const ActorContext& c, std::string& out) {
                bool isTame = false;
                if (c.actor) isTame = c.actor->isTame();
                out = isTame ? "true" : "false";
            }
        ),
        owner
    );
    // {actor_runtimeid}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_runtimeid}",
            +[](const ActorContext& c, std::string& out) { out = std::to_string(c.actor->getRuntimeID().rawID); }
        ),
        owner
    );

    // {actor_effects}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<
            ActorContext,
            void (*)(const ActorContext&, const std::vector<std::string_view>&, std::string&)>>(
            "{actor_effects}",
            +[](const ActorContext& c, const std::vector<std::string_view>& args, std::string& out) {
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
                    std::string effect_name_arg(args[0]);
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
            }
        ),
        owner
    );

    // {actor_max_health}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_max_health}",
            +[](const ActorContext& c, std::string& out) {
                out = "0";
                if (c.actor) out = std::to_string(c.actor->getMaxHealth());
            }
        ),
        owner
    );

    // {actor_name}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_name}",
            +[](const ActorContext& c, std::string& out) {
                out = "N/A";
                if (c.actor) out = CommandUtils::getActorName(*c.actor);
            }
        ),
        owner
    );
}

} // namespace PA
