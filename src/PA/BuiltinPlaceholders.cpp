#include "PA/BuiltinPlaceholders.h"
#include "PA/PlaceholderAPI.h"

#include "PA/Placeholders/ActorPlaceholders.h"
#include "PA/Placeholders/ContextAliasPlaceholders.h"
#include "PA/Placeholders/MobPlaceholders.h"
#include "PA/Placeholders/PlayerPlaceholders.h"
#include "PA/Placeholders/ServerPlaceholders.h"
#include "PA/Placeholders/SystemPlaceholders.h"
#include "PA/Placeholders/TimePlaceholders.h"
#include "PA/Placeholders/BlockPlaceholders.h" // Add BlockPlaceholders header
#include "PA/Placeholders/ItemStackBasePlaceholders.h" // Add ItemStackBasePlaceholders header

namespace PA {

// 注册内置占位符
// 注意：owner 指针用于跨模块卸载时反注册。建议使用模块内唯一地址作为 owner。
void registerAllBuiltinPlaceholders(IPlaceholderService* svc) {
    registerActorPlaceholders(svc);
    registerContextAliasPlaceholders(svc);
    registerMobPlaceholders(svc);
    registerPlayerPlaceholders(svc);
    registerServerPlaceholders(svc);
    registerSystemPlaceholders(svc);
    registerTimePlaceholders(svc);
    registerBlockPlaceholders(svc); // Register BlockPlaceholders
    registerItemStackBasePlaceholders(svc); // Register ItemStackBasePlaceholders
}

} // namespace PA
