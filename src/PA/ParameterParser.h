// src/PA/ParameterParser.h
#pragma once

#include <string>
#include <string_view>
#include <map>
#include <vector>
#include "PA/PlaceholderAPI.h"

namespace PA {

namespace ParameterParser {

// 表示单个条件规则
struct Condition {
    enum class Operator { GT, LT, EQ, GTE, LTE, NEQ } op;
    double      threshold;
    std::string output;
};

// 表示完整的条件输出规则
struct ConditionalOutput {
    bool                   enabled = false;
    bool                   hasElse = false;
    std::vector<Condition> conditions;
    std::string            elseOutput;
};

// 表示布尔值映射规则
struct BooleanMap {
    bool                   enabled = false;
    std::map<std::string, std::string> mappings;
};

// 表示从占位符解析的参数
struct PlaceholderParams {
    int                                precision = -1;
    std::string                        colorParamPart;
    std::map<std::string, std::string> otherParams;
    ConditionalOutput                  conditional;
    BooleanMap                         booleanMap; // 新增的布尔值映射参数
};

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

} // namespace ParameterParser
} // namespace PA
