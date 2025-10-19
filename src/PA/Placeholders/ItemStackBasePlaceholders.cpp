#include "PA/Placeholders/ItemStackBasePlaceholders.h"
#include "PA/Placeholders/CommonPlaceholderTemplates.h"

#include "mc/world/item/ItemStackBase.h"

namespace PA {

void registerItemStackBasePlaceholders(IPlaceholderService* svc) {
    static int kBuiltinOwnerTag = 0;
    void*      owner            = &kBuiltinOwnerTag;

    // {item_name}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_name}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                out = "N/A";
                if (c.itemStackBase) out = c.itemStackBase->getDescriptionName();
            }
        ),
        owner
    );

    // {item_count}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_count}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                out = "0";
                if (c.itemStackBase) out = std::to_string(c.itemStackBase->mCount);
            }
        ),
        owner
    );

    // {item_aux_value}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_aux_value}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                out = "0";
                if (c.itemStackBase) out = std::to_string(c.itemStackBase->getAuxValue());
            }
        ),
        owner
    );

    // {item_max_stack_size}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_max_stack_size}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                out = "0";
                if (c.itemStackBase) out = std::to_string(c.itemStackBase->getMaxStackSize());
            }
        ),
        owner
    );

    // {item_is_null}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_is_null}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                bool isNull = true;
                if (c.itemStackBase) isNull = c.itemStackBase->isNull();
                out = isNull ? "true" : "false";
            }
        ),
        owner
    );

    // {item_is_enchanted}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_is_enchanted}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                bool isEnchanted = false;
                if (c.itemStackBase) isEnchanted = c.itemStackBase->isEnchanted();
                out = isEnchanted ? "true" : "false";
            }
        ),
        owner
    );

    // {item_is_damaged}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_is_damaged}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                bool isDamaged = false;
                if (c.itemStackBase) isDamaged = c.itemStackBase->isDamaged();
                out = isDamaged ? "true" : "false";
            }
        ),
        owner
    );

    // {item_damage_value}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_damage_value}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                out = "0";
                if (c.itemStackBase) out = std::to_string(c.itemStackBase->getDamageValue());
            }
        ),
        owner
    );

    // {item_max_damage}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_max_damage}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                out = "0";
                if (c.itemStackBase) out = std::to_string(c.itemStackBase->getMaxDamage());
            }
        ),
        owner
    );
}

} // namespace PA
