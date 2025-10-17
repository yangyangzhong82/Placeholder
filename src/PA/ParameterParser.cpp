// src/PA/ParameterParser.cpp
#include "PA/ParameterParser.h"
#include "PA/logger.h" // 引入 logger 头文件
#include <algorithm>   // For std::transform and ::tolower
#include <cctype>      // For ::tolower
#include <charconv>
#include <iomanip>
#include <regex> // 引入正则表达式头文件
#include <sstream>
#include <vector>


namespace PA {

namespace ParameterParser {

PlaceholderParams parse(std::string_view paramPart) {
    PlaceholderParams params;
    if (paramPart.empty()) {
        return params;
    }

    std::string              currentParam(paramPart);
    std::vector<std::string> paramSegments;
    size_t                   start = 0;
    size_t                   end   = currentParam.find(',');
    while (end != std::string::npos) {
        paramSegments.push_back(currentParam.substr(start, end - start));
        start = end + 1;
        end   = currentParam.find(',', start);
    }
    paramSegments.push_back(currentParam.substr(start));

    std::vector<std::string> remainingParams;
    for (const auto& p : paramSegments) {
        if (p.rfind("precision=", 0) == 0) {
            std::string_view precision_sv = std::string_view(p).substr(10); // "precision=".length()
            int              parsedPrecision;
            auto [prec_ptr, prec_ec] =
                std::from_chars(precision_sv.data(), precision_sv.data() + precision_sv.size(), parsedPrecision);
            if (prec_ec == std::errc()) {
                params.precision = parsedPrecision;
            }
        } else if (p.rfind("map=", 0) == 0) {
            params.conditional.enabled = true;
            std::string_view rules_sv  = std::string_view(p).substr(4); // "map=".length()

            auto parse_condition = [&](std::string_view rule) {
                size_t colon_pos = rule.find(':');
                if (colon_pos == std::string_view::npos) return false;

                size_t op_len = 0;
                if (rule.rfind(">=", 0) == 0 || rule.rfind("<=", 0) == 0 || rule.rfind("!=", 0) == 0) {
                    op_len = 2;
                } else if (rule.rfind(">", 0) == 0 || rule.rfind("<", 0) == 0 || rule.rfind("=", 0) == 0) {
                    op_len = 1;
                } else {
                    return false; // No valid operator at the start
                }
                std::string_view op_str = rule.substr(0, op_len);

                std::string_view val_str = rule.substr(op_len, colon_pos - op_len);

                Condition c;
                if (op_str == ">") c.op = Condition::Operator::GT;
                else if (op_str == "<") c.op = Condition::Operator::LT;
                else if (op_str == "=") c.op = Condition::Operator::EQ;
                else if (op_str == ">=") c.op = Condition::Operator::GTE;
                else if (op_str == "<=") c.op = Condition::Operator::LTE;
                else if (op_str == "!=") c.op = Condition::Operator::NEQ;
                else return false; // Should be unreachable

                auto [ptr, ec] = std::from_chars(val_str.data(), val_str.data() + val_str.size(), c.threshold);
                if (ec != std::errc()) return false;

                c.output = rule.substr(colon_pos + 1);
                params.conditional.conditions.push_back(c);
                return true;
            };

            size_t start = 0;
            size_t end   = 0;
            while ((end = rules_sv.find(';', start)) != std::string_view::npos) {
                std::string_view rule = rules_sv.substr(start, end - start);
                if (!rule.empty()) {
                    parse_condition(rule);
                }
                start = end + 1;
            }

            std::string_view last_part = rules_sv.substr(start);
            if (!last_part.empty()) {
                if (!parse_condition(last_part)) {
                    params.conditional.hasElse    = true;
                    params.conditional.elseOutput = last_part;
                }
            } else if (start > 0 && rules_sv[start - 1] == ';') {
                params.conditional.hasElse    = true;
                params.conditional.elseOutput = "";
            }
        } else if (p.rfind("bool_map=", 0) == 0) {
            params.booleanMap.enabled = true;
            std::string_view rules_sv = std::string_view(p).substr(9); // "bool_map=".length()

            size_t start = 0;
            size_t end   = 0;
            while ((end = rules_sv.find(';', start)) != std::string_view::npos) {
                std::string_view rule      = rules_sv.substr(start, end - start);
                size_t           colon_pos = rule.find(':');
                if (colon_pos != std::string_view::npos) {
                    params.booleanMap.mappings[std::string(rule.substr(0, colon_pos))] =
                        std::string(rule.substr(colon_pos + 1));
                }
                start = end + 1;
            }
            std::string_view last_part = rules_sv.substr(start);
            if (!last_part.empty()) {
                size_t colon_pos = last_part.find(':');
                if (colon_pos != std::string_view::npos) {
                    params.booleanMap.mappings[std::string(last_part.substr(0, colon_pos))] =
                        std::string(last_part.substr(colon_pos + 1));
                }
            }
        } else if (p.rfind("char_map=", 0) == 0) {
            params.charReplaceMap.enabled = true;
            std::string_view rules_sv     = std::string_view(p).substr(9); // "char_map=".length()

            size_t start = 0;
            size_t end   = 0;
            while ((end = rules_sv.find(';', start)) != std::string_view::npos) {
                std::string_view rule      = rules_sv.substr(start, end - start);
                size_t           colon_pos = rule.find(':');
                if (colon_pos != std::string_view::npos) {
                    params.charReplaceMap.mappings[std::string(rule.substr(0, colon_pos))] =
                        std::string(rule.substr(colon_pos + 1));
                }
                start = end + 1;
            }
            std::string_view last_part = rules_sv.substr(start);
            if (!last_part.empty()) {
                size_t colon_pos = last_part.find(':');
                if (colon_pos != std::string_view::npos) {
                    params.charReplaceMap.mappings[std::string(last_part.substr(0, colon_pos))] =
                        std::string(last_part.substr(colon_pos + 1));
                }
            }
        } else if (p.rfind("json_map=", 0) == 0) {
            params.jsonMap.enabled    = true;
            std::string_view json_sv = std::string_view(p).substr(9); // "json_map=".length()
            try {
                params.jsonMap.mappings = nlohmann::json::parse(json_sv);
            } catch (const nlohmann::json::exception& e) {
                logger.error("Failed to parse json_map: {}", e.what());
                params.jsonMap.enabled = false; // Disable if parsing fails
            }
        } else if (p.rfind("regex_map=", 0) == 0) {
            params.regexReplaceMap.enabled = true;
            std::string_view rules_sv      = std::string_view(p).substr(10); // "regex_map=".length()

            auto add_mapping = [&](std::string_view rule) {
                size_t colon_pos = rule.find(':');
                if (colon_pos != std::string_view::npos) {
                    std::string regex_str = std::string(rule.substr(0, colon_pos));
                    std::string replacement = std::string(rule.substr(colon_pos + 1));
                    try {
                        params.regexReplaceMap.mappings.emplace_back(
                            std::regex(regex_str, std::regex_constants::optimize),
                            replacement
                        );
                    } catch (const std::regex_error& e) {
                        logger.error("Invalid regex pattern '{}': {}", regex_str, e.what());
                    }
                }
            };

            size_t start = 0;
            size_t end   = 0;
            while ((end = rules_sv.find(';', start)) != std::string_view::npos) {
                add_mapping(rules_sv.substr(start, end - start));
                start = end + 1;
            }
            add_mapping(rules_sv.substr(start));
        } else if (p.find('=') != std::string::npos) {
            size_t separatorPos                           = p.find('=');
            params.otherParams[p.substr(0, separatorPos)] = p.substr(separatorPos + 1);
        } else {
            remainingParams.push_back(p);
        }
    }

    if (!remainingParams.empty()) {
        std::stringstream ss;
        for (size_t i = 0; i < remainingParams.size(); ++i) {
            ss << remainingParams[i];
            if (i < remainingParams.size() - 1) {
                ss << ",";
            }
        }
        params.colorParamPart = ss.str();
    }

    return params;
}

void formatNumericValue(std::string& evaluatedValue, int precision) {
    if (precision == -1) {
        return;
    }

    double value;
    auto [ptr, ec] = std::from_chars(evaluatedValue.data(), evaluatedValue.data() + evaluatedValue.size(), value);
    if (ec == std::errc()) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(precision) << value;
        evaluatedValue = ss.str();
    }
}

void applyColorRules(std::string& evaluatedValue, const std::string& colorParamPart, std::string_view colorFormat) {
    if (colorParamPart.empty()) {
        return;
    }

    auto applyFormat = [&](const std::string& color) {
        std::string formatted = std::string(colorFormat);
        size_t      pos;
        while ((pos = formatted.find("{color}")) != std::string::npos) {
            formatted.replace(pos, 7, color);
        }
        while ((pos = formatted.find("{value}")) != std::string::npos) {
            formatted.replace(pos, 7, evaluatedValue);
        }
        evaluatedValue = formatted;
    };

    std::string              currentParam(colorParamPart);
    std::vector<std::string> params;
    size_t                   start = 0;
    size_t                   end   = currentParam.find(',');
    while (end != std::string::npos) {
        params.push_back(currentParam.substr(start, end - start));
        start = end + 1;
        end   = currentParam.find(',', start);
    }
    params.push_back(currentParam.substr(start));

    if (params.size() == 1) {
        // If there is only one parameter, treat it as a color code
        applyFormat(params[0]);
        return;
    }

    double value;
    auto [ptr, ec] = std::from_chars(evaluatedValue.data(), evaluatedValue.data() + evaluatedValue.size(), value);
    if (ec != std::errc()) {
        // Not a numeric value, cannot apply threshold-based coloring
        return;
    }

    if (params.size() >= 3 && params.size() % 2 == 1) {
        // Format: {value,color_low,value,color_mid,default_color}
        std::string appliedColor = params.back(); // Default color
        for (size_t i = 0; i < params.size() - 1; i += 2) {
            double           threshold;
            std::string_view threshold_sv = params[i];
            auto [t_ptr, t_ec] =
                std::from_chars(threshold_sv.data(), threshold_sv.data() + threshold_sv.size(), threshold);
            if (t_ec == std::errc()) {
                if (value < threshold) {
                    appliedColor = params[i + 1];
                    break;
                }
            }
        }
        applyFormat(appliedColor);
    }
}

// 辅助函数：修剪字符串两端的空白字符
static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r\f\v");
    if (std::string::npos == first) {
        return str;
    }
    size_t last = str.find_last_not_of(" \t\n\r\f\v");
    return str.substr(first, (last - first + 1));
}

