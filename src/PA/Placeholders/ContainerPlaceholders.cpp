#include "PA/Placeholders/ContainerPlaceholders.h"
#include "PA/Placeholders/CommonPlaceholderTemplates.h"

#include "mc/world/Container.h"
#include "mc/world/item/ItemStack.h"

namespace PA {

namespace {

// 26.40 移除了 Container::getItemCount(std::function) 重载，这里保持旧语义：累加匹配槽位的物品数量
template <typename Pred>
int countContainerItems(const Container& container, Pred&& pred) {
    int total = 0;
    int size  = container.getContainerSize();
    for (int i = 0; i < size; ++i) {
        const ItemStack& item = container.getItem(i);
        if (item.isNull()) continue;
        if (pred(item)) total += static_cast<int>(item.mCount);
    }
    return total;
}

} // namespace

void registerContainerPlaceholders(IPlaceholderService* svc) {
    void* owner = builtinPlaceholderOwner();

    // {container_size}
    PA_SIMPLE(svc, owner, ContainerContext, "{container_size}", {
        out = "0";
        if (c.container) out = std::to_string(c.container->getContainerSize());
    });

    // {container_empty_slots}
    PA_SIMPLE(svc, owner, ContainerContext, "{container_empty_slots}", {
        out = "0";
        if (c.container) out = std::to_string(c.container->getEmptySlotsCount());
    });

    // {container_type_name}
    PA_SIMPLE(svc, owner, ContainerContext, "{container_type_name}", {
        out = "N/A";
        if (c.container) out = c.container->getTypeName();
    });

    // {container_has_custom_name}
    PA_SIMPLE(svc, owner, ContainerContext, "{container_has_custom_name}", {
        bool hasCustomName = false;
        if (c.container) hasCustomName = c.container->hasCustomName();
        out = hasCustomName ? "true" : "false";
    });

    // {container_custom_name}
    PA_SIMPLE(svc, owner, ContainerContext, "{container_custom_name}", {
        out = "N/A";
        if (c.container) out = c.container->mName.mUnredactedString;
    });

    // {container_is_empty}
    PA_SIMPLE(svc, owner, ContainerContext, "{container_is_empty}", {
        bool isEmpty = true;
        if (c.container) isEmpty = c.container->isEmpty();
        out = isEmpty ? "true" : "false";
    });

    // {container_item_count}
    PA_SIMPLE(svc, owner, ContainerContext, "{container_item_count}", {
        out = "0";
        if (c.container) out = std::to_string(countContainerItems(*c.container, [](const ItemStack&) { return true; }));
    });

    // {container_remaining_capacity}
    PA_SIMPLE(svc, owner, ContainerContext, "{container_remaining_capacity}", {
        out = "0";
        if (c.container) out = std::to_string(c.container->getEmptySlotsCount());
    });

    // {container_item_count_type:<item_type_name1>,<item_type_name2>,...}
    PA_WITH_ARGS(svc, owner, ContainerContext, "{container_item_count_type}", {
        out = "0";
        if (!c.container || args.empty()) {
            return;
        }

        std::vector<std::string> targetTypeNames;
        for (const auto& arg : args) {
            targetTypeNames.emplace_back(arg);
        }

        int count = countContainerItems(*c.container, [&](const ItemStack& item) {
            if (item.isNull()) {
                return false;
            }
            std::string itemTypeName = item.getTypeName();
            for (const auto& targetName : targetTypeNames) {
                if (itemTypeName == targetName) {
                    return true;
                }
            }
            return false;
        });
        out = std::to_string(count);
    });
}

} // namespace PA
