#include "Utils.h"

#include <utf8.h>

#include "exprtk.hpp"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string_view>

#include "PA/Config/ConfigManager.h" // 引入 ConfigManager
#include "PA/logger.h"               // 引入 logger

namespace PA::Utils {

// 内部使用的参数解析实现
static std::unordered_map<std::string, std::string> parseParamsInternal(std::string_view s) {
    // 支持引号、转义：key=value;key2="a;b\"c";key3='x\';y'
    std::unordered_map<std::string, std::string> params;
    size_t                                       i = 0, n = s.size();

    auto skipSpaces = [&]() {
        while (i < n && isSpace((unsigned char)s[i])) ++i;
    };
    auto readKey = [&]() -> std::string_view {
        size_t start = i;
        while (i < n && s[i] != '=' && s[i] != ';') ++i;
        return trim_sv(s.substr(start, i - start));
    };
    auto readValue = [&]() -> std::string {
        if (i >= n) return "";
        if (s[i] == '"' || s[i] == '\'') {
            char        quote = s[i++];
            std::string val;
            while (i < n) {
                char c = s[i++];
                if (c == '\\') {
                    if (i < n) {
                        char esc = s[i++];
                        switch (esc) {
                        case 'n':
                            val.push_back('\n');
                            break;
                        case 't':
                            val.push_back('\t');
                            break;
                        case 'r':
                            val.push_back('\r');
                            break;
                        default:
                            val.push_back(esc);
                            break;
                        }
                    }
                } else if (c == quote) {
                    break;
                } else {
                    val.push_back(c);
                }
            }
            return val;
        } else {
            size_t start = i;
            while (i < n && s[i] != ';') ++i;
            return std::string(trim_sv(s.substr(start, i - start)));
        }
    };

    while (i < n) {
        skipSpaces();
        if (i >= n) break;
        std::string_view key_sv = readKey();
        std::string      key    = toLower(std::string(key_sv));
        if (i < n && s[i] == '=') {
            ++i;
            std::string val = readValue();
            params[key]     = val;
        } else {
            if (!key.empty()) params[key] = "";
        }
        if (i < n && s[i] == ';') ++i;
    }
    return params;
}


// ParsedParams 实现
ParsedParams::ParsedParams(std::string_view paramStr) { mParams = parseParamsInternal(paramStr); }

std::optional<std::string_view> ParsedParams::get(const std::string& key) const {
    auto it = mParams.find(key);
    if (it != mParams.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<bool> ParsedParams::getBool(const std::string& key) const {
    auto it = mBoolCache.find(key);
    if (it != mBoolCache.end()) return it->second;

    auto val = get(key);
    if (!val) return mBoolCache[key] = std::nullopt;

    return mBoolCache[key] = parseBoolish(std::string(*val));
}

std::optional<int> ParsedParams::getInt(const std::string& key) const {
    auto it = mIntCache.find(key);
    if (it != mIntCache.end()) return it->second;

    auto val = get(key);
    if (!val) return mIntCache[key] = std::nullopt;

    return mIntCache[key] = parseInt(std::string(*val));
}

std::optional<double> ParsedParams::getDouble(const std::string& key) const {
    auto it = mDoubleCache.find(key);
    if (it != mDoubleCache.end()) return it->second;

    auto val = get(key);
    if (!val) return mDoubleCache[key] = std::nullopt;

    return mDoubleCache[key] = parseDouble(std::string(*val));
}

bool ParsedParams::has(const std::string& key) const { return mParams.count(key) > 0; }

// 颜色/样式映射
inline const std::unordered_map<std::string, std::string>& colorMap() {
    static const std::unordered_map<std::string, std::string> m = {
        {"black",         "§0"},
        {"dark_blue",     "§1"},
        {"darkblue",      "§1"},
        {"navy",          "§1"},
        {"dark_green",    "§2"},
        {"darkgreen",     "§2"},
        {"dark_aqua",     "§3"},
        {"teal",          "§3"},
        {"darkcyan",      "§3"},
        {"dark_red",      "§4"},
        {"darkred",       "§4"},
        {"dark_purple",   "§5"},
        {"purple",        "§5"},
        {"gold",          "§6"},
        {"orange",        "§6"},
        {"gray",          "§7"},
        {"grey",          "§7"},
        {"dark_gray",     "§8"},
        {"darkgrey",      "§8"},
        {"blue",          "§9"},
        {"green",         "§a"},
        {"light_green",   "§a"},
        {"lime",          "§a"},
        {"aqua",          "§b"},
        {"cyan",          "§b"},
        {"red",           "§c"},
        {"light_purple",  "§d"},
        {"pink",          "§d"},
        {"magenta",       "§d"},
        {"yellow",        "§e"},
        {"white",         "§f"},
        {"reset",         "§r"},
        {"bold",          "§l"},
        {"italic",        "§o"},
        {"underline",     "§n"},
        {"strikethrough", "§m"},
        {"obfuscated",    "§k"},
    };
    return m;
}

inline std::string styleSpecToCodes(const std::string& spec) {
    auto s = trim(spec);
    if (s.empty()) return "";

    if (s.size() >= 2 && s.rfind("§", 0) == 0 && s.find('+') == std::string::npos) {
        return s;
    }

    std::string       result;
    std::stringstream ss(s);
    std::string       token;
    while (std::getline(ss, token, '+')) {
        token = trim(token);
        if (token.empty()) continue;
        if (!token.empty() && token.rfind("§", 0) == 0) {
            result += token;
            continue;
        }
        auto it = colorMap().find(toLower(token));
        if (it != colorMap().end()) {
            result += it->second;
        }
    }
    return result;
}

// 计算可见长度（忽略 §x 风格码），支持 UTF-8
inline size_t visibleLength(std::string_view s) {
    size_t                                vis = 0;
    std::string_view::const_iterator      it  = s.begin();
    const std::string_view::const_iterator end = s.end();

    while (it != end) {
        if (*it == '\xA7') {
            // 跳过颜色代码
            it++;
            if (it != end) {
                it++;
            }
        } else {
            try {
                // 向前移动一个 UTF-8 码位
                utf8::next(it, end);
                vis++;
            } catch (const std::exception&) {
                // 处理无效的 UTF-8 序列或不完整的字符
                // 将其视为单个字节并继续
                it++;
                vis++;
            }
        }
    }
    return vis;
}

// 可见安全截断，支持 UTF-8
std::string truncateVisible(
    std::string_view s, size_t maxlen, std::string_view ellipsis, bool preserve_styles
) {
    if (s.empty() || visibleLength(s) <= maxlen) {
        return std::string(s);
    }

    size_t                                 vis_count = 0;
    std::string_view::const_iterator       it        = s.begin();
    const std::string_view::const_iterator end       = s.end();
    std::string_view::const_iterator       truncate_it = it;

    std::vector<char>   active_formats;
    std::optional<char> last_color;

    while (it != end) {
        if (vis_count >= maxlen) {
            break;
        }

        truncate_it = it; // 记录当前安全截断点

        if (*it == '\xA7' && std::distance(it, end) > 1) {
            auto next_it = it;
            next_it++;
            char code = tolower(*next_it);
            if (preserve_styles) {
                if ((code >= '0' && code <= '9') || (code >= 'a' && code <= 'f')) {
                    last_color = code;
                    active_formats.clear();
                } else if (code >= 'k' && code <= 'o') {
                    if (std::find(active_formats.begin(), active_formats.end(), code)
                        == active_formats.end()) {
                        active_formats.push_back(code);
                    }
                } else if (code == 'r') {
                    last_color.reset();
                    active_formats.clear();
                }
            }
            it++;
            it++;
        } else {
            try {
                utf8::next(it, end);
                vis_count++;
            } catch (const std::exception&) {
                it++; // 将无效字节视为单个字符
                vis_count++;
            }
        }
    }
    if (vis_count < maxlen) {
        truncate_it = end;
    }

    size_t      byte_pos = std::distance(s.begin(), truncate_it);
    std::string result   = std::string(s.substr(0, byte_pos));
    result += ellipsis;

    if (preserve_styles) {
        std::string styles_to_restore;
        if (last_color) {
            styles_to_restore += '\xA7';
            styles_to_restore += *last_color;
        }
        for (char fmt : active_formats) {
            styles_to_restore += '\xA7';
            styles_to_restore += fmt;
        }
        result += styles_to_restore;
    }

    return result;
}

inline std::string stripColorCodes(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size();) {
        if (s[i] == '\xA7') {
            i += (i + 1 < s.size() ? 2 : 1);
            continue;
        }
        out.push_back(s[i++]);
    }
    return out;
}

inline std::string addThousandSeparators(std::string s, char groupSep, char decimalSep) {
    // 仅处理纯数字段（可带 +/- 和小数点），不处理科学计数法
    std::string sign;
    if (!s.empty() && (s[0] == '+' || s[0] == '-')) {
        sign = s.substr(0, 1);
        s.erase(s.begin());
    }
    auto        dot      = s.find('.');
    std::string intPart  = dot == std::string::npos ? s : s.substr(0, dot);
    std::string fracPart = dot == std::string::npos ? "" : s.substr(dot);

    if (fracPart.length() > 0 && decimalSep != '.') {
        fracPart[0] = decimalSep;
    }

    std::string out;
    out.reserve(s.size() + s.size() / 3);
    int count = 0;
    for (int i = (int)intPart.size() - 1; i >= 0; --i) {
        out.push_back(intPart[(size_t)i]);
        if (++count == 3 && i > 0) {
            out.push_back(groupSep);
            count = 0;
        }
    }
    std::reverse(out.begin(), out.end());
    return sign + out + fracPart;
}

// SI 缩放
inline std::string siScale(
    double             v,
    int                base,
    int                decimals,
    bool               doRound,
    bool               space,
    std::string        unitCase,
    const std::string& unit
) {
    static const char* units1000[]     = {"", "K", "M", "G", "T", "P", "E"};
    static const char* units1000_low[] = {"", "k", "m", "g", "t", "p", "e"};
    static const char* units1024[]     = {"", "Ki", "Mi", "Gi", "Ti", "Pi", "Ei"};
    static const char* units1024_low[] = {"", "ki", "mi", "gi", "ti", "pi", "ei"};

    const char** units;
    if (base == 1024) {
        units = (unitCase == "lower") ? units1024_low : units1024;
    } else {
        units = (unitCase == "lower") ? units1000_low : units1000;
    }

    int    idx = 0;
    double x   = std::fabs(v);
    while (x >= base && idx < 6) {
        x /= base;
        v /= base;
        ++idx;
    }
    double factor = std::pow(10.0, (double)std::max(0, decimals));
    double y      = doRound ? std::round(v * factor) / factor : std::trunc(v * factor) / factor;

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(std::max(0, decimals)) << y;

    std::string suffix_part;
    if (idx > 0) {
        suffix_part += units[idx];
    }
    suffix_part += unit;

    if (!suffix_part.empty() && space) {
        oss << " ";
    }
    oss << suffix_part;

    return oss.str();
}

inline std::string formatNumber(double x, int decimals, bool doRound, bool trimzeros = false) {
    if (decimals < 0) {
        // 默认尽可能精简地输出
        std::ostringstream oss;
        oss << x;
        return oss.str();
    }
    double             factor = std::pow(10.0, (double)decimals);
    double             y      = doRound ? std::round(x * factor) / factor : std::trunc(x * factor) / factor;
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(decimals) << y;
    if (trimzeros) {
        std::string s = oss.str();
        auto        dot_pos = s.find('.');
        if (dot_pos != std::string::npos) {
            s.erase(s.find_last_not_of('0') + 1, std::string::npos);
            if (s.back() == '.') {
                s.pop_back();
            }
        }
        return s;
    }
    return oss.str();
}

inline bool matchCond(double v, const std::string& condRaw) {
    auto cond = trim(condRaw);
    if (cond.empty()) return false;

    auto dash = cond.find('-');
    if (dash != std::string::npos) {
        auto a = parseDouble(cond.substr(0, dash));
        auto b = parseDouble(cond.substr(dash + 1));
        if (a && b) return v >= *a && v <= *b;
    }

    auto startsWith = [&](const char* p) {
        size_t n = std::strlen(p);
        return cond.size() >= n && cond.substr(0, n) == p;
    };

    if (startsWith(">=")) {
        auto num = parseDouble(cond.substr(2));
        return num ? (v >= *num) : false;
    }
    if (startsWith("<=")) {
        auto num = parseDouble(cond.substr(2));
        return num ? (v <= *num) : false;
    }
    if (startsWith("==")) {
        auto num = parseDouble(cond.substr(2));
        return num ? (v == *num) : false;
    }
    if (startsWith(">")) {
        auto num = parseDouble(cond.substr(1));
        return num ? (v > *num) : false;
    }
    if (startsWith("<")) {
        auto num = parseDouble(cond.substr(1));
        return num ? (v < *num) : false;
    }

    if (auto num = parseDouble(cond)) {
        return v == *num;
    }

    return false;
}

inline std::optional<ThresholdResult> evalThresholds(double v, const std::string& spec) {
    ThresholdResult   result;
    std::string       defaultVal;
    std::stringstream ss(spec);
    std::string       item;
    bool              found = false;

    while (std::getline(ss, item, ',')) {
        auto part = trim(item);
        if (part.empty()) continue;
        auto col = part.find(':');
        if (col == std::string::npos) continue;

        auto lhs = trim(part.substr(0, col));
        auto rhs = trim(part.substr(col + 1));

        if (lhs == "default" || lhs == "*") {
            defaultVal = rhs;
            continue;
        }
        if (matchCond(v, lhs)) {
            result.text    = rhs;
            result.matched = true;
            found          = true;
            break; // 找到第一个就停止
        }
    }

    if (!found && !defaultVal.empty()) {
        result.text    = defaultVal;
        result.matched = true;
    }

    if (result.matched) return result;
    return std::nullopt;
}

inline std::optional<std::string> evalMap(const std::string& raw, const std::string& spec) {
    std::string       rawLower = toLower(trim(raw));
    std::string       defaultVal;
    std::stringstream ss(spec);
    std::string       item;

    while (std::getline(ss, item, ',')) {
        auto part = trim(item);
        if (part.empty()) continue;
        auto col = part.find(':');
        if (col == std::string::npos) continue;

        auto key = toLower(trim(part.substr(0, col)));
        auto val = trim(part.substr(col + 1));

        if (key == "default" || key == "*") {
            defaultVal = val;
            continue;
        }
        if (key == rawLower) {
            return val;
        }
    }
    if (!defaultVal.empty()) return defaultVal;
    return std::nullopt;
}

inline std::optional<std::string> evalMapCI(const std::string& raw, const std::string& spec) {
    return evalMap(raw, spec); // 已经小写比对
}

// 数学函数实现
inline double math_sqrt(double v) { return std::sqrt(v); }
inline double math_round(double v) { return std::round(v); }
inline double math_floor(double v) { return std::floor(v); }
inline double math_ceil(double v) { return std::ceil(v); }
inline double math_abs(double v) { return std::fabs(v); }
inline double math_min(double a, double b) { return std::min(a, b); }
inline double math_max(double a, double b) { return std::max(a, b); }

// 辅助函数：处理条件 if/then/else
void applyConditionalFormatting(
    std::string&                 out,
    const std::string&           rawValue,
    const std::optional<double>& maybeNum,
    const ParsedParams&          params
) {
    if (auto cond = params.get("cond")) {
        if (maybeNum) {
            bool ok = matchCond(*maybeNum, std::string(*cond));
            if (ok) {
                if (auto th = params.get("then")) out = *th;
            } else {
                if (auto el = params.get("else")) out = *el;
            }
        }
    }
    if (auto equals = params.get("equals")) {
        bool ci = params.getBool("ci").value_or(false);
        bool ok = ci ? iequals(out, std::string(*equals)) : (trim(out) == trim(std::string(*equals)));
        if (ok) {
            if (auto th = params.get("then")) out = *th;
        } else {
            if (auto el = params.get("else")) out = *el;
        }
    }
}

// 辅助函数：处理布尔值专用映射
void applyBooleanFormatting(
    std::string&               out,
    const std::optional<bool>& maybeBool,
    const ParsedParams&        params
) {
    if (maybeBool.has_value()) {
        bool b   = *maybeBool;
        auto itT = params.get("truetext");
        auto itF = params.get("falsetext");
        if (itT || itF) {
            out = b ? (itT ? std::string(*itT) : "true") : (itF ? std::string(*itF) : "false");
        } else if (auto itMap = params.get("map")) {
            auto mapped = evalMap(b ? "true" : "false", std::string(*itMap));
            if (!mapped) mapped = evalMap(b ? "1" : "0", std::string(*itMap));
            if (mapped) out = *mapped;
        } else if (auto itMapci = params.get("mapci")) {
            auto mapped = evalMapCI(b ? "true" : "false", std::string(*itMapci));
            if (!mapped) mapped = evalMapCI(b ? "1" : "0", std::string(*itMapci));
            if (mapped) out = *mapped;
        }
    }
}

// 辅助函数：处理数字格式化
void applyNumberFormatting(
    std::string&                 out,
    const std::optional<double>& maybeNum,
    const ParsedParams&          params
) {
    if (maybeNum.has_value()) {
        double v = *maybeNum;

        // 数学函数
        if (auto exprOpt = params.get("math")) {
            // 尝试将当前值作为参数传入数学表达式
            std::string expr = std::string(*exprOpt);
            // 替换表达式中的特殊变量 `_` 为当前值
            std::string formatted_v = formatNumber(v, -1, false, false); // 使用原始值，不进行格式化
            size_t      pos         = expr.find("_");
            while (pos != std::string::npos) {
                expr.replace(pos, 1, formatted_v);
                pos = expr.find("_", pos + formatted_v.length());
            }
            auto result = evalMathExpression(expr, params);
            if (result) {
                v = *result;
            } else {
                // 表达式求值失败
                if (auto onerror = params.get("onerror")) {
                    auto action = trim(std::string(*onerror));
                    if (iequals(action, "empty")) {
                        out.clear();
                        return; // 跳过所有数字格式化
                    }
                    if (action.size() > 5 && iequals(action.substr(0, 5), "text:")) {
                        out = trim(action.substr(5));
                        return; // 跳过所有数字格式化
                    }
                    // "keep" is default, do nothing, fall through to format original `v`
                }
            }
        }

        int  decimals  = params.getInt("decimals").value_or(-1);
        bool doRound   = params.getBool("round").value_or(true);
        bool trimzeros = params.getBool("trimzeros").value_or(false);

        // SI 缩放
        if (params.getBool("si").value_or(false)) {
            int         base     = params.getInt("base").value_or(1000) == 1024 ? 1024 : 1000;
            bool        space    = params.getBool("space").value_or(true);
            std::string unitCase = toLower(trim(std::string(params.get("unitcase").value_or("upper"))));
            std::string unit     = std::string(params.get("unit").value_or(""));
            out = siScale(v, base, decimals < 0 ? 2 : decimals, doRound, space, unitCase, unit);
        } else {
            // 默认数字格式
            out = formatNumber(v, decimals, doRound, trimzeros);
        }

        // 千分位
        if (params.getBool("commas").value_or(false)) {
            char groupSep   = ',';
            char decimalSep = '.';

            if (auto locale = params.get("locale")) {
                auto loc_sv = toLower(trim(std::string(*locale)));
                if (loc_sv == "zh_cn") {
                    // default is fine
                } else if (loc_sv == "en_us") {
                    // default is fine
                } else if (loc_sv == "de_de") {
                    groupSep   = '.';
                    decimalSep = ',';
                }
            }

            if (auto g = params.get("group")) {
                if (!g->empty()) groupSep = (*g)[0];
            }
            if (auto d = params.get("decimal")) {
                if (!d->empty()) decimalSep = (*d)[0];
            }

            out = addThousandSeparators(out, groupSep, decimalSep);
        }

        // 阈值 -> 样式/文本
        if (auto it = params.get("thresholds")) {
            if (auto matched_res = evalThresholds(v, std::string(*it))) {
                std::string tpl       = matched_res->text;
                bool        replace   = params.getBool("replace").value_or(false);
                std::string prepend   = std::string(params.get("prepend").value_or(""));
                std::string append    = std::string(params.get("append").value_or(""));
                std::string styleCode = styleSpecToCodes(tpl);

                if (replace) {
                    out = tpl;
                } else {
                    if (!styleCode.empty()) {
                        out = styleCode + out;
                    }
                    out = prepend + out + append;
                }
            }
        }
    }
}

// 辅助函数：处理通用映射
void applyStringMapping(
    std::string&               out,
    const std::optional<bool>& maybeBool,
    const std::optional<double>&                       maybeNum,
    const ParsedParams&        params
) {
    if (!maybeBool.has_value() && !maybeNum.has_value()) {
        if (auto it = params.get("map")) {
            if (auto mapped = evalMap(out, std::string(*it))) out = *mapped;
        } else if (auto it = params.get("mapci")) {
            if (auto mapped = evalMapCI(out, std::string(*it))) out = *mapped;
        }
    }
}

// 辅助函数：处理字符串替换
void applyStringReplacement(std::string& out, const ParsedParams& params) {
    if (auto it = params.get("repl")) {
        std::string       replStr(*it);
        std::stringstream ss(replStr);
        std::string       item;
        while (std::getline(ss, item, ',')) {
            auto arrow = item.find("->");
            if (arrow == std::string::npos) continue;
            auto from = item.substr(0, arrow);
            auto to   = item.substr(arrow + 2);
            if (!from.empty()) {
                size_t pos = 0;
                while ((pos = out.find(from, pos)) != std::string::npos) {
                    out.replace(pos, from.size(), to);
                    pos += to.size();
                }
            }
        }
    }
}

// 辅助函数：处理大小写转换
void applyCaseConversion(std::string& out, const ParsedParams& params) {
    if (auto it = params.get("case")) {
        auto v = toLower(std::string(*it));
        if (v == "lower") {
            std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        } else if (v == "upper") {
            std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return (char)std::toupper(c); });
        } else if (v == "title") {
            bool newWord = true;
            for (auto& ch : out) {
                if (std::isalpha((unsigned char)ch)) {
                    ch      = newWord ? (char)std::toupper((unsigned char)ch) : (char)std::tolower((unsigned char)ch);
                    newWord = false;
                } else {
                    newWord = isSpace((unsigned char)ch);
                }
            }
        }
    }
}

