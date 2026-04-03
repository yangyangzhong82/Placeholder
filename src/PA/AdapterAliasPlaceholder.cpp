// src/PA/AdapterAliasPlaceholder.cpp
#include "PA/AdapterAliasPlaceholder.h"
#include "PA/ParameterParser.h"
#include "PA/PlaceholderProcessor.h"

namespace PA {

AdapterAliasPlaceholder::AdapterAliasPlaceholder(
    std::string                alias,
    uint64_t                   fromId,
    uint64_t                   toId,
    ContextResolverFn          resolver,
    const PlaceholderRegistry& reg
)
: mAlias(std::move(alias)),
  mFrom(fromId),
  mTo(toId),
  mResolver(resolver),
  mReg(reg) {}

std::string_view AdapterAliasPlaceholder::token() const noexcept { return mAlias; }
uint64_t         AdapterAliasPlaceholder::contextTypeId() const noexcept { return mFrom; }
bool             AdapterAliasPlaceholder::isContextAliasPlaceholder() const noexcept { return true; }

void AdapterAliasPlaceholder::evaluate(const IContext* /* ctx */, std::string& out) const {
    // 无参数时无法知道要复用哪个内层占位符
    out.clear();
}

void AdapterAliasPlaceholder::evaluateWithArgs(
    const IContext*                      ctx,
    const std::vector<std::string_view>& args,
    std::string&                         out
) const {
    out.clear();
    if (!ctx || !mResolver) return;
    if (args.empty()) {
        out = PA_COLOR_RED "Usage: {" + mAlias + ":<inner_placeholder_spec>}" PA_COLOR_RESET;
        return;
    }

    // 合并所有参数为一个字符串，因为原始参数部分可能包含逗号
    std::string full_param_part;
    for (size_t i = 0; i < args.size(); ++i) {
        full_param_part.append(args[i]);
        if (i < args.size() - 1) {
            full_param_part.push_back(',');
        }
    }

    std::string_view              innerSpec_sv;
    std::vector<std::string_view> resolver_args;

    // Heuristic to distinguish parameter-less aliases from those with parameters.
    // Parameter-less aliases pass the entire parameter string as the inner spec.
    if (mAlias == "player_inventory" || mAlias == "player_enderchest" || mAlias == "player_hand"
        || mAlias == "player_riding" || mAlias == "item_block" || mAlias == "player_world_coordinate"
        || mAlias == "block" || mAlias == "block_actor") {
        innerSpec_sv = full_param_part;
    } else {
        size_t last_colon_pos = full_param_part.rfind(':');
        if (last_colon_pos != std::string::npos) {
            std::string_view resolver_param_part = std::string_view(full_param_part).substr(0, last_colon_pos);
            innerSpec_sv                         = std::string_view(full_param_part).substr(last_colon_pos + 1);

            // 使用 splitParamString 来解析 resolver 的参数
            std::vector<std::string> resolver_params_str =
                ParameterParser::splitParamString(resolver_param_part, ',');
            // 注意：这里需要一个临时的 vector 来存储 string_view，因为 splitParamString 返回 vector<string>
            // 这是一个简化的处理，理想情况下需要避免 string 拷贝
            static thread_local std::vector<std::string> arg_storage;
            arg_storage = std::move(resolver_params_str);
            for (const auto& s : arg_storage) {
                resolver_args.push_back(s);
            }

        } else {
            innerSpec_sv = full_param_part;
        }
    }

    // 1) 解析来源上下文 -> 目标底层对象指针
    void* raw = mResolver(ctx, resolver_args);
    if (!raw) {
        return;
    }

    // 2) 查找上下文工厂并构造目标上下文
    // 3) 在目标上下文下对"内层占位符表达式"做一次完整解析
    std::string innerSpec(innerSpec_sv);
    std::string wrapped = "{" + innerSpec + "}";

    ContextFactoryFn factory = mReg.findContextFactory(mTo);
    if (factory) {
        std::unique_ptr<IContext> targetCtx = factory(raw);
        if (targetCtx) {
            out = PlaceholderProcessor::process(wrapped, targetCtx.get(), mReg);
        }
    }
}

} // namespace PA
