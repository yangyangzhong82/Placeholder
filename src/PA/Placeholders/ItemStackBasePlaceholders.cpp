#include "PA/Placeholders/ItemStackBasePlaceholders.h"
#include "PA/Placeholders/CommonPlaceholderTemplates.h"

#include "mc/world/item/ItemStackBase.h"
#include "mc/world/item/Item.h" // For Item class and related enums (Rarity)
#include "mc/world/item/ItemStack.h" // For ItemStack class
#include "mc/deps/core/math/Color.h" // For mce::Color
#include "mc/world/item/ItemColor.h" // For ItemColor enum
#include "mc/world/item/Rarity.h"    // For Rarity enum
#include "mc/deps/core/string/HashedString.h" // For HashedString
#include <magic_enum.hpp>

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



    // {item_lore}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_lore}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                out = "";
                if (c.itemStackBase) {
                    auto lore = c.itemStackBase->getCustomLore();
                    for (const auto& line : lore) {
                        out += line + "\n";
                    }
                    if (!out.empty()) {
                        out.pop_back(); // 移除最后一个换行符
                    }
                }
            }
        ),
        owner
    );

    // {item_custom_name}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_custom_name}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                out = "";
                if (c.itemStackBase) out = c.itemStackBase->getCustomName();
            }
        ),
        owner
    );

    // {item_id}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_id}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                out = "0";
                if (c.itemStackBase) out = std::to_string(c.itemStackBase->getId());
            }
        ),
        owner
    );

    // {item_raw_name_id}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_raw_name_id}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                out = "";
                if (c.itemStackBase) out = c.itemStackBase->getRawNameId();
            }
        ),
        owner
    );

    // {item_description_id}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_description_id}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                out = "";
                if (c.itemStackBase) out = c.itemStackBase->getDescriptionId();
            }
        ),
        owner
    );

    // {item_is_block}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_is_block}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                bool isBlock = false;
                if (c.itemStackBase) isBlock = c.itemStackBase->isBlock();
                out = isBlock ? "true" : "false";
            }
        ),
        owner
    );

    // {item_is_armor}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_is_armor}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                bool isArmor = false;
                if (c.itemStackBase) isArmor = c.itemStackBase->isArmorItem();
                out = isArmor ? "true" : "false";
            }
        ),
        owner
    );

    // {item_is_potion}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_is_potion}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                bool isPotion = false;
                if (c.itemStackBase) isPotion = c.itemStackBase->isPotionItem();
                out = isPotion ? "true" : "false";
            }
        ),
        owner
    );

    // {item_type_name}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_type_name}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                out = "";
                if (c.itemStackBase) out = c.itemStackBase->getTypeName();
            }
        ),
        owner
    );

    // {item_base_repair_cost}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_base_repair_cost}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                out = "0";
                if (c.itemStackBase) out = std::to_string(c.itemStackBase->getBaseRepairCost());
            }
        ),
        owner
    );

    // {item_color}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_color}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                out = "0,0,0,0"; // RGBA
                if (c.itemStackBase) {
                    mce::Color color = c.itemStackBase->getColor();
                    out              = std::to_string(static_cast<int>(color.r * 255)) + "," +
                         std::to_string(static_cast<int>(color.g * 255)) + "," +
                         std::to_string(static_cast<int>(color.b * 255)) + "," +
                         std::to_string(static_cast<int>(color.a * 255));
                }
            }
        ),
        owner
    );

    // {item_has_container_data}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_has_container_data}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                bool hasData = false;
                if (c.itemStackBase) hasData = c.itemStackBase->hasContainerData();
                out = hasData ? "true" : "false";
            }
        ),
        owner
    );

    // {item_has_custom_hover_name}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_has_custom_hover_name}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                bool hasName = false;
                if (c.itemStackBase) hasName = c.itemStackBase->hasCustomHoverName();
                out = hasName ? "true" : "false";
            }
        ),
        owner
    );

    // {item_is_damageable_item_type}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_is_damageable_item_type}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                bool isDamageable = false;
                if (c.itemStackBase) isDamageable = c.itemStackBase->isDamageableItem();
                out = isDamageable ? "true" : "false";
            }
        ),
        owner
    );

    // {item_is_enchanting_book}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_is_enchanting_book}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                bool isBook = false;
                if (c.itemStackBase) isBook = c.itemStackBase->isEnchantingBook();
                out = isBook ? "true" : "false";
            }
        ),
        owner
    );





    // {item_is_horse_armor}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_is_horse_armor}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                bool isHorseArmor = false;
                if (c.itemStackBase) isHorseArmor = c.itemStackBase->isHorseArmorItem();
                out = isHorseArmor ? "true" : "false";
            }
        ),
        owner
    );


    // {item_is_humanoid_wearable_block}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_is_humanoid_wearable_block}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                bool isWearableBlock = false;
                if (c.itemStackBase) isWearableBlock = c.itemStackBase->isHumanoidWearableBlockItem();
                out = isWearableBlock ? "true" : "false";
            }
        ),
        owner
    );



 

    // {item_is_music_disk}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_is_music_disk}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                bool isMusicDisk = false;
                if (c.itemStackBase && c.itemStackBase->getItem()) {
                    isMusicDisk = c.itemStackBase->getItem()->isMusicDisk();
                }
                out = isMusicDisk ? "true" : "false";
            }
        ),
        owner
    );

    // {item_is_component_based}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_is_component_based}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                bool isComponentBased = false;
                if (c.itemStackBase && c.itemStackBase->getItem()) {
                    isComponentBased = c.itemStackBase->getItem()->isComponentBased();
                }
                out = isComponentBased ? "true" : "false";
            }
        ),
        owner
    );

    // {item_is_block_planter}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_is_block_planter}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                bool isBlockPlanter = false;
                if (c.itemStackBase && c.itemStackBase->getItem()) {
                    isBlockPlanter = c.itemStackBase->getItem()->isBlockPlanterItem();
                }
                out = isBlockPlanter ? "true" : "false";
            }
        ),
        owner
    );

    // {item_is_bucket}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_is_bucket}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                bool isBucket = false;
                if (c.itemStackBase && c.itemStackBase->getItem()) {
                    isBucket = c.itemStackBase->getItem()->isBucket();
                }
                out = isBucket ? "true" : "false";
            }
        ),
        owner
    );

    // {item_is_candle}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_is_candle}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                bool isCandle = false;
                if (c.itemStackBase && c.itemStackBase->getItem()) {
                    isCandle = c.itemStackBase->getItem()->isCandle();
                }
                out = isCandle ? "true" : "false";
            }
        ),
        owner
    );

    // {item_is_dyeable}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_is_dyeable}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                bool isDyeable = false;
                if (c.itemStackBase && c.itemStackBase->getItem()) {
                    isDyeable = c.itemStackBase->getItem()->isDyeable();
                }
                out = isDyeable ? "true" : "false";
            }
        ),
        owner
    );

    // {item_is_dye}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_is_dye}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                bool isDye = false;
                if (c.itemStackBase && c.itemStackBase->getItem()) {
                    isDye = c.itemStackBase->getItem()->isDye();
                }
                out = isDye ? "true" : "false";
            }
        ),
        owner
    );

    // {item_color_enum}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_color_enum}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                out = "None";
                if (c.itemStackBase && c.itemStackBase->getItem()) {
                    out = magic_enum::enum_name(c.itemStackBase->getItem()->getItemColor());
                }
            }
        ),
        owner
    );

    // {item_is_fertilizer}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_is_fertilizer}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                bool isFertilizer = false;
                if (c.itemStackBase && c.itemStackBase->getItem()) {
                    isFertilizer = c.itemStackBase->getItem()->isFertilizer();
                }
                out = isFertilizer ? "true" : "false";
            }
        ),
        owner
    );

    // {item_is_food_item_type}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_is_food_item_type}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                bool isFood = false;
                if (c.itemStackBase && c.itemStackBase->getItem()) {
                    isFood = c.itemStackBase->getItem()->isFood();
                }
                out = isFood ? "true" : "false";
            }
        ),
        owner
    );

    // {item_is_throwable}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_is_throwable}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                bool isThrowable = false;
                if (c.itemStackBase && c.itemStackBase->getItem()) {
                    isThrowable = c.itemStackBase->getItem()->isThrowable();
                }
                out = isThrowable ? "true" : "false";
            }
        ),
        owner
    );

    // {item_is_useable}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_is_useable}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                bool isUseable = false;
                if (c.itemStackBase && c.itemStackBase->getItem()) {
                    isUseable = c.itemStackBase->getItem()->isUseable();
                }
                out = isUseable ? "true" : "false";
            }
        ),
        owner
    );

    // {item_is_trim_allowed}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_is_trim_allowed}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                bool isTrimAllowed = false;
                if (c.itemStackBase && c.itemStackBase->getItem()) {
                    isTrimAllowed = c.itemStackBase->getItem()->isTrimAllowed();
                }
                out = isTrimAllowed ? "true" : "false";
            }
        ),
        owner
    );

    // {item_max_damage_type}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_max_damage_type}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                out = "0";
                if (c.itemStackBase && c.itemStackBase->getItem()) {
                    out = std::to_string(c.itemStackBase->getItem()->getMaxDamage());
                }
            }
        ),
        owner
    );

    // {item_attack_damage}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_attack_damage}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                out = "0";
                if (c.itemStackBase && c.itemStackBase->getItem()) {
                    out = std::to_string(c.itemStackBase->getItem()->getAttackDamage());
                }
            }
        ),
        owner
    );

    // {item_is_hand_equipped}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_is_hand_equipped}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                bool isHandEquipped = false;
                if (c.itemStackBase && c.itemStackBase->getItem()) {
                    isHandEquipped = c.itemStackBase->getItem()->isHandEquipped();
                }
                out = isHandEquipped ? "true" : "false";
            }
        ),
        owner
    );

    // {item_is_pattern}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_is_pattern}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                bool isPattern = false;
                if (c.itemStackBase && c.itemStackBase->getItem()) {
                    isPattern = c.itemStackBase->getItem()->isPattern();
                }
                out = isPattern ? "true" : "false";
            }
        ),
        owner
    );

    // {item_pattern_index}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_pattern_index}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                out = "0";
                if (c.itemStackBase && c.itemStackBase->getItem()) {
                    out = std::to_string(c.itemStackBase->getItem()->getPatternIndex());
                }
            }
        ),
        owner
    );

    // {item_base_rarity}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_base_rarity}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                out = "Common"; // Default
                if (c.itemStackBase && c.itemStackBase->getItem()) {
                    out = magic_enum::enum_name(c.itemStackBase->getItem()->getBaseRarity());
                }
            }
        ),
        owner
    );

    // {item_rarity}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_rarity}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                out = "Common"; // Default
                if (c.itemStackBase && c.itemStackBase->getItem()) {
                    out = magic_enum::enum_name(c.itemStackBase->getItem()->getRarity(*c.itemStackBase));
                }
            }
        ),
        owner
    );

    // {item_shows_durability_in_creative}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_shows_durability_in_creative}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                bool showsDurability = false;
                if (c.itemStackBase && c.itemStackBase->getItem()) {
                    showsDurability = c.itemStackBase->getItem()->showsDurabilityInCreative();
                }
                out = showsDurability ? "true" : "false";
            }
        ),
        owner
    );

    // {item_is_complex}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_is_complex}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                bool isComplex = false;
                if (c.itemStackBase && c.itemStackBase->getItem()) {
                    isComplex = c.itemStackBase->getItem()->isComplex();
                }
                out = isComplex ? "true" : "false";
            }
        ),
        owner
    );

    // {item_is_actor_placer}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_is_actor_placer}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                bool isActorPlacer = false;
                if (c.itemStackBase && c.itemStackBase->getItem()) {
                    isActorPlacer = c.itemStackBase->getItem()->isActorPlacerItem();
                }
                out = isActorPlacer ? "true" : "false";
            }
        ),
        owner
    );

    // {item_has_custom_color_item_type}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_has_custom_color_item_type}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                bool hasCustomColor = false;
                if (c.itemStackBase && c.itemStackBase->getItem()) {
                    hasCustomColor = c.itemStackBase->getItem()->hasCustomColor(*c.itemStackBase);
                }
                out = hasCustomColor ? "true" : "false";
            }
        ),
        owner
    );

    // {item_base_color_rgb}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_base_color_rgb}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                out = "0,0,0,0"; // RGBA
                if (c.itemStackBase && c.itemStackBase->getItem()) {
                    mce::Color color = c.itemStackBase->getItem()->getBaseColor(*reinterpret_cast<const ItemStack*>(c.itemStackBase));
                    out              = std::to_string(static_cast<int>(color.r * 255)) + "," +
                         std::to_string(static_cast<int>(color.g * 255)) + "," +
                         std::to_string(static_cast<int>(color.b * 255)) + "," +
                         std::to_string(static_cast<int>(color.a * 255));
                }
            }
        ),
        owner
    );

    // {item_secondary_color_rgb}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_secondary_color_rgb}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                out = "0,0,0,0"; // RGBA
                if (c.itemStackBase && c.itemStackBase->getItem()) {
                    mce::Color color = c.itemStackBase->getItem()->getSecondaryColor(*reinterpret_cast<const ItemStack*>(c.itemStackBase));
                    out              = std::to_string(static_cast<int>(color.r * 255)) + "," +
                         std::to_string(static_cast<int>(color.g * 255)) + "," +
                         std::to_string(static_cast<int>(color.b * 255)) + "," +
                         std::to_string(static_cast<int>(color.a * 255));
                }
            }
        ),
        owner
    );

    // {item_can_be_charged}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_can_be_charged}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                bool canBeCharged = false;
                if (c.itemStackBase && c.itemStackBase->getItem()) {
                    canBeCharged = c.itemStackBase->getItem()->canBeCharged();
                }
                out = canBeCharged ? "true" : "false";
            }
        ),
        owner
    );

    // {item_furnace_xp_multiplier}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ItemStackBaseContext, void (*)(const ItemStackBaseContext&, std::string&)>>(
            "{item_furnace_xp_multiplier}",
            +[](const ItemStackBaseContext& c, std::string& out) {
                out = "0.0";
                if (c.itemStackBase && c.itemStackBase->getItem()) {
                    out = std::to_string(c.itemStackBase->getItem()->getFurnaceXPmultiplier(*c.itemStackBase));
                }
            }
        ),
        owner
    );
}

} // namespace PA
