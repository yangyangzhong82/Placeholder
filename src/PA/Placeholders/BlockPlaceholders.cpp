#include "PA/Placeholders/BlockPlaceholders.h"
#include "PA/Placeholders/CommonPlaceholderTemplates.h"

#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockType.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/BlockPos.h"

namespace PA {

void registerBlockPlaceholders(IPlaceholderService* svc) {
    static int kBuiltinOwnerTag = 0;
    void*      owner            = &kBuiltinOwnerTag;

    // {block_type_name}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<BlockContext, void (*)(const BlockContext&, std::string&)>>(
            "{block_type_name}",
            +[](const BlockContext& c, std::string& out) {
                out = "N/A";
                if (c.block) out = c.block->getTypeName();
            }
        ),
        owner
    );

    // {block_data}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<BlockContext, void (*)(const BlockContext&, std::string&)>>(
            "{block_data}",
            +[](const BlockContext& c, std::string& out) {
                out = "0";
                if (c.block) out = std::to_string(c.block->getData());
            }
        ),
        owner
    );

    // {block_is_solid}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<BlockContext, void (*)(const BlockContext&, std::string&)>>(
            "{block_is_solid}",
            +[](const BlockContext& c, std::string& out) {
                bool isSolid = false;
                if (c.block) isSolid = c.block->_isSolid();
                out = isSolid ? "true" : "false";
            }
        ),
        owner
    );

    // {block_is_air}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<BlockContext, void (*)(const BlockContext&, std::string&)>>(
            "{block_is_air}",
            +[](const BlockContext& c, std::string& out) {
                bool isAir = false;
                if (c.block) isAir = c.block->isAir();
                out = isAir ? "true" : "false";
            }
        ),
        owner
    );

    // {block_description_id}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<BlockContext, void (*)(const BlockContext&, std::string&)>>(
            "{block_description_id}",
            +[](const BlockContext& c, std::string& out) {
                out = "N/A";
                if (c.block) out = c.block->getDescriptionId();
            }
        ),
        owner
    );
}

} // namespace PA