// 辅助函数：处理文本效果（去色码、JSON转义、千分位、颜色/样式、前缀/后缀、截断、对齐/填充、颜色复位、空值处理）
void applyTextEffects(std::string& out, const ParsedParams& params) {
    // 去色码
    if (params.getBool("stripcodes").value_or(false)) {
        out = stripColorCodes(out);
    }

    // JSON 转义
    if (params.getBool("json").value_or(false)) {
        std::string esc;
        esc.reserve(out.size() * 1.2);
        for (char c : out) {
            switch (c) {
            case '\\':
                esc += "\\\\";
                break;
            case '"':
                esc += "\\\"";
                break;
            case '\n':
                esc += "\\n";
                break;
            case '\r':
                esc += "\\r";
                break;
            case '\t':
                esc += "\\t";
                break;
            default:
                esc.push_back(c);
                break;
            }
            // TODO: 更多控制字符转义
        }
        out.swap(esc);
    }

    // 颜色/样式
    if (auto it = params.get("color")) {
        std::string codes = styleSpecToCodes(std::string(*it));
        if (!codes.empty()) out = codes + out;
    }

    // 前后缀
    if (auto it = params.get("prefix")) out = std::string(*it) + out;
    if (auto it = params.get("suffix")) out += std::string(*it);

    // 截断
    if (auto maxlenOpt = params.getInt("maxlen")) {
        size_t maxlen_val = std::max(0, *maxlenOpt);
        if (visibleLength(out) > maxlen_val) {
            std::string ellipsis_val = "...";
            if (auto e = params.get("ellipsis")) ellipsis_val = *e;
            bool preserve_styles_val = params.getBool("preserve_styles").value_or(true);
            out                      = truncateVisible(out, maxlen_val, ellipsis_val, preserve_styles_val);
        }
    }

    // 对齐/填充
    if (auto widthOpt = params.getInt("width")) {
        int width = *widthOpt;
        if (width > 0) {
            char        fill  = params.get("fill").value_or(" ").front();
            std::string align = toLower(trim(std::string(params.get("align").value_or("left"))));
            int         vis   = (int)visibleLength(out);
            if (vis < width) {
                int         pad = width - vis;
                std::string pads((size_t)pad, fill);
                if (align == "right") {
                    out = pads + out;
                } else if (align == "center" || align == "centre") {
                    int left  = pad / 2;
                    int right = pad - left;
                    out       = std::string((size_t)left, fill) + out + std::string((size_t)right, fill);
                } else {
                    out = out + pads;
                }
            }
        }
    }

    // 颜色复位
    if (params.getBool("reset").value_or(false)) {
        out += "§r";
    }

    if (out.empty()) {
        if (auto it = params.get("emptytext")) out = *it;
    }
}