void applyConditionalOutput(std::string& evaluatedValue, const ConditionalOutput& conditional) {
    if (!conditional.enabled) {
        return;
    }

    double value;
    auto [ptr, ec] = std::from_chars(evaluatedValue.data(), evaluatedValue.data() + evaluatedValue.size(), value);
    if (ec != std::errc()) {
        // Not a numeric value, cannot apply conditional output
        return;
    }

    std::string originalValue = evaluatedValue;
    std::string output;
    bool        matched = false;

    for (const auto& condition : conditional.conditions) {
        bool result = false;
        switch (condition.op) {
        case Condition::Operator::GT:
            result = value > condition.threshold;
            break;
        case Condition::Operator::LT:
            result = value < condition.threshold;
            break;
        case Condition::Operator::EQ:
            result = value == condition.threshold;
            break;
        case Condition::Operator::GTE:
            result = value >= condition.threshold;
            break;
        case Condition::Operator::LTE:
            result = value <= condition.threshold;
            break;
        case Condition::Operator::NEQ:
            result = value != condition.threshold;
            break;
        }
        if (result) {
            output  = condition.output;
            matched = true;
            break;
        }
    }

    if (!matched && conditional.hasElse) {
        output  = conditional.elseOutput;
        matched = true;
    }

    if (matched) {
        size_t pos = output.find("{value}");
        if (pos != std::string::npos) {
            output.replace(pos, 7, originalValue);
            evaluatedValue = output;
        } else {
            evaluatedValue = output + originalValue;
        }
    }
}

