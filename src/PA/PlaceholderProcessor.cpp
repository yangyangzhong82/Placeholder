// src/PA/PlaceholderProcessor.cpp
#include "PA/PlaceholderProcessor.h"
#include "PA/PlaceholderRegistry.h"
#include "PA/ParameterParser.h"
#include <vector>
#include <regex>

namespace PA {

// Regex to find placeholders like %placeholder% or {placeholder}
// It captures the full content between delimiters.
static const std::regex placeholderRegex(R"((%|\{)([^%\{\}]+)(%|\}))");

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
        std::string content     = match[2].str();
        std::string close_delim = match[3].str();

        // Basic validation: delimiters must match (%...% or {...})
        if ((open_delim == "%" && close_delim != "%") || (open_delim == "{" && close_delim != "}")) {
            result.append(match[0].str());
            continue;
        }

        std::string token;
        std::string param_part;

        // Heuristically split content into token and param_part
        size_t last_colon = content.rfind(':');
        if (last_colon != std::string::npos) {
            std::string after_colon = content.substr(last_colon + 1);
            // Simple heuristic: if the part after the last colon contains typical parameter characters,
            // treat it as a parameter. This covers "precision=2" and "100,red".
            if (after_colon.find('=') != std::string::npos || after_colon.find(',') != std::string::npos ||
                (!after_colon.empty() && isdigit(static_cast<unsigned char>(after_colon[0])))) {
                token      = content.substr(0, last_colon);
                param_part = after_colon;
            } else {
                token = content;
            }
        } else {
            token = content;
        }

        auto ph = registry.findPlaceholder(token, ctx);
        if (ph) {
            std::string evaluatedValue;
            if (!param_part.empty()) {
                ph->evaluateWithParam(ctx, param_part, evaluatedValue);
            } else {
                ph->evaluate(ctx, evaluatedValue);
            }

            auto params = ParameterParser::parse(param_part);
            ParameterParser::formatNumericValue(evaluatedValue, params.precision);

            // 从参数中提取颜色格式
            std::string_view colorFormat = "{color}{value}"; // 默认值
            auto colorFormatIt = params.otherParams.find("color_format");
            if (colorFormatIt != params.otherParams.end()) {
                colorFormat = colorFormatIt->second;
            }

            ParameterParser::applyColorRules(evaluatedValue, params.colorParamPart, colorFormat);
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
