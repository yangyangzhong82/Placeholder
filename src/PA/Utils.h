#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>
#include <algorithm>
#include <cctype>

namespace PA::Utils {

// 前向声明
class ParsedParams;

// 字符串处理
inline bool isSpace(unsigned char ch) { return std::isspace(ch) != 0; }

inline std::string_view ltrim_sv(std::string_view s) {
    auto it = std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !isSpace(ch); });
    s.remove_prefix(std::distance(s.begin(), it));
    return s;
}
inline std::string_view rtrim_sv(std::string_view s) {
    auto it = std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !isSpace(ch); });
    s.remove_suffix(std::distance(s.rbegin(), it));
    return s;
}
inline std::string_view trim_sv(std::string_view s) { return ltrim_sv(rtrim_sv(s)); }


inline std::string ltrim(std::string s);
inline std::string rtrim(std::string s);
inline std::string trim(std::string s);

inline std::string toLower(std::string s);
inline bool iequals(std::string a, std::string b);

// 解析
inline std::optional<int>    parseInt(const std::string& s);
inline std::optional<double> parseDouble(const std::string& s);
inline std::optional<bool>   parseBoolish(const std::string& s);

/**
 * @brief 解析后的参数视图，提供类型化访问和缓存
 *
 * 该结构是不可变的，一次解析，多处复用。
 */
class ParsedParams {
public:
    ParsedParams(std::string_view paramStr);

    // 获取原始字符串值
    std::optional<std::string_view> get(const std::string& key) const;

    // 获取并缓存类型化值
    std::optional<bool>   getBool(const std::string& key) const;
    std::optional<int>    getInt(const std::string& key) const;
    std::optional<double> getDouble(const std::string& key) const;

    // 检查是否存在某个键
    bool has(const std::string& key) const;

    // 允许遍历原始参数
    const std::unordered_map<std::string, std::string>& getRawParams() const { return mParams; }

private:
    std::unordered_map<std::string, std::string> mParams;

    // 类型化缓存
    mutable std::unordered_map<std::string, std::optional<bool>>   mBoolCache;
    mutable std::unordered_map<std::string, std::optional<int>>    mIntCache;
    mutable std::unordered_map<std::string, std::optional<double>> mDoubleCache;
};


// 格式化
inline const std::unordered_map<std::string, std::string>& colorMap();
inline std::string styleSpecToCodes(const std::string& spec);
inline size_t      visibleLength(std::string_view s);
std::string        truncateVisible(
           std::string_view s, size_t maxlen, std::string_view ellipsis, bool preserve_styles
       );
inline std::string stripColorCodes(std::string_view s);
inline std::string addThousandSeparators(std::string s, char groupSep = ',', char decimalSep = '.');
inline std::string siScale(double v, int base, int decimals, bool doRound);
inline std::string formatNumber(double x, int decimals, bool doRound);
inline bool        matchCond(double v, const std::string& condRaw);
// 增强版 evalThresholds
struct ThresholdResult {
    std::string text;
    bool        matched{false};
};
inline std::optional<ThresholdResult> evalThresholds(double v, const std::string& spec);
inline std::optional<std::string> evalMap(const std::string& raw, const std::string& spec);
inline std::optional<std::string> evalMapCI(const std::string& raw, const std::string& spec);

// 数学函数
inline double math_sqrt(double v);
inline double math_round(double v);
inline double math_floor(double v);
inline double math_ceil(double v);
inline double math_abs(double v);
inline double math_min(double a, double b);
inline double math_max(double a, double b);

// 数学表达式求值
std::optional<double> evalMathExpression(const std::string& expression, const ParsedParams& params);

// 辅助函数：处理条件 if/then/else
void applyConditionalFormatting(
    std::string&                 out,
    const std::string&           rawValue,
    const std::optional<double>& maybeNum,
    const ParsedParams&          params
);

// 辅助函数：处理布尔值专用映射
void applyBooleanFormatting(
    std::string&               out,
    const std::optional<bool>& maybeBool,
    const ParsedParams&        params
);

// 辅助函数：处理数字格式化
void applyNumberFormatting(
    std::string&                 out,
    const std::optional<double>& maybeNum,
    const ParsedParams&          params
);

// 辅助函数：处理通用映射
void applyStringMapping(
    std::string&               out,
    const std::optional<bool>& maybeBool,
    const std::optional<double>&                       maybeNum,
    const ParsedParams&        params
);

// 辅助函数：处理字符串替换
void applyStringReplacement(std::string& out, const ParsedParams& params);

// 辅助函数：处理大小写转换
void applyCaseConversion(std::string& out, const ParsedParams& params);

// 辅助函数：处理文本效果（去色码、JSON转义、千分位、颜色/样式、前缀/后缀、截断、对齐/填充、颜色复位、空值处理）
void applyTextEffects(std::string& out, const ParsedParams& params);

// 旧接口，内部转换为新接口
std::string applyFormatting(const std::string& rawValue, const std::string& paramStr);
// 新接口，使用预解析的参数
std::string applyFormatting(const std::string& rawValue, const ParsedParams& params);


// 寻找分隔符（忽略花括号中的内容以及引号内内容）
std::optional<size_t> findSepOutside(std::string_view s, std::string_view needle);

} // namespace PA::Utils