void applyBooleanMap(std::string& evaluatedValue, const BooleanMap& booleanMap) {
    if (!booleanMap.enabled) {
        return;
    }

    std::string trimmedValue = trim(evaluatedValue); // 修剪 evaluatedValue
    auto        it           = booleanMap.mappings.find(trimmedValue);
    if (it != booleanMap.mappings.end()) {
        evaluatedValue = it->second;
    }
}

void applyCharReplaceMap(std::string& evaluatedValue, const CharReplaceMap& charReplaceMap) {
    if (!charReplaceMap.enabled) {
        return;
    }

    for (const auto& pair : charReplaceMap.mappings) {
        const std::string& from = pair.first;
        const std::string& to   = pair.second;

        size_t pos = evaluatedValue.find(from, 0);
        while (pos != std::string::npos) {
            evaluatedValue.replace(pos, from.length(), to);
            pos = evaluatedValue.find(from, pos + to.length());
        }
    }
}

// 辅助函数：尝试解析大小写转换指令，例如 "\l$1" 或 "\u$1"
static bool
tryParseCaseDirective(const std::string& replacement, char directive_char, bool& is_case_flag, int& case_group_num) {
    // We are looking for replacement strings like "\l$1" or "\u$1"
    // which means the actual string content is '\', 'l', '$', '1'
    if (replacement.size() < 4 || replacement[0] != '\\' || replacement[1] != directive_char || replacement[2] != '$') {
        return false;
    }
    // We expect something like \l$1, \u$12 etc.
    std::string_view group_sv(replacement);
    group_sv.remove_prefix(3); // Remove "\l$" or "\u$"

    if (group_sv.empty() || !std::isdigit(static_cast<unsigned char>(group_sv[0]))) {
        logger.debug("    Case directive parse failed: no group number after '$' in '{}'", replacement);
        return false; // No number after '$'
    }

    size_t parsed_count = 0;
    for (char c : group_sv) {
        if (std::isdigit(static_cast<unsigned char>(c))) {
            parsed_count++;
        } else {
            break; // Stop at the first non-digit
        }
    }

    // The directive must cover the whole replacement string
    if (parsed_count != group_sv.length()) {
        logger.debug("    Case directive parse failed: extra chars after group number in '{}'", replacement);
        return false;
    }

    try {
        case_group_num = std::stoi(std::string(group_sv));
        is_case_flag   = true;
        logger.debug(
            std::string("    Parsed case directive \\") + directive_char + "$" + std::to_string(case_group_num)
            + ", group number is " + std::to_string(case_group_num)
        );
        return true;
    } catch (...) {
        logger.warn("    Failed to parse group number from '{}'", std::string(group_sv));
        return false;
    }
}

