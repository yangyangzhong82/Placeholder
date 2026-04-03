// src/PA/ParameterApply.cpp
#include "PA/ParameterParser.h"
#include "PA/logger.h"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <iomanip>
#include <regex>
#include <sstream>

namespace PA::ParameterParser {

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
        applyFormat(params[0]);
        return;
    }

    double value;
    auto [ptr, ec] = std::from_chars(evaluatedValue.data(), evaluatedValue.data() + evaluatedValue.size(), value);
    if (ec != std::errc()) {
        return;
    }

    if (params.size() >= 3 && params.size() % 2 == 1) {
        std::string appliedColor = params.back();
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
            result = std::fabs(value - condition.threshold) <= condition.epsilon;
            break;
        case Condition::Operator::GTE:
            result = value >= condition.threshold;
            break;
        case Condition::Operator::LTE:
            result = value <= condition.threshold;
            break;
        case Condition::Operator::NEQ:
            result = std::fabs(value - condition.threshold) > condition.epsilon;
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

    std::string trimmedValue = trim(evaluatedValue);
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
    if (replacement.size() < 4 || replacement[0] != '\\' || replacement[1] != directive_char || replacement[2] != '$') {
        return false;
    }
    std::string_view group_sv(replacement);
    group_sv.remove_prefix(3);

    if (group_sv.empty() || !std::isdigit(static_cast<unsigned char>(group_sv[0]))) {
        logger.debug("    Case directive parse failed: no group number after '$' in '{}'", replacement);
        return false;
    }

    size_t parsed_count = 0;
    for (char c : group_sv) {
        if (std::isdigit(static_cast<unsigned char>(c))) {
            parsed_count++;
        } else {
            break;
        }
    }

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
                } else {
                    std::transform(captured.begin(), captured.end(), captured.begin(), [](unsigned char c) {
                        return static_cast<char>(std::toupper(c));
                    });
                }
                result_value.append(captured);

            } else {
                std::string formatted_replacement = replacement;
                for (int i = static_cast<int>(it->size()) - 1; i >= 0; --i) {
                    std::string group_placeholder = "$" + std::to_string(i);
                    size_t      pos               = formatted_replacement.find(group_placeholder);
                    while (pos != std::string::npos) {
                        formatted_replacement.replace(pos, group_placeholder.length(), (*it)[i].str());
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
        return;
    }

    auto it = jsonMap.mappings.find(evaluatedValue);
    if (it != jsonMap.mappings.end()) {
        const auto& mappedValue = *it;
        if (mappedValue.is_string()) {
            evaluatedValue = mappedValue.get<std::string>();
        } else {
            evaluatedValue = mappedValue.dump();
        }
    }
}

} // namespace PA::ParameterParser
