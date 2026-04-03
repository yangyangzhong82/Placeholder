// src/PA/AdapterAliasPlaceholder.h
#pragma once

#include "PA/PlaceholderAPI.h"
#include "PA/PlaceholderRegistry.h"

#include <memory>
#include <string>
#include <vector>

namespace PA {

// 动态"别名占位符"，把来源上下文适配为目标上下文，并在目标上下文下再次解析内层表达式
class AdapterAliasPlaceholder final : public IPlaceholder {
public:
    AdapterAliasPlaceholder(
        std::string                alias,
        uint64_t                   fromId,
        uint64_t                   toId,
        ContextResolverFn          resolver,
        const PlaceholderRegistry& reg
    );

    std::string_view token() const noexcept override;
    uint64_t         contextTypeId() const noexcept override;
    bool             isContextAliasPlaceholder() const noexcept override;

    void evaluate(const IContext* ctx, std::string& out) const override;
    void evaluateWithArgs(
        const IContext*                      ctx,
        const std::vector<std::string_view>& args,
        std::string&                         out
    ) const override;

private:
    std::string                mAlias;
    uint64_t                   mFrom{};
    uint64_t                   mTo{};
    ContextResolverFn          mResolver{};
    const PlaceholderRegistry& mReg;
};

} // namespace PA