std::optional<double> evalMathExpression(const std::string& expression_str, const ParsedParams& params) {
    using namespace exprtk;

    symbol_table<double>                    symbol_table;
    std::unordered_map<std::string, double> variables; // 持久化存储变量

    // 注册变量
    for (const auto& [key, value_str] : params.getRawParams()) {
        if (auto num = parseDouble(value_str)) {
            variables[key] = *num; // 存储实际值
            symbol_table.add_variable(key, variables[key]); // 传入持久化存储的引用
        }
    }

    // 注册数学函数
    symbol_table.add_function("sqrt", math_sqrt);
    symbol_table.add_function("round", math_round);
    symbol_table.add_function("floor", math_floor);
    symbol_table.add_function("ceil", math_ceil);
    symbol_table.add_function("abs", math_abs);
    symbol_table.add_function("min", math_min);
    symbol_table.add_function("max", math_max);

    expression<double> expression;
    expression.register_symbol_table(symbol_table);

    parser<double> parser;
    if (!parser.compile(expression_str, expression)) {
        // 编译失败，返回空
        if (ConfigManager::getInstance().get().debugMode) {
            logger.warn("Math expression '{}' failed to parse. Error: {}", expression_str, parser.error().c_str());
        }
        return std::nullopt;
    }

    return expression.value();
}

