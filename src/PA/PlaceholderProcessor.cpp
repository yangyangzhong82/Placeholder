// src/PA/PlaceholderProcessor.cpp
#include "PA/PlaceholderProcessor.h"
#include "PA/ParameterParser.h"
#include "PA/PlaceholderRegistry.h"
#include "PA/logger.h"
#include <sstream>
#include <vector>


namespace PA {

std::string
PlaceholderProcessor::process(std::string_view text, const IContext* ctx, const PlaceholderRegistry& registry) {
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

        char open_delim  = text[start_pos];
        char close_delim = (open_delim == '{') ? '}' : '%';

        size_t end_pos = text.find(close_delim, start_pos + 1);

        if (end_pos == std::string_view::npos) {
            result.push_back(open_delim);
            pos = start_pos + 1;
            continue;
        }

        std::string_view full_placeholder = text.substr(start_pos, end_pos - start_pos + 1);
        std::string_view content_sv       = text.substr(start_pos + 1, end_pos - start_pos - 1);
        std::string      content(content_sv);

        std::string token;
        std::string param_part;
        std::shared_ptr<const IPlaceholder> ph;

        // Find the longest registered placeholder that is a prefix of `content`.
        for (size_t split_pos = content.length();;) {
            std::string potential_token = content.substr(0, split_pos);
            ph = registry.findPlaceholder(potential_token, ctx);

            if (ph) {
                token = potential_token;
                if (split_pos < content.length()) {
                    if (content[split_pos] == ':') {
                        param_part = content.substr(split_pos + 1);
                    } else {
                        // The character after the potential token is not a parameter separator,
                        // so this is not a valid match.
                        ph.reset();
                        token.clear();
                    }
                }
                if (ph) {
                    break; // Found the longest valid match
                }
            }

            if (split_pos == 0) {
                break; // Reached the beginning of the string
            }
            size_t prev_colon = content.rfind(':', split_pos - 1);
            if (prev_colon == std::string::npos) {
                break; // No more colons to check for shorter prefixes
            }
            split_pos = prev_colon;
        }

        logger.debug("1. Initial Parse: token='{}', param_part='{}'", token, param_part);
        if (ph) {
            std::string evaluatedValue;
            std::string placeholder_param_part;
            std::string formatting_param_part;

            if (!param_part.empty()) {
                std::string              currentParam(param_part);
                std::vector<std::string> paramSegments;
                size_t                   start = 0;
                size_t                   end   = currentParam.find(',');
                while (end != std::string::npos) {
                    paramSegments.push_back(currentParam.substr(start, end - start));
                    start = end + 1;
                    end   = currentParam.find(',', start);
                }
                paramSegments.push_back(currentParam.substr(start));

                std::stringstream p_param_ss;
                std::stringstream f_param_ss;
                bool              first_p = true;
                bool              first_f = true;

                for (const auto& p : paramSegments) {
                    if (p.rfind("precision=", 0) == 0 || p.rfind("map=", 0) == 0 || p.rfind("color_format=", 0) == 0 || p.rfind("bool_map=", 0) == 0) {
                        if (!first_f) f_param_ss << ",";
                        f_param_ss << p;
                        first_f = false;
                    } else {
                        if (!first_p) p_param_ss << ",";
                        p_param_ss << p;
                        first_p = false;
                    }
                }
                placeholder_param_part = p_param_ss.str();
                formatting_param_part  = f_param_ss.str();
            }
            logger.debug(
                "2. Separated Params: placeholder_param='{}', formatting_param='{}'",
                placeholder_param_part,
                formatting_param_part
            );

            if (!placeholder_param_part.empty()) {
                ph->evaluateWithParam(ctx, placeholder_param_part, evaluatedValue);
            } else {
                ph->evaluate(ctx, evaluatedValue);
            }
            logger.debug("3. After Evaluate: evaluatedValue='{}'", evaluatedValue);

            if (!formatting_param_part.empty()) {
                auto params = ParameterParser::parse(formatting_param_part);
                ParameterParser::formatNumericValue(evaluatedValue, params.precision);
                logger.debug("4. After formatNumericValue: evaluatedValue='{}'", evaluatedValue);
                ParameterParser::applyConditionalOutput(evaluatedValue, params.conditional);
                logger.debug("5. After applyConditionalOutput: evaluatedValue='{}'", evaluatedValue);
                ParameterParser::applyBooleanMap(evaluatedValue, params.booleanMap); // 应用布尔值映射
                logger.debug("5.5. After applyBooleanMap: evaluatedValue='{}'", evaluatedValue);

                std::string_view colorFormat   = "{color}{value}";
                auto             colorFormatIt = params.otherParams.find("color_format");
                if (colorFormatIt != params.otherParams.end()) {
                    colorFormat = colorFormatIt->second;
                }
                // If map= is used, it takes precedence over color rules.
                if (!params.conditional.enabled) {
                    ParameterParser::applyColorRules(evaluatedValue, params.colorParamPart, colorFormat);
                }
                logger.debug("6. After applyColorRules: evaluatedValue='{}'", evaluatedValue);
            }

            logger.debug("7. Final Value: evaluatedValue='{}'", evaluatedValue);
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
