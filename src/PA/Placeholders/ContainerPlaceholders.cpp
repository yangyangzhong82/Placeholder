#include "PA/Placeholders/ContainerPlaceholders.h"
#include "PA/Placeholders/CommonPlaceholderTemplates.h"

#include "mc/world/Container.h"

namespace PA {

void registerContainerPlaceholders(IPlaceholderService* svc) {
    static int kBuiltinOwnerTag = 0;
    void*      owner            = &kBuiltinOwnerTag;

    // {container_size}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ContainerContext, void (*)(const ContainerContext&, std::string&)>>(
            "{container_size}",
            +[](const ContainerContext& c, std::string& out) {
                out = "0";
                if (c.container) out = std::to_string(c.container->getContainerSize());
            }
        ),
        owner
    );

    // {container_empty_slots}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ContainerContext, void (*)(const ContainerContext&, std::string&)>>(
            "{container_empty_slots}",
            +[](const ContainerContext& c, std::string& out) {
                out = "0";
                if (c.container) out = std::to_string(c.container->getEmptySlotsCount());
            }
        ),
        owner
    );

    // {container_type_name}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ContainerContext, void (*)(const ContainerContext&, std::string&)>>(
            "{container_type_name}",
            +[](const ContainerContext& c, std::string& out) {
                out = "N/A";
                if (c.container) out = c.container->getTypeName();
            }
        ),
        owner
    );

    // {container_has_custom_name}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ContainerContext, void (*)(const ContainerContext&, std::string&)>>(
            "{container_has_custom_name}",
            +[](const ContainerContext& c, std::string& out) {
                bool hasCustomName = false;
                if (c.container) hasCustomName = c.container->hasCustomName();
                out = hasCustomName ? "true" : "false";
            }
        ),
        owner
    );

    // {container_custom_name}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ContainerContext, void (*)(const ContainerContext&, std::string&)>>(
            "{container_custom_name}",
            +[](const ContainerContext& c, std::string& out) {
                out = "N/A";
                if (c.container) out = c.container->mName.mUnredactedString; // 使用 mName.mUnredactedString 成员变量
            }
        ),
        owner
    );

    // {container_is_empty}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ContainerContext, void (*)(const ContainerContext&, std::string&)>>(
            "{container_is_empty}",
            +[](const ContainerContext& c, std::string& out) {
                bool isEmpty = true;
                if (c.container) isEmpty = c.container->isEmpty();
                out = isEmpty ? "true" : "false";
            }
        ),
        owner
    );

    // {container_item_count}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ContainerContext, void (*)(const ContainerContext&, std::string&)>>(
            "{container_item_count}",
            +[](const ContainerContext& c, std::string& out) {
                out = "0";
                if (c.container) out = std::to_string(c.container->getItemCount([](const ItemStack&){ return true; }));
            }
        ),
        owner
    );

    // {container_remaining_capacity}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ContainerContext, void (*)(const ContainerContext&, std::string&)>>(
            "{container_remaining_capacity}",
            +[](const ContainerContext& c, std::string& out) {
                out = "0";
                if (c.container) out = std::to_string(c.container->getEmptySlotsCount());
            }
        ),
        owner
    );

    // {container_item_count_type:<item_type_name1>,<item_type_name2>,...}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<
            ContainerContext,
            void (*)(const ContainerContext&, const std::vector<std::string_view>&, std::string&)>>(
            "{container_item_count_type}",
            +[](const ContainerContext& c, const std::vector<std::string_view>& args, std::string& out) {
                out = "0";
                if (!c.container || args.empty()) {
                    return;
                }

                std::vector<std::string> targetTypeNames;
                for (const auto& arg : args) {
                    targetTypeNames.emplace_back(arg);
                }

                int count = c.container->getItemCount([&](const ItemStack& item) {
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
            }
        ),
        owner
    );
}

} // namespace PA
