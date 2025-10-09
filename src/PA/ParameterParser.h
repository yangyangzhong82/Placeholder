// src/PA/ParameterParser.h
#pragma once

#include <string>
#include <string_view>
#include <map>
#include "PA/PlaceholderAPI.h"

namespace PA {

namespace ParameterParser {

// 表示从占位符解析的参数
struct PlaceholderParams {
    int                                precision = -1;
    std::string                        colorParamPart;
    std::map<std::string, std::string> otherParams;
};

// 解析占位符的参数部分
PlaceholderParams parse(std::string_view paramPart);

// 根据给定精度格式化数值字符串
void formatNumericValue(std::string& evaluatedValue, int precision);

// 将颜色规则应用于评估值
void applyColorRules(std::string& evaluatedValue, const std::string& colorParamPart, std::string_view colorFormat);

} // namespace ParameterParser
} // namespace PA
