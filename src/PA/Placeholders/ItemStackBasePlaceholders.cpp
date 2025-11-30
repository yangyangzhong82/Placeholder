#include "PA/Placeholders/ItemStackBasePlaceholders.h"
#include "PA/Placeholders/CommonPlaceholderTemplates.h"

#include "mc/world/item/ItemStackBase.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemStack.h"
#include "mc/deps/core/math/Color.h"
#include "mc/world/item/ItemColor.h"
#include "mc/world/item/Rarity.h"
#include "mc/deps/core/string/HashedString.h"
#include <magic_enum.hpp>

namespace PA {

void registerItemStackBasePlaceholders(IPlaceholderService* svc) {
    static int kBuiltinOwnerTag = 0;
    void*      owner            = &kBuiltinOwnerTag;

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_name}", {
        out = "N/A";
        if (c.itemStackBase) out = c.itemStackBase->getDescriptionName();
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_count}", {
        out = "0";
        if (c.itemStackBase) out = std::to_string(c.itemStackBase->mCount);
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_aux_value}", {
        out = "0";
        if (c.itemStackBase) out = std::to_string(c.itemStackBase->getAuxValue());
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_max_stack_size}", {
        out = "0";
        if (c.itemStackBase) out = std::to_string(c.itemStackBase->getMaxStackSize());
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_is_null}", {
        bool isNull = true;
        if (c.itemStackBase) isNull = c.itemStackBase->isNull();
        out = isNull ? "true" : "false";
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_is_enchanted}", {
        bool isEnchanted = false;
        if (c.itemStackBase) isEnchanted = c.itemStackBase->isEnchanted();
        out = isEnchanted ? "true" : "false";
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_damage_value}", {
        out = "0";
        if (c.itemStackBase) out = std::to_string(c.itemStackBase->getDamageValue());
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_lore}", {
        out = "";
        if (c.itemStackBase) {
            auto lore = c.itemStackBase->getCustomLore();
            for (const auto& line : lore) {
                out += line + "\n";
            }
            if (!out.empty()) out.pop_back();
        }
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_custom_name}", {
        out = "";
        if (c.itemStackBase) out = c.itemStackBase->getCustomName();
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_id}", {
        out = "0";
        if (c.itemStackBase) out = std::to_string(c.itemStackBase->getId());
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_raw_name_id}", {
        out = "";
        if (c.itemStackBase) out = c.itemStackBase->getRawNameId();
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_description_id}", {
        out = "";
        if (c.itemStackBase) out = c.itemStackBase->getDescriptionId();
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_is_block}", {
        bool isBlock = false;
        if (c.itemStackBase) isBlock = c.itemStackBase->isBlock();
        out = isBlock ? "true" : "false";
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_is_armor}", {
        bool isArmor = false;
        if (c.itemStackBase) isArmor = c.itemStackBase->isArmorItem();
        out = isArmor ? "true" : "false";
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_is_potion}", {
        bool isPotion = false;
        if (c.itemStackBase) isPotion = c.itemStackBase->isPotionItem();
        out = isPotion ? "true" : "false";
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_type_name}", {
        out = "";
        if (c.itemStackBase) out = c.itemStackBase->getTypeName();
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_base_repair_cost}", {
        out = "0";
        if (c.itemStackBase) out = std::to_string(c.itemStackBase->getBaseRepairCost());
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_color}", {
        out = "0,0,0,0";
        if (c.itemStackBase) {
            mce::Color color = c.itemStackBase->getColor();
            out = std::to_string(static_cast<int>(color.r * 255)) + "," +
                  std::to_string(static_cast<int>(color.g * 255)) + "," +
                  std::to_string(static_cast<int>(color.b * 255)) + "," +
                  std::to_string(static_cast<int>(color.a * 255));
        }
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_has_container_data}", {
        bool hasData = false;
        if (c.itemStackBase) hasData = c.itemStackBase->hasContainerData();
        out = hasData ? "true" : "false";
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_has_custom_hover_name}", {
        bool hasName = false;
        if (c.itemStackBase) hasName = c.itemStackBase->hasCustomHoverName();
        out = hasName ? "true" : "false";
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_is_damageable_item_type}", {
        bool isDamageable = false;
        if (c.itemStackBase) isDamageable = c.itemStackBase->isDamageableItem();
        out = isDamageable ? "true" : "false";
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_is_enchanting_book}", {
        bool isBook = false;
        if (c.itemStackBase) isBook = c.itemStackBase->isEnchantingBook();
        out = isBook ? "true" : "false";
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_is_horse_armor}", {
        bool isHorseArmor = false;
        if (c.itemStackBase) isHorseArmor = c.itemStackBase->isHorseArmorItem();
        out = isHorseArmor ? "true" : "false";
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_is_humanoid_wearable_block}", {
        bool isWearableBlock = false;
        if (c.itemStackBase) isWearableBlock = c.itemStackBase->isHumanoidWearableBlockItem();
        out = isWearableBlock ? "true" : "false";
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_is_music_disk}", {
        bool isMusicDisk = false;
        if (c.itemStackBase && c.itemStackBase->getItem()) {
            isMusicDisk = c.itemStackBase->getItem()->isMusicDisk();
        }
        out = isMusicDisk ? "true" : "false";
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_is_component_based}", {
        bool isComponentBased = false;
        if (c.itemStackBase && c.itemStackBase->getItem()) {
            isComponentBased = c.itemStackBase->getItem()->isComponentBased();
        }
        out = isComponentBased ? "true" : "false";
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_is_block_planter}", {
        bool isBlockPlanter = false;
        if (c.itemStackBase && c.itemStackBase->getItem()) {
            isBlockPlanter = c.itemStackBase->getItem()->isBlockPlanterItem();
        }
        out = isBlockPlanter ? "true" : "false";
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_is_bucket}", {
        bool isBucket = false;
        if (c.itemStackBase && c.itemStackBase->getItem()) {
            isBucket = c.itemStackBase->getItem()->isBucket();
        }
        out = isBucket ? "true" : "false";
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_is_candle}", {
        bool isCandle = false;
        if (c.itemStackBase && c.itemStackBase->getItem()) {
            isCandle = c.itemStackBase->getItem()->isCandle();
        }
        out = isCandle ? "true" : "false";
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_is_dyeable}", {
        bool isDyeable = false;
        if (c.itemStackBase && c.itemStackBase->getItem()) {
            isDyeable = c.itemStackBase->getItem()->isDyeable();
        }
        out = isDyeable ? "true" : "false";
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_is_dye}", {
        bool isDye = false;
        if (c.itemStackBase && c.itemStackBase->getItem()) {
            isDye = c.itemStackBase->getItem()->isDye();
        }
        out = isDye ? "true" : "false";
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_color_enum}", {
        out = "None";
        if (c.itemStackBase && c.itemStackBase->getItem()) {
            out = magic_enum::enum_name(c.itemStackBase->getItem()->getItemColor());
        }
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_is_fertilizer}", {
        bool isFertilizer = false;
        if (c.itemStackBase && c.itemStackBase->getItem()) {
            isFertilizer = c.itemStackBase->getItem()->isFertilizer();
        }
        out = isFertilizer ? "true" : "false";
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_is_food_item_type}", {
        bool isFood = false;
        if (c.itemStackBase && c.itemStackBase->getItem()) {
            isFood = c.itemStackBase->getItem()->isFood();
        }
        out = isFood ? "true" : "false";
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_is_throwable}", {
        bool isThrowable = false;
        if (c.itemStackBase && c.itemStackBase->getItem()) {
            isThrowable = c.itemStackBase->getItem()->isThrowable();
        }
        out = isThrowable ? "true" : "false";
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_is_useable}", {
        bool isUseable = false;
        if (c.itemStackBase && c.itemStackBase->getItem()) {
            isUseable = c.itemStackBase->getItem()->isUseable();
        }
        out = isUseable ? "true" : "false";
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_is_trim_allowed}", {
        bool isTrimAllowed = false;
        if (c.itemStackBase && c.itemStackBase->getItem()) {
            isTrimAllowed = c.itemStackBase->getItem()->isTrimAllowed();
        }
        out = isTrimAllowed ? "true" : "false";
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_max_damage_type}", {
        out = "0";
        if (c.itemStackBase && c.itemStackBase->getItem()) {
            out = std::to_string(c.itemStackBase->getItem()->getMaxDamage());
        }
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_attack_damage}", {
        out = "0";
        if (c.itemStackBase && c.itemStackBase->getItem()) {
            out = std::to_string(c.itemStackBase->getItem()->getAttackDamage());
        }
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_is_hand_equipped}", {
        bool isHandEquipped = false;
        if (c.itemStackBase && c.itemStackBase->getItem()) {
            isHandEquipped = c.itemStackBase->getItem()->isHandEquipped();
        }
        out = isHandEquipped ? "true" : "false";
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_is_pattern}", {
        bool isPattern = false;
        if (c.itemStackBase && c.itemStackBase->getItem()) {
            isPattern = c.itemStackBase->getItem()->isPattern();
        }
        out = isPattern ? "true" : "false";
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_pattern_index}", {
        out = "0";
        if (c.itemStackBase && c.itemStackBase->getItem()) {
            out = std::to_string(c.itemStackBase->getItem()->getPatternIndex());
        }
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_base_rarity}", {
        out = "Common";
        if (c.itemStackBase && c.itemStackBase->getItem()) {
            out = magic_enum::enum_name(c.itemStackBase->getItem()->getBaseRarity());
        }
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_rarity}", {
        out = "Common";
        if (c.itemStackBase && c.itemStackBase->getItem()) {
            out = magic_enum::enum_name(c.itemStackBase->getItem()->getRarity(*c.itemStackBase));
        }
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_shows_durability_in_creative}", {
        bool showsDurability = false;
        if (c.itemStackBase && c.itemStackBase->getItem()) {
            showsDurability = c.itemStackBase->getItem()->showsDurabilityInCreative();
        }
        out = showsDurability ? "true" : "false";
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_is_complex}", {
        bool isComplex = false;
        if (c.itemStackBase && c.itemStackBase->getItem()) {
            isComplex = c.itemStackBase->getItem()->isComplex();
        }
        out = isComplex ? "true" : "false";
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_is_actor_placer}", {
        bool isActorPlacer = false;
        if (c.itemStackBase && c.itemStackBase->getItem()) {
            isActorPlacer = c.itemStackBase->getItem()->isActorPlacerItem();
        }
        out = isActorPlacer ? "true" : "false";
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_has_custom_color_item_type}", {
        bool hasCustomColor = false;
        if (c.itemStackBase && c.itemStackBase->getItem()) {
            hasCustomColor = c.itemStackBase->getItem()->hasCustomColor(*c.itemStackBase);
        }
        out = hasCustomColor ? "true" : "false";
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_base_color_rgb}", {
        out = "0,0,0,0";
        if (c.itemStackBase && c.itemStackBase->getItem()) {
            mce::Color color = c.itemStackBase->getItem()->getBaseColor(*reinterpret_cast<const ItemStack*>(c.itemStackBase));
            out = std::to_string(static_cast<int>(color.r * 255)) + "," +
                  std::to_string(static_cast<int>(color.g * 255)) + "," +
                  std::to_string(static_cast<int>(color.b * 255)) + "," +
                  std::to_string(static_cast<int>(color.a * 255));
        }
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_secondary_color_rgb}", {
        out = "0,0,0,0";
        if (c.itemStackBase && c.itemStackBase->getItem()) {
            mce::Color color = c.itemStackBase->getItem()->getSecondaryColor(*reinterpret_cast<const ItemStack*>(c.itemStackBase));
            out = std::to_string(static_cast<int>(color.r * 255)) + "," +
                  std::to_string(static_cast<int>(color.g * 255)) + "," +
                  std::to_string(static_cast<int>(color.b * 255)) + "," +
                  std::to_string(static_cast<int>(color.a * 255));
        }
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_can_be_charged}", {
        bool canBeCharged = false;
        if (c.itemStackBase && c.itemStackBase->getItem()) {
            canBeCharged = c.itemStackBase->getItem()->canBeCharged();
        }
        out = canBeCharged ? "true" : "false";
    });

    PA_SIMPLE(svc, owner, ItemStackBaseContext, "{item_furnace_xp_multiplier}", {
        out = "0.0";
        if (c.itemStackBase && c.itemStackBase->getItem()) {
            out = std::to_string(c.itemStackBase->getItem()->getFurnaceXPmultiplier(*c.itemStackBase));
        }
    });
}

} // namespace PA
