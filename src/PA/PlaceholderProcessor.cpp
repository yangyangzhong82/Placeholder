// src/PA/PlaceholderProcessor.cpp
#include "PA/PlaceholderProcessor.h"
#include "PA/PlaceholderRegistry.h"
#include "PA/ParameterParser.h"
#include <vector>
#include <regex>

namespace PA {

// Regex to find placeholders like %placeholder_name:param% or {placeholder_name:param}
// It captures the token name and optional parameters.
static const std::regex placeholderRegex(R"((%|\{)([^%\{\}:]+)(?::([^%\{\}]+))?(%|\}))");

std::string PlaceholderProcessor::process(std::string_view text, const IContext* ctx, const PlaceholderRegistry& registry) {
    std::string result;
    result.reserve(text.length());
    auto last_match_end = text.begin();

    auto it = std::cregex_iterator(text.data(), text.data() + text.size(), placeholderRegex);
    auto end = std::cregex_iterator();

    for (; it != end; ++it) {
        const std::cmatch& match = *it;
        result.append(last_match_end, text.begin() + match.position(0));
        last_match_end = text.begin() + match.position(0) + match.length(0);

        std::string open_delim  = match[1].str();
        std::string token       = match[2].str();
        std::string param_part  = match[3].str();
        std::string close_delim = match[4].str();

        // Basic validation: delimiters must match (%...% or {...})
        if ((open_delim == "%" && close_delim != "%") || (open_delim == "{" && close_delim != "}")) {
            result.append(match[0].str());
            continue;
        }

        const IPlaceholder* ph = registry.findPlaceholder(token, ctx);
        if (ph) {
            std::string evaluatedValue;
            if (!param_part.empty()) {
                ph->evaluateWithParam(ctx, param_part, evaluatedValue);
            } else {
                ph->evaluate(ctx, evaluatedValue);
            }

            auto params = ParameterParser::parse(param_part);
            ParameterParser::formatNumericValue(evaluatedValue, params.precision);
            ParameterParser::applyColorRules(evaluatedValue, params.colorParamPart);
            result.append(evaluatedValue);
        } else {
            // If placeholder is not found, append the original text
            result.append(match[0].str());
        }
    }

    result.append(last_match_end, text.end());
    return result;
}

std::string PlaceholderProcessor::processServer(std::string_view text, const PlaceholderRegistry& registry) {
    return process(text, nullptr, registry);
}

} // namespace PA
