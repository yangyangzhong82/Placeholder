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
    PA_SIMPLE(svc, owner, BlockContext, "{block_type_name}", {
        out = "N/A";
        if (c.block) out = c.block->getTypeName();
    });

    // {block_data}
    PA_SIMPLE(svc, owner, BlockContext, "{block_data}", {
        out = "0";
        if (c.block) out = std::to_string(c.block->getData());
    });

    // {block_is_solid}
    PA_SIMPLE(svc, owner, BlockContext, "{block_is_solid}", {
        bool isSolid = false;
        if (c.block) isSolid = c.block->_isSolid();
        out = isSolid ? "true" : "false";
    });

    // {block_is_air}
    PA_SIMPLE(svc, owner, BlockContext, "{block_is_air}", {
        bool isAir = false;
        if (c.block) isAir = c.block->isAir();
        out = isAir ? "true" : "false";
    });

    // {block_description_id}
    PA_SIMPLE(svc, owner, BlockContext, "{block_description_id}", {
        out = "N/A";
        if (c.block) out = c.block->getDescriptionId();
    });
}

} // namespace PA
