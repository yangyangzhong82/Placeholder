// src/PA/ParameterParser.h
#pragma once

#include <string>
#include <string_view>
#include <map>
#include <vector>
#include <regex>
#include <nlohmann/json.hpp>
#include "PA/PlaceholderAPI.h"

namespace PA {

namespace ParameterParser {

// 表示单个条件规则
struct Condition {
    enum class Operator { GT, LT, EQ, GTE, LTE, NEQ } op;
    double      threshold;
    std::string output;
    double      epsilon = 1e-9; // 浮点比较的容差
};

// 表示完整的条件输出规则
struct ConditionalOutput {
    bool                   enabled = false;
    bool                   hasElse = false;
    std::vector<Condition> conditions;
    std::string            elseOutput;
    double                 default_epsilon = 1e-9; // 默认浮点比较容差
};

// 表示布尔值映射规则
struct BooleanMap {
    bool                   enabled = false;
    std::map<std::string, std::string> mappings;
};

// 表示字符替换映射规则
struct CharReplaceMap {
    bool                   enabled = false;
    std::map<std::string, std::string> mappings;
};

// 表示正则表达式替换映射规则
struct RegexReplaceMap {
    bool                                           enabled = false;
    std::vector<std::pair<std::regex, std::string>> mappings;
};

// 表示JSON映射规则
struct JsonMap {
    bool           enabled = false;
    nlohmann::json mappings;
};

// 表示从占位符解析的参数
struct PlaceholderParams {
    int                                precision = -1;
    std::string                        colorParamPart;
    std::map<std::string, std::string> otherParams;
    ConditionalOutput                  conditional;
    BooleanMap                         booleanMap;
    CharReplaceMap                     charReplaceMap;
    RegexReplaceMap                    regexReplaceMap; // 正则表达式替换映射参数
    JsonMap                            jsonMap;
};

// 辅助函数：根据逗号分割参数字符串，同时处理引号、转义和括号/花括号嵌套
std::vector<std::string> splitParamString(std::string_view paramPart, char delimiter);

// 解析占位符的参数部分
PlaceholderParams parse(std::string_view paramPart);

// 根据给定精度格式化数值字符串
void formatNumericValue(std::string& evaluatedValue, int precision);

// 将颜色规则应用于评估值
void applyColorRules(std::string& evaluatedValue, const std::string& colorParamPart, std::string_view colorFormat);

// 将条件输出规则应用于评估值
void applyConditionalOutput(std::string& evaluatedValue, const ConditionalOutput& conditional);

// 将布尔值映射规则应用于评估值
void applyBooleanMap(std::string& evaluatedValue, const BooleanMap& booleanMap);

// 将字符替换映射规则应用于评估值
void applyCharReplaceMap(std::string& evaluatedValue, const CharReplaceMap& charReplaceMap);

// 将正则表达式替换映射规则应用于评估值
void applyRegexReplaceMap(std::string& evaluatedValue, const RegexReplaceMap& regexReplaceMap);

// 将JSON映射规则应用于评估值
void applyJsonMap(std::string& evaluatedValue, const JsonMap& jsonMap);

} // namespace ParameterParser
} // namespace PA
