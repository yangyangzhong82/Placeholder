// src/PA/ParameterParser.cpp
#include "PA/ParameterParser.h"
#include "PA/logger.h" // 引入 logger 头文件
#include <charconv>
#include <sstream>
#include <vector>

namespace PA::ParameterParser {

// 辅助函数：根据逗号分割参数字符串，同时处理引号、转义和括号/花括号嵌套
std::vector<std::string> splitParamString(std::string_view paramPart, char delimiter) {
    std::vector<std::string> segments;
    if (paramPart.empty()) {
        return segments;
    }

    size_t       start_idx = 0;
    int          quote_level = 0; // 0: none, 1: single, 2: double
    int          paren_level = 0; // ()
    int          brace_level = 0; // {}
    bool         escaped     = false;

    for (size_t i = 0; i < paramPart.length(); ++i) {
        char c = paramPart[i];

        if (escaped) {
            escaped = false;
            continue;
        }

        if (c == '\\') {
            escaped = true;
            continue;
        }

        if (quote_level == 0) {
            if (c == '\'') {
                quote_level = 1;
            } else if (c == '"') {
                quote_level = 2;
            } else if (c == '(') {
                paren_level++;
            } else if (c == ')') {
                paren_level--;
            } else if (c == '{') {
                brace_level++;
            } else if (c == '}') {
                brace_level--;
            } else if (c == delimiter && paren_level == 0 && brace_level == 0) {
                segments.push_back(std::string(paramPart.substr(start_idx, i - start_idx)));
                start_idx = i + 1;
            }
        } else if (quote_level == 1 && c == '\'') {
            quote_level = 0;
        } else if (quote_level == 2 && c == '"') {
            quote_level = 0;
        }
    }

    segments.push_back(std::string(paramPart.substr(start_idx)));
    return segments;
}

PlaceholderParams parse(std::string_view paramPart) {
    PlaceholderParams params;
    if (paramPart.empty()) {
        return params;
    }

    std::vector<std::string> paramSegments = splitParamString(paramPart, ',');

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
                c.epsilon = params.conditional.default_epsilon; // 使用默认 epsilon
                params.conditional.conditions.push_back(c);
                return true;
            };

            std::vector<std::string> ruleSegments = splitParamString(rules_sv, ';');
            for (size_t i = 0; i < ruleSegments.size(); ++i) {
                std::string_view rule = ruleSegments[i];
                if (rule.empty()) {
                    if (i + 1 == ruleSegments.size()) {
                        params.conditional.hasElse    = true;
                        params.conditional.elseOutput = "";
                    }
                    continue;
                }

                bool parsed = parse_condition(rule);
                if (!parsed && i + 1 == ruleSegments.size()) {
                    params.conditional.hasElse    = true;
                    params.conditional.elseOutput = rule;
                }
            }
        } else if (p.rfind("eq_eps=", 0) == 0) {
            std::string_view epsilon_sv = std::string_view(p).substr(7); // "eq_eps=".length()
            double           parsedEpsilon;
            auto [eps_ptr, eps_ec] =
                std::from_chars(epsilon_sv.data(), epsilon_sv.data() + epsilon_sv.size(), parsedEpsilon);
            if (eps_ec == std::errc()) {
                params.conditional.default_epsilon = parsedEpsilon;
                for (auto& c : params.conditional.conditions) {
                    c.epsilon = parsedEpsilon;
                }
            }
        } else if (p.rfind("bool_map=", 0) == 0) {
            params.booleanMap.enabled = true;
            std::string_view rules_sv = std::string_view(p).substr(9); // "bool_map=".length()

            std::vector<std::string> ruleSegments = splitParamString(rules_sv, ';');
            for (const auto& rule : ruleSegments) {
                size_t colon_pos = rule.find(':');
                if (colon_pos != std::string_view::npos) {
                    params.booleanMap.mappings[std::string(rule.substr(0, colon_pos))] =
                        std::string(rule.substr(colon_pos + 1));
                }
            }
        } else if (p.rfind("char_map=", 0) == 0) {
            params.charReplaceMap.enabled = true;
            std::string_view rules_sv     = std::string_view(p).substr(9); // "char_map=".length()

            std::vector<std::string> ruleSegments = splitParamString(rules_sv, ';');
            for (const auto& rule : ruleSegments) {
                size_t colon_pos = rule.find(':');
                if (colon_pos != std::string_view::npos) {
                    params.charReplaceMap.mappings[std::string(rule.substr(0, colon_pos))] =
                        std::string(rule.substr(colon_pos + 1));
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

            std::vector<std::string> ruleSegments = splitParamString(rules_sv, ';');
            for (const auto& rule : ruleSegments) {
                add_mapping(rule);
            }
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

} // namespace PA::ParameterParser
