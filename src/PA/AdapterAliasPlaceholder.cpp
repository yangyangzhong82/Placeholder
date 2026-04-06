// src/PA/AdapterAliasPlaceholder.cpp
#include "PA/AdapterAliasPlaceholder.h"
#include "PA/ParameterParser.h"
#include "PA/PlaceholderProcessor.h"

#include <string>
#include <vector>

namespace PA {

namespace {

enum class CandidateResult {
    Matched,
    MissingTarget,
    NoMatch,
};

std::string joinSegments(const std::vector<std::string>& segments, size_t begin, size_t end, char delimiter) {
    std::string joined;
    for (size_t i = begin; i < end; ++i) {
        if (!joined.empty()) {
            joined.push_back(delimiter);
        }
        joined.append(segments[i]);
    }
    return joined;
}

std::vector<std::string_view> makeStringViews(const std::vector<std::string>& storage) {
    std::vector<std::string_view> views;
    views.reserve(storage.size());
    for (const auto& value : storage) {
        views.emplace_back(value);
    }
    return views;
}

bool canResolveInnerSpec(std::string_view innerSpec, const IContext* targetCtx, const PlaceholderRegistry& registry) {
    if (!targetCtx || innerSpec.empty()) {
        return false;
    }

    std::string content(innerSpec);
    size_t      pipePos         = content.find('|');
    std::string tokenSearchPart = pipePos == std::string::npos ? content : content.substr(0, pipePos);

    for (size_t splitPos = tokenSearchPart.length();;) {
        std::string potentialToken = tokenSearchPart.substr(0, splitPos);
        auto        lookup         = registry.findPlaceholder(potentialToken, targetCtx);

        if (lookup.placeholder) {
            if (splitPos == tokenSearchPart.length() || tokenSearchPart[splitPos] == ':') {
                return true;
            }
        }

        if (splitPos == 0) {
            break;
        }
        size_t prevColon = tokenSearchPart.rfind(':', splitPos - 1);
        if (prevColon == std::string::npos) {
            break;
        }
        splitPos = prevColon;
    }

    return false;
}

} // namespace

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

    ContextFactoryFn factory = mReg.findContextFactory(mTo);
    if (!factory) {
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

    bool sawMissingTarget = false;
    auto tryCandidate = [&](std::string resolverParamPart, std::string innerSpec) -> CandidateResult {
        if (innerSpec.empty()) {
            return CandidateResult::NoMatch;
        }

        std::vector<std::string> resolverArgStorage;
        if (!resolverParamPart.empty()) {
            resolverArgStorage = ParameterParser::splitParamString(resolverParamPart, ',');
        }
        auto resolverArgs = makeStringViews(resolverArgStorage);

        void* raw = mResolver(ctx, resolverArgs);
        if (!raw) {
            return CandidateResult::MissingTarget;
        }

        std::unique_ptr<IContext> targetCtx = factory(raw);
        if (!targetCtx || !canResolveInnerSpec(innerSpec, targetCtx.get(), mReg)) {
            return CandidateResult::NoMatch;
        }

        out = PlaceholderProcessor::process("{" + innerSpec + "}", targetCtx.get(), mReg);
        return CandidateResult::Matched;
    };

    std::vector<std::string> pipeSegments = ParameterParser::splitParamString(full_param_part, '|');
    if (pipeSegments.size() >= 2) {
        auto result = tryCandidate(pipeSegments.front(), joinSegments(pipeSegments, 1, pipeSegments.size(), '|'));
        if (result == CandidateResult::Matched) {
            return;
        }
        sawMissingTarget = sawMissingTarget || result == CandidateResult::MissingTarget;
    }

    std::vector<std::string> colonSegments = ParameterParser::splitParamString(full_param_part, ':');
    for (size_t boundary = 0; boundary < colonSegments.size(); ++boundary) {
        auto result = tryCandidate(
            joinSegments(colonSegments, 0, boundary, ':'),
            joinSegments(colonSegments, boundary, colonSegments.size(), ':')
        );
        if (result == CandidateResult::Matched) {
            return;
        }
        sawMissingTarget = sawMissingTarget || result == CandidateResult::MissingTarget;
    }

    if (sawMissingTarget) {
        out.clear();
        return;
    }

    out = PA_COLOR_RED "Usage: {" + mAlias + ":<inner_placeholder_spec>}" PA_COLOR_RESET;
}

} // namespace PA
