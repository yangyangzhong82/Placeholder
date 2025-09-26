#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace PA::Utils {

// 字符串处理
inline bool isSpace(unsigned char ch);
inline std::string ltrim(std::string s);
inline std::string rtrim(std::string s);
inline std::string trim(std::string s);
inline std::string toLower(std::string s);
inline bool iequals(std::string a, std::string b);

// 解析
inline std::optional<int> parseInt(const std::string& s);
inline std::optional<double> parseDouble(const std::string& s);
inline std::optional<bool> parseBoolish(const std::string& s);
inline std::unordered_map<std::string, std::string> parseParams(const std::string& paramStr);

// 格式化
inline const std::unordered_map<std::string, std::string>& colorMap();
inline std::string styleSpecToCodes(const std::string& spec);
inline size_t visibleLength(std::string_view s);
inline std::string stripColorCodes(std::string_view s);
inline std::string addThousandSeparators(std::string s);
inline std::string siScale(double v, int base, int decimals, bool doRound);
inline std::string formatNumber(double x, int decimals, bool doRound);
inline bool matchCond(double v, const std::string& condRaw);
inline std::optional<std::string> evalThresholds(double v, const std::string& spec);
inline std::optional<std::string> evalMap(const std::string& raw, const std::string& spec);
inline std::optional<std::string> evalMapCI(const std::string& raw, const std::string& spec);

// 辅助函数：处理条件 if/then/else
void applyConditionalFormatting(
    std::string&                                       out,
    const std::string&                                 rawValue,
    const std::optional<double>&                       maybeNum,
    const std::unordered_map<std::string, std::string>& params
);

// 辅助函数：处理布尔值专用映射
void applyBooleanFormatting(
    std::string&                                       out,
    const std::optional<bool>&                         maybeBool,
    const std::unordered_map<std::string, std::string>& params
);

// 辅助函数：处理数字格式化
void applyNumberFormatting(
    std::string&                                       out,
    const std::optional<double>&                       maybeNum,
    const std::unordered_map<std::string, std::string>& params
);

// 辅助函数：处理通用映射
void applyStringMapping(
    std::string&                                       out,
    const std::optional<bool>&                         maybeBool,
    const std::optional<double>&                       maybeNum,
    const std::unordered_map<std::string, std::string>& params
);

// 辅助函数：处理字符串替换
void applyStringReplacement(
    std::string&                                       out,
    const std::unordered_map<std::string, std::string>& params
);

// 辅助函数：处理大小写转换
void applyCaseConversion(
    std::string&                                       out,
    const std::unordered_map<std::string, std::string>& params
);

// 辅助函数：处理文本效果（去色码、JSON转义、千分位、颜色/样式、前缀/后缀、截断、对齐/填充、颜色复位、空值处理）
void applyTextEffects(
    std::string&                                       out,
    const std::unordered_map<std::string, std::string>& params
);

std::string applyFormatting(const std::string& rawValue, const std::string& paramStr);

// 寻找分隔符（忽略花括号中的内容以及引号内内容）
std::optional<size_t> findSepOutside(std::string_view s, std::string_view needle);

} // namespace PA::Utils
