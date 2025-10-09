// src/PA/PlaceholderProcessor.cpp
#include "PA/PlaceholderProcessor.h"
#include "PA/PlaceholderRegistry.h"
#include "PA/ParameterParser.h"
#include <vector>

namespace PA {

std::string PlaceholderProcessor::process(std::string_view text, const IContext* ctx, const PlaceholderRegistry& registry) {
    std::string result;
    result.reserve(text.length());
    size_t pos = 0;

    while (pos < text.length()) {
        size_t start_pos = text.find_first_of("%{", pos);

        if (start_pos == std::string_view::npos) {
            result.append(text.substr(pos));
            break;
        }

        result.append(text.substr(pos, start_pos - pos));

        char open_delim = text[start_pos];
        char close_delim = (open_delim == '{') ? '}' : '%';

        size_t end_pos = text.find(close_delim, start_pos + 1);

        if (end_pos == std::string_view::npos) {
            result.push_back(open_delim);
            pos = start_pos + 1;
            continue;
        }

        std::string_view full_placeholder = text.substr(start_pos, end_pos - start_pos + 1);
        std::string_view content_sv = text.substr(start_pos + 1, end_pos - start_pos - 1);
        std::string content(content_sv);

        std::string token;
        std::string param_part;

        size_t last_colon = content.rfind(':');
        if (last_colon != std::string::npos) {
            std::string after_colon = content.substr(last_colon + 1);
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

            ParameterParser::applyConditionalOutput(evaluatedValue, params.conditional);

            std::string_view colorFormat = "{color}{value}";
            auto colorFormatIt = params.otherParams.find("color_format");
            if (colorFormatIt != params.otherParams.end()) {
                colorFormat = colorFormatIt->second;
            }

            ParameterParser::applyColorRules(evaluatedValue, params.colorParamPart, colorFormat);
            result.append(evaluatedValue);
        } else {
            result.append(full_placeholder);
        }
        pos = end_pos + 1;
    }

    return result;
}

std::string PlaceholderProcessor::processServer(std::string_view text, const PlaceholderRegistry& registry) {
    return process(text, nullptr, registry);
}

} // namespace PA
