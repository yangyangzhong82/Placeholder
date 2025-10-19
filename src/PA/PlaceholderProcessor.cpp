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

        size_t end_pos       = std::string_view::npos;
        int    nesting_level = 1;
        size_t scan_pos      = start_pos + 1;

        while (scan_pos < text.length()) {
            char current_char = text[scan_pos];
            if (current_char == '\\' && scan_pos + 1 < text.length()) {
                // Skip escaped character
                scan_pos++;
            } else if (current_char == open_delim) {
                nesting_level++;
            } else if (current_char == close_delim) {
                nesting_level--;
                if (nesting_level == 0) {
                    end_pos = scan_pos;
                    break;
                }
            }
            scan_pos++;
        }

        if (end_pos == std::string_view::npos) {
            result.push_back(open_delim);
            pos = start_pos + 1;
            continue;
        }

        std::string_view full_placeholder = text.substr(start_pos, end_pos - start_pos + 1);
        std::string_view content_sv       = text.substr(start_pos + 1, end_pos - start_pos - 1);
        std::string      content(content_sv);

        std::string                                  token;
        std::string                                  param_part;
        std::shared_ptr<const IPlaceholder>          ph;
        const ::PA::CachedEntry*                     cachedEntry = nullptr; // 指向缓存条目，如果找到的话

        // Find the longest registered placeholder that is a prefix of `content`.
        for (size_t split_pos = content.length();;) {
            std::string potential_token = content.substr(0, split_pos);
            auto [found_ph, found_cached_entry] = registry.findPlaceholder(potential_token, ctx);

            if (found_ph) {
                ph          = found_ph;
                cachedEntry = found_cached_entry;
                token       = potential_token;
                if (split_pos < content.length()) {
                    if (content[split_pos] == ':') {
                        param_part = content.substr(split_pos + 1);
                    } else {
                        // The character after the potential token is not a parameter separator,
                        // so this is not a valid match.
                        ph.reset();
                        token.clear();
                        cachedEntry = nullptr;
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

            bool useCachedValue = false;
            if (cachedEntry) {
                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::seconds>(now - cachedEntry->lastEvaluated).count() < cachedEntry->cacheDuration) {
                    // 缓存命中且未过期
                    evaluatedValue = cachedEntry->cachedValue;
                    useCachedValue = true;
                    logger.debug("3. Cache Hit: evaluatedValue='{}'", evaluatedValue);
                }
            }

            if (!useCachedValue) {
                if (!param_part.empty()) {
                    size_t pipe_pos = param_part.find('|');
                    if (pipe_pos != std::string::npos) {
                        // Split by '|': left for placeholder, right for formatting
                        placeholder_param_part = param_part.substr(0, pipe_pos);
                        formatting_param_part  = param_part.substr(pipe_pos + 1);
                    } else {
                        // No '|' found, use new logic to separate by key= using splitParamString
                        std::vector<std::string> paramSegments = ParameterParser::splitParamString(param_part, ',');

                        std::stringstream p_param_ss;
                        std::stringstream f_param_ss;
                        bool              first_p = true;
                        bool              first_f = true;

                        for (const auto& p : paramSegments) {
                            if (p.rfind("precision=", 0) == 0 || p.rfind("map=", 0) == 0
                                || p.rfind("color_format=", 0) == 0 || p.rfind("bool_map=", 0) == 0
                                || p.rfind("char_map=", 0) == 0 || p.rfind("regex_map=", 0) == 0
                                || p.rfind("json_map=", 0) == 0) {
                                if (!first_f) f_param_ss << ",";
                                f_param_ss << p;
                                first_f = false;
                            } else {
                                // If it doesn't have a key=, it's considered a placeholder param (or color threshold)
                                // For now, we put it in placeholder_param_part.
                                // If it doesn't have a key=, it's considered a placeholder param (positional argument)
                                if (!first_p) p_param_ss << ",";
                                p_param_ss << p;
                                first_p = false;
                            }
                        }
                        placeholder_param_part = p_param_ss.str();
                        formatting_param_part  = f_param_ss.str();
                    }
                }
                logger.debug(
                    "2. Separated Params: placeholder_param='{}', formatting_param='{}'",
                    placeholder_param_part,
                    formatting_param_part
                );

                if (!placeholder_param_part.empty()) {
                    std::vector<std::string_view> args;
                    // Use splitParamString for placeholder_param_part as well, as it might contain commas
                    std::vector<std::string> placeholderArgs = ParameterParser::splitParamString(placeholder_param_part, ',');
                    for (const auto& arg_str : placeholderArgs) {
                        args.push_back(arg_str);
                    }
                    ph->evaluateWithArgs(ctx, args, evaluatedValue);
                } else {
                    ph->evaluate(ctx, evaluatedValue);
                }
                logger.debug("3. After Evaluate: evaluatedValue='{}'", evaluatedValue);

                // 更新缓存
                if (cachedEntry) {
                    cachedEntry->cachedValue   = evaluatedValue;
                    cachedEntry->lastEvaluated = std::chrono::steady_clock::now();
                    logger.debug("3.5. Cache Updated: evaluatedValue='{}'", evaluatedValue);
                }
            }

            if (!formatting_param_part.empty()) {
                auto params = ParameterParser::parse(formatting_param_part);
                ParameterParser::applyConditionalOutput(evaluatedValue, params.conditional);
                logger.debug("4. After applyConditionalOutput: evaluatedValue='{}'", evaluatedValue);
                ParameterParser::formatNumericValue(evaluatedValue, params.precision);
                logger.debug("5. After formatNumericValue: evaluatedValue='{}'", evaluatedValue);
                ParameterParser::applyBooleanMap(evaluatedValue, params.booleanMap); // 应用布尔值映射
                logger.debug("5.5. After applyBooleanMap: evaluatedValue='{}'", evaluatedValue);
                ParameterParser::applyCharReplaceMap(evaluatedValue, params.charReplaceMap); // 应用字符替换映射
                logger.debug("5.6. After applyCharReplaceMap: evaluatedValue='{}'", evaluatedValue);
                ParameterParser::applyRegexReplaceMap(evaluatedValue, params.regexReplaceMap); // 应用正则表达式替换映射
                logger.debug("5.7. After applyRegexReplaceMap: evaluatedValue='{}'", evaluatedValue);
                ParameterParser::applyJsonMap(evaluatedValue, params.jsonMap); // 应用JSON映射
                logger.debug("5.8. After applyJsonMap: evaluatedValue='{}'", evaluatedValue);

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