void applyRegexReplaceMap(std::string& evaluatedValue, const RegexReplaceMap& regexReplaceMap) {
    if (!regexReplaceMap.enabled) {
        return;
    }

    logger.debug("applyRegexReplaceMap: Initial evaluatedValue='{}'", evaluatedValue);

    for (const auto& pair : regexReplaceMap.mappings) {
        const std::regex&  re          = pair.first;
        const std::string& replacement = pair.second;
        logger.debug("  Applying regex, raw replacement='{}'", replacement);

        std::string result_value;
        auto        last_match_end = evaluatedValue.cbegin();

        bool is_lowercase_replacement = false;
        bool is_uppercase_replacement = false;
        int  case_group_num           = -1;

        if (!tryParseCaseDirective(replacement, 'l', is_lowercase_replacement, case_group_num)) {
            tryParseCaseDirective(replacement, 'u', is_uppercase_replacement, case_group_num);
        }

        for (std::sregex_iterator it(evaluatedValue.cbegin(), evaluatedValue.cend(), re), end; it != end; ++it) {
            result_value.append(last_match_end, it->prefix().second);

            if ((is_lowercase_replacement || is_uppercase_replacement) && case_group_num >= 0
                && case_group_num < static_cast<int>(it->size())) {

                std::string captured = (*it)[case_group_num].str();
                if (is_lowercase_replacement) {
                    std::transform(captured.begin(), captured.end(), captured.begin(), [](unsigned char c) {
                        return static_cast<char>(std::tolower(c));
                    });
                } else { // is_uppercase_replacement
                    std::transform(captured.begin(), captured.end(), captured.begin(), [](unsigned char c) {
                        return static_cast<char>(std::toupper(c));
                    });
                }
                result_value.append(captured);

            } else {
                // 通用 $N 处理：逆序替换，避免 $1 替换 $10 中的 $1 部分
                std::string formatted_replacement = replacement;
                for (int i = static_cast<int>(it->size()) - 1; i >= 0; --i) {
                    std::string group_placeholder = "$" + std::to_string(i);
                    size_t      pos               = formatted_replacement.find(group_placeholder);
                    while (pos != std::string::npos) {
                        formatted_replacement.replace(pos, group_placeholder.length(), (*it)[i].str());
                        // 替换后，从新的位置继续查找，避免重复替换
                        pos = formatted_replacement.find(group_placeholder, pos + (*it)[i].str().length());
                    }
                }
                result_value.append(formatted_replacement);
            }

            last_match_end = it->suffix().first;
        }
        result_value.append(last_match_end, evaluatedValue.cend());
        evaluatedValue = result_value;
        logger.debug("  After applying regex, evaluatedValue='{}'", evaluatedValue);
    }
    logger.debug("applyRegexReplaceMap: Final evaluatedValue='{}'", evaluatedValue);
}

void applyJsonMap(std::string& evaluatedValue, const JsonMap& jsonMap) {
    if (!jsonMap.enabled) {
        return;
    }

    if (!jsonMap.mappings.is_object()) {
        // The JSON map must be an object (key-value pairs).
        return;
    }

    auto it = jsonMap.mappings.find(evaluatedValue);
    if (it != jsonMap.mappings.end()) {
        const auto& mappedValue = *it;
        if (mappedValue.is_string()) {
            evaluatedValue = mappedValue.get<std::string>();
        } else {
            // For other types (number, boolean, array, object), dump() provides a string representation.
            evaluatedValue = mappedValue.dump();
        }
    }
}

} // namespace ParameterParser
} // namespace PA