// 新接口
std::string applyFormatting(const std::string& rawValue, const ParsedParams& params) {
    auto maybeBool = parseBoolish(rawValue);
    auto maybeNum  = parseDouble(rawValue);

    std::string out = rawValue;

    applyConditionalFormatting(out, rawValue, maybeNum, params);
    applyBooleanFormatting(out, maybeBool, params);
    applyNumberFormatting(out, maybeNum, params);
    applyStringMapping(out, maybeBool, maybeNum, params);
    applyStringReplacement(out, params);
    applyCaseConversion(out, params);
    applyTextEffects(out, params);

    return out;
}

// 旧接口，内部转换为新接口
std::string applyFormatting(const std::string& rawValue, const std::string& paramStr) {
    if (paramStr.empty()) return rawValue;
    return applyFormatting(rawValue, ParsedParams(paramStr));
}


// 寻找分隔符（忽略花括号中的内容以及引号内内容）
std::optional<size_t> findSepOutside(std::string_view s, std::string_view needle) {
    if (needle.empty()) return std::nullopt;
    size_t depth   = 0;
    bool   inQuote = false;
    char   quote   = '\0';
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (!inQuote) {
            if (c == '"' || c == '\'') {
                inQuote = true;
                quote   = c;
                continue;
            }
            if (c == '{') {
                // 双大括号视为转义字面量
                if (i + 1 < s.size() && s[i + 1] == '{') {
                    ++i;
                    continue;
                }
                ++depth;
                continue;
            }
            if (c == '}') {
                if (i + 1 < s.size() && s[i + 1] == '}') {
                    ++i;
                    continue;
                }
                if (depth > 0) --depth;
                continue;
            }
            if (depth == 0) {
                // 匹配 needle
                bool ok = true;
                for (size_t k = 0; k < needle.size(); ++k) {
                    if (i + k >= s.size() || s[i + k] != needle[k]) {
                        ok = false;
                        break;
                    }
                }
                if (ok) return i;
            }
        } else {
            if (c == '\\') {
                ++i;
                continue;
            }
            if (c == quote) {
                inQuote = false;
                quote   = '\0';
            }
        }
    }
    return std::nullopt;
}

std::string join(const std::vector<std::string>& elements, std::string_view separator) {
    if (elements.empty()) {
        return "";
    }
    std::string result;
    result.reserve(elements.size() * 10); // Pre-allocate some memory
    result += elements[0];
    for (size_t i = 1; i < elements.size(); ++i) {
        result += separator;
        result += elements[i];
    }
    return result;
}

} // namespace PA::Utils
