#include "PlaceholderManager.h"

#include "ll/api/service/Bedrock.h"
#include "mc/network/ServerNetworkHandler.h"
#include "mc/server/ServerPlayer.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/Level.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <iomanip>
#include <mutex>
#include <optional>
#include <queue>
#include <random>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>


namespace PA {

// 工具函数（匿名命名空间）
namespace {

inline bool isSpace(unsigned char ch) { return std::isspace(ch) != 0; }

inline std::string ltrim(std::string s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !isSpace(ch); }));
    return s;
}
inline std::string rtrim(std::string s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !isSpace(ch); }).base(), s.end());
    return s;
}
inline std::string trim(std::string s) { return rtrim(ltrim(std::move(s))); }

inline std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

inline bool iequals(std::string a, std::string b) { return toLower(trim(std::move(a))) == toLower(trim(std::move(b))); }

// faster parse int
inline std::optional<int> parseInt(const std::string& s) {
    auto t     = trim(s);
    int  v     = 0;
    auto first = t.data();
    auto last  = t.data() + t.size();
    if (first == last) return std::nullopt;
    std::from_chars_result res = std::from_chars(first, last, v);
    if (res.ec == std::errc() && res.ptr == last) return v;
    return std::nullopt;
}

// faster parse double (C++23: from_chars for double widely supported)
inline std::optional<double> parseDouble(const std::string& s) {
    auto t = trim(s);
    if (t.empty()) return std::nullopt;
    double v     = 0.0;
    auto   first = t.data();
    auto   last  = t.data() + t.size();
    auto   res   = std::from_chars(first, last, v);
    if (res.ec == std::errc() && res.ptr == last) return v;
    // 作为兜底（locale 安全）再试一试 stod
    try {
        size_t idx = 0;
        double x   = std::stod(t, &idx);
        if (idx == t.size()) return x;
    } catch (...) {}
    return std::nullopt;
}

inline std::optional<bool> parseBoolish(const std::string& s) {
    auto v = toLower(trim(s));
    if (v == "true" || v == "yes" || v == "y" || v == "1" || v == "on") return true;
    if (v == "false" || v == "no" || v == "n" || v == "0" || v == "off") return false;
    return std::nullopt;
}

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

// 计算可见长度（忽略 §x 风格码）
inline size_t visibleLength(std::string_view s) {
    size_t i = 0, n = s.size(), vis = 0;
    while (i < n) {
        if (s[i] == '\xA7') {
            if (i + 1 < n) i += 2;
            else ++i;
        } else {
            ++vis;
            ++i;
        }
    }
    return vis;
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

inline std::string addThousandSeparators(std::string s) {
    // 仅处理纯数字段（可带 +/- 和小数点），不处理科学计数法
    std::string sign;
    if (!s.empty() && (s[0] == '+' || s[0] == '-')) {
        sign = s.substr(0, 1);
        s.erase(s.begin());
    }
    auto        dot      = s.find('.');
    std::string intPart  = dot == std::string::npos ? s : s.substr(0, dot);
    std::string fracPart = dot == std::string::npos ? "" : s.substr(dot);

    std::string out;
    out.reserve(s.size() + s.size() / 3);
    int count = 0;
    for (int i = (int)intPart.size() - 1; i >= 0; --i) {
        out.push_back(intPart[(size_t)i]);
        if (++count == 3 && i > 0) {
            out.push_back(',');
            count = 0;
        }
    }
    std::reverse(out.begin(), out.end());
    return sign + out + fracPart;
}

// SI 缩放
inline std::string siScale(double v, int base, int decimals, bool doRound) {
    static const char* units1000[] = {"", "K", "M", "G", "T", "P", "E"};
    static const char* units1024[] = {"", "Ki", "Mi", "Gi", "Ti", "Pi", "Ei"};
    const char**       units       = (base == 1024) ? units1024 : units1000;
    int                idx         = 0;
    double             x           = std::fabs(v);
    while (x >= base && idx < 6) {
        x /= base;
        v /= base;
        ++idx;
    }
    double factor = std::pow(10.0, (double)std::max(0, decimals));
    double y      = doRound ? std::round(v * factor) / factor : std::trunc(v * factor) / factor;

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(std::max(0, decimals)) << y << units[idx];
    return oss.str();
}

inline std::unordered_map<std::string, std::string> parseParams(const std::string& paramStr) {
    // 支持引号、转义：key=value;key2="a;b\"c";key3='x\';y'
    std::unordered_map<std::string, std::string> params;
    std::string                                  s = paramStr;
    size_t                                       i = 0, n = s.size();

    auto skipSpaces = [&]() {
        while (i < n && isSpace((unsigned char)s[i])) ++i;
    };
    auto readKey = [&]() -> std::string {
        size_t start = i;
        while (i < n && s[i] != '=' && s[i] != ';') ++i;
        return trim(s.substr(start, i - start));
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
            return trim(s.substr(start, i - start));
        }
    };

    while (i < n) {
        skipSpaces();
        if (i >= n) break;
        std::string key = readKey();
        if (i < n && s[i] == '=') {
            ++i;
            std::string val      = readValue();
            params[toLower(key)] = val;
        } else {
            if (!key.empty()) params[toLower(key)] = "";
        }
        if (i < n && s[i] == ';') ++i;
    }
    return params;
}

inline std::string formatNumber(double x, int decimals, bool doRound) {
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

inline std::optional<std::string> evalThresholds(double v, const std::string& spec) {
    std::string       defaultVal;
    std::stringstream ss(spec);
    std::string       item;

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
            return rhs;
        }
    }

    if (!defaultVal.empty()) return defaultVal;
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

inline std::string applyFormatting(const std::string& rawValue, const std::string& paramStr) {
    if (paramStr.empty()) return rawValue;
    auto params = parseParams(paramStr);

    auto maybeBool = parseBoolish(rawValue);
    auto maybeNum  = parseDouble(rawValue);

    std::string out = rawValue;

    // 条件 if/then/else
    if (auto it = params.find("cond"); it != params.end()) {
        if (maybeNum) {
            bool ok = matchCond(*maybeNum, it->second);
            if (ok) {
                if (auto th = params.find("then"); th != params.end()) out = th->second;
            } else {
                if (auto el = params.find("else"); el != params.end()) out = el->second;
            }
        }
    }
    if (auto it = params.find("equals"); it != params.end()) {
        bool ci = false;
        if (auto ciit = params.find("ci"); ciit != params.end()) {
            if (auto bv = parseBoolish(ciit->second)) ci = *bv;
        }
        bool ok = ci ? iequals(out, it->second) : (trim(out) == trim(it->second));
        if (ok) {
            if (auto th = params.find("then"); th != params.end()) out = th->second;
        } else {
            if (auto el = params.find("else"); el != params.end()) out = el->second;
        }
    }

    // Bool 专用映射
    if (maybeBool.has_value()) {
        bool b   = *maybeBool;
        auto itT = params.find("truetext");
        auto itF = params.find("falsetext");
        if (itT != params.end() || itF != params.end()) {
            out = b ? (itT != params.end() ? itT->second : "true") : (itF != params.end() ? itF->second : "false");
        } else if (auto itMap = params.find("map"); itMap != params.end()) {
            auto mapped = evalMap(b ? "true" : "false", itMap->second);
            if (!mapped) mapped = evalMap(b ? "1" : "0", itMap->second);
            if (mapped) out = *mapped;
        } else if (auto itMapci = params.find("mapci"); itMapci != params.end()) {
            auto mapped = evalMapCI(b ? "true" : "false", itMapci->second);
            if (!mapped) mapped = evalMapCI(b ? "1" : "0", itMapci->second);
            if (mapped) out = *mapped;
        }
    }

    // 数字格式化
    if (maybeNum.has_value()) {
        double v = *maybeNum;

        if (auto it = params.find("abs"); it != params.end()) {
            if (auto bv = parseBoolish(it->second); bv && *bv) v = std::fabs(v);
        }

        int  decimals = -1;
        bool doRound  = true;
        if (auto it = params.find("decimals"); it != params.end()) {
            if (auto iv = parseInt(it->second)) decimals = *iv;
        }
        if (auto it = params.find("round"); it != params.end()) {
            if (auto bv = parseBoolish(it->second)) doRound = *bv;
        }

        // SI 缩放
        if (auto it = params.find("si"); it != params.end()) {
            if (auto bv = parseBoolish(it->second); bv && *bv) {
                int base = 1000;
                if (auto b = params.find("base"); b != params.end()) {
                    if (auto iv = parseInt(b->second)) base = (*iv == 1024 ? 1024 : 1000);
                }
                out = siScale(v, base, decimals < 0 ? 2 : decimals, doRound);
            } else {
                // 默认数字格式
                out = formatNumber(v, decimals, doRound);
            }
        } else {
            out = formatNumber(v, decimals, doRound);
        }

        // 阈值 -> 样式/文本
        if (auto it = params.find("thresholds"); it != params.end()) {
            if (auto matched = evalThresholds(v, it->second)) {
                std::string codes = styleSpecToCodes(*matched);
                if (!codes.empty()) {
                    out = codes + out;
                } else {
                    bool showValue = true;
                    if (auto sv = params.find("showvalue"); sv != params.end()) {
                        if (auto bv = parseBoolish(sv->second)) showValue = *bv;
                    }
                    if (!showValue) {
                        out = *matched;
                    } else {
                        out = *matched; // 再附加值可自定
                    }
                }
            }
        }
    } else {
        if (auto it = params.find("map"); it != params.end()) {
            if (auto mapped = evalMap(out, it->second)) out = *mapped;
        } else if (auto it = params.find("mapci"); it != params.end()) {
            if (auto mapped = evalMapCI(out, it->second)) out = *mapped;
        }
    }

    // 字符串替换：repl=a->b,c->d
    if (auto it = params.find("repl"); it != params.end()) {
        std::stringstream ss(it->second);
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

    // 大小写
    if (auto it = params.find("case"); it != params.end()) {
        auto v = toLower(it->second);
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

    // 去色码
    if (auto it = params.find("stripcodes"); it != params.end()) {
        if (auto bv = parseBoolish(it->second); bv && *bv) out = stripColorCodes(out);
    }

    // JSON 转义
    if (auto it = params.find("json"); it != params.end()) {
        if (auto bv = parseBoolish(it->second); bv && *bv) {
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
            }
            out.swap(esc);
        }
    }

    // 千分位
    if (auto it = params.find("commas"); it != params.end()) {
        if (auto bv = parseBoolish(it->second); bv && *bv) out = addThousandSeparators(out);
    }

    // 颜色/样式
    if (auto it = params.find("color"); it != params.end()) {
        std::string codes = styleSpecToCodes(it->second);
        if (!codes.empty()) out = codes + out;
    }

    // 前后缀
    if (auto it = params.find("prefix"); it != params.end()) out = it->second + out;
    if (auto it = params.find("suffix"); it != params.end()) out += it->second;

    // 截断
    if (auto it = params.find("maxlen"); it != params.end()) {
        if (auto iv = parseInt(it->second)) {
            int maxlen = std::max(0, *iv);
            int vis    = (int)visibleLength(out);
            if (vis > maxlen) {
                std::string ell = "...";
                if (auto e = params.find("ellipsis"); e != params.end()) ell = e->second;
                // 简化处理：按字节近似截断
                if ((int)out.size() > maxlen) out.resize((size_t)maxlen);
                out += ell;
            }
        }
    }

    // 对齐/填充
    int width = 0;
    if (auto it = params.find("width"); it != params.end()) {
        if (auto iv = parseInt(it->second)) width = *iv;
    }
    if (width > 0) {
        char fill = ' ';
        if (auto it = params.find("fill"); it != params.end() && !it->second.empty()) fill = it->second[0];
        std::string align = "left";
        if (auto it = params.find("align"); it != params.end()) align = toLower(trim(it->second));
        int vis = (int)visibleLength(out);
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

    // 颜色复位
    if (auto it = params.find("reset"); it != params.end()) {
        if (auto bv = parseBoolish(it->second); bv && *bv) out += "§r";
    }

    if (out.empty()) {
        if (auto it = params.find("emptytext"); it != params.end()) out = it->second;
    }

    return out;
}

// 寻找分隔符（忽略花括号中的内容以及引号内内容）
inline std::optional<size_t> findSepOutside(std::string_view s, std::string_view needle) {
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

} // namespace

// 单例
PlaceholderManager& PlaceholderManager::getInstance() {
    static PlaceholderManager instance;
    return instance;
}

PlaceholderManager::PlaceholderManager() = default;

// ==== 类型系统实现 ====

std::size_t PlaceholderManager::ensureTypeId(const std::string& typeKeyStr) {
    std::unique_lock lk(mMutex);
    auto             it = mTypeKeyToId.find(typeKeyStr);
    if (it != mTypeKeyToId.end()) return it->second;
    auto id                  = mNextTypeId++;
    mTypeKeyToId[typeKeyStr] = id;
    mIdToTypeKey[id]         = typeKeyStr;
    return id;
}

void PlaceholderManager::registerInheritanceByKeys(
    const std::string& derivedKey,
    const std::string& baseKey,
    Caster             caster
) {
    std::unique_lock lk(mMutex);
    auto             d = ensureTypeId(derivedKey);
    auto             b = ensureTypeId(baseKey);
    mUpcastEdges[d][b] = caster;
}

// BFS 查询 from -> to 的“最短上行路径”
bool PlaceholderManager::findUpcastChain(std::size_t fromTypeId, std::size_t toTypeId, std::vector<Caster>& outChain)
    const {
    std::shared_lock lk(mMutex);
    outChain.clear();
    if (fromTypeId == 0 || toTypeId == 0) return false;
    if (fromTypeId == toTypeId) return true; // 空链表示已是同型

    std::unordered_map<std::size_t, std::pair<std::size_t, Caster>> prev;
    std::queue<std::size_t>                                         q;

    prev[fromTypeId] = {0, nullptr};
    q.push(fromTypeId);

    while (!q.empty()) {
        auto cur = q.front();
        q.pop();

        auto it = mUpcastEdges.find(cur);
        if (it == mUpcastEdges.end()) continue;

        for (auto& [nxt, caster] : it->second) {
            if (prev.find(nxt) != prev.end()) continue;
            prev[nxt] = {cur, caster};
            if (nxt == toTypeId) {
                // 回溯构造链
                std::vector<Caster> chain;
                std::size_t         x = nxt;
                while (x != fromTypeId) {
                    auto [p, c] = prev[x];
                    chain.push_back(c);
                    x = p;
                }
                std::reverse(chain.begin(), chain.end());
                outChain = std::move(chain);
                return true;
            }
            q.push(nxt);
        }
    }
    return false;
}

// ==== 占位符注册 ====

void PlaceholderManager::registerServerPlaceholder(
    const std::string& pluginName,
    const std::string& placeholder,
    ServerReplacer     replacer
) {
    std::unique_lock lk(mMutex);
    mServerPlaceholders[pluginName][placeholder] = ServerReplacerEntry{replacer};
}

void PlaceholderManager::registerServerPlaceholderWithParams(
    const std::string&         pluginName,
    const std::string&         placeholder,
    ServerReplacerWithParams&& replacer
) {
    std::unique_lock lk(mMutex);
    mServerPlaceholders[pluginName][placeholder] = ServerReplacerEntry{ServerReplacerWithParams(std::move(replacer))};
}

void PlaceholderManager::registerPlaceholderForTypeId(
    const std::string& pluginName,
    const std::string& placeholder,
    std::size_t        targetTypeId,
    AnyPtrReplacer     replacer
) {
    std::unique_lock lk(mMutex);
    mContextPlaceholders[pluginName][placeholder].push_back(TypedReplacer{
        targetTypeId,
        std::variant<AnyPtrReplacer, AnyPtrReplacerWithParams>(std::move(replacer)),
    });
}

void PlaceholderManager::registerPlaceholderForTypeId(
    const std::string&         pluginName,
    const std::string&         placeholder,
    std::size_t                targetTypeId,
    AnyPtrReplacerWithParams&& replacer
) {
    std::unique_lock lk(mMutex);
    mContextPlaceholders[pluginName][placeholder].push_back(TypedReplacer{
        targetTypeId,
        std::variant<AnyPtrReplacer, AnyPtrReplacerWithParams>(std::move(replacer)),
    });
}

void PlaceholderManager::registerPlaceholderForTypeKey(
    const std::string& pluginName,
    const std::string& placeholder,
    const std::string& typeKeyStr,
    AnyPtrReplacer     replacer
) {
    auto id = ensureTypeId(typeKeyStr);
    registerPlaceholderForTypeId(pluginName, placeholder, id, std::move(replacer));
}

void PlaceholderManager::registerPlaceholderForTypeKeyWithParams(
    const std::string&         pluginName,
    const std::string&         placeholder,
    const std::string&         typeKeyStr,
    AnyPtrReplacerWithParams&& replacer
) {
    auto id = ensureTypeId(typeKeyStr);
    registerPlaceholderForTypeId(pluginName, placeholder, id, std::move(replacer));
}

// ==== 注销 ====

void PlaceholderManager::unregisterPlaceholders(const std::string& pluginName) {
    std::unique_lock lk(mMutex);
    mServerPlaceholders.erase(pluginName);
    mContextPlaceholders.erase(pluginName);
}

// ==== Context 构造 ====

PlaceholderContext PlaceholderManager::makeContextRaw(void* ptr, const std::string& typeKeyStr) {
    PlaceholderContext ctx;
    ctx.ptr    = ptr;
    ctx.typeId = getInstance().ensureTypeId(typeKeyStr);
    return ctx;
}

// ==== 配置 ====
void PlaceholderManager::setMaxRecursionDepth(int depth) { mMaxRecursionDepth = std::max(0, depth); }
int  PlaceholderManager::getMaxRecursionDepth() const { return mMaxRecursionDepth; }
void PlaceholderManager::setDoubleBraceEscape(bool enable) { mEnableDoubleBraceEscape = enable; }
bool PlaceholderManager::getDoubleBraceEscape() const { return mEnableDoubleBraceEscape; }

// ==== 替换 ====

std::string PlaceholderManager::replacePlaceholders(const std::string& text) {
    return replacePlaceholders(text, PlaceholderContext{nullptr, 0});
}

std::string PlaceholderManager::replacePlaceholders(const std::string& text, std::any contextObject) {
    if (!contextObject.has_value()) {
        return replacePlaceholders(text, PlaceholderContext{nullptr, 0});
    }

    if (contextObject.type() == typeid(PlaceholderContext)) {
        auto ctx = std::any_cast<PlaceholderContext>(contextObject);
        return replacePlaceholders(text, ctx);
    }

    try {
        if (contextObject.type() == typeid(Player*)) {
            auto p = std::any_cast<Player*>(contextObject);
            return replacePlaceholders(text, makeContext(p));
        }
    } catch (...) {}

    return replacePlaceholders(text, PlaceholderContext{nullptr, 0});
}

std::string PlaceholderManager::replacePlaceholders(const std::string& text, const PlaceholderContext& ctx) {
    ReplaceState st;
    return replacePlaceholdersImpl(text, ctx, st);
}

std::string PlaceholderManager::replacePlaceholders(const std::string& text, Player* player) {
    return replacePlaceholders(text, makeContext(player));
}

// 内部实现：带嵌套/转义/默认值/参数
std::string
PlaceholderManager::replacePlaceholdersImpl(const std::string& text, const PlaceholderContext& ctx, ReplaceState& st) {
    if (st.depth > mMaxRecursionDepth) {
        return text; // 超深度：直接原样返回，避免递归膨胀
    }

    std::string result;
    result.reserve(text.size() * 1.2);

    const char* s = text.c_str();
    size_t      n = text.size();

    for (size_t i = 0; i < n;) {
        char c = s[i];

        // 转义：双大括号输出单大括号
        if (mEnableDoubleBraceEscape && c == '{' && i + 1 < n && s[i + 1] == '{') {
            result.push_back('{');
            i += 2;
            continue;
        }
        if (mEnableDoubleBraceEscape && c == '}' && i + 1 < n && s[i + 1] == '}') {
            result.push_back('}');
            i += 2;
            continue;
        }

        if (c != '{') {
            result.push_back(c);
            ++i;
            continue;
        }

        // 找到匹配的 '}'
        size_t j       = i + 1;
        int    depth   = 1;
        bool   matched = false;
        while (j < n) {
            if (mEnableDoubleBraceEscape && s[j] == '{' && j + 1 < n && s[j + 1] == '{') {
                j += 2;
                continue;
            }
            if (mEnableDoubleBraceEscape && s[j] == '}' && j + 1 < n && s[j + 1] == '}') {
                j += 2;
                continue;
            }
            if (s[j] == '{') {
                ++depth;
                ++j;
                continue;
            }
            if (s[j] == '}') {
                --depth;
                if (depth == 0) {
                    matched = true;
                    break;
                }
            }
            ++j;
        }

        if (!matched) {
            // 没有匹配：余下原样拷贝
            result.append(s + i, n - i);
            break;
        }

        // 提取占位符内部
        std::string inside(text.substr(i + 1, j - (i + 1)));

        // 解析：plugin:placeholder[ :- default][ | params]
        // 先找冒号（深度=0）
        auto colonPosOpt = findSepOutside(inside, ":");
        if (!colonPosOpt) {
            // 非法：按原样输出
            result.append("{").append(inside).append("}");
            i = j + 1;
            continue;
        }
        size_t      colonPos   = *colonPosOpt;
        std::string pluginName = trim(inside.substr(0, colonPos));
        std::string rest       = inside.substr(colonPos + 1);

        // 找 default 与 | 的位置（均是深度 0）
        auto defaultPosOpt = findSepOutside(rest, ":-");
        auto pipePosOpt    = findSepOutside(rest, "|");

        size_t      nameEnd         = std::min(defaultPosOpt.value_or(rest.size()), pipePosOpt.value_or(rest.size()));
        std::string placeholderName = trim(rest.substr(0, nameEnd));

        std::string defaultText;
        if (defaultPosOpt) {
            size_t defaultStart = *defaultPosOpt + 2;
            size_t defaultEnd   = pipePosOpt ? *pipePosOpt : rest.size();
            if (defaultStart < defaultEnd) defaultText = trim(rest.substr(defaultStart, defaultEnd - defaultStart));
        }

        std::string paramString;
        if (pipePosOpt) {
            size_t paramStart = *pipePosOpt + 1;
            if (paramStart < rest.size()) paramString = trim(rest.substr(paramStart));
        }

        // 嵌套：先求 default 与 params 内的占位符
        if (!defaultText.empty()) {
            ++st.depth;
            defaultText = replacePlaceholdersImpl(defaultText, ctx, st);
            --st.depth;
        }
        if (!paramString.empty()) {
            ++st.depth;
            paramString = replacePlaceholdersImpl(paramString, ctx, st);
            --st.depth;
        }

        // 构造缓存 key
        std::string cacheKey;
        cacheKey.reserve(pluginName.size() + placeholderName.size() + paramString.size() + 32);
        cacheKey.append(std::to_string(reinterpret_cast<uintptr_t>(ctx.ptr)));
        cacheKey.push_back('#');
        cacheKey.append(std::to_string(ctx.typeId));
        cacheKey.push_back('#');
        cacheKey.append(pluginName);
        cacheKey.push_back(':');
        cacheKey.append(placeholderName);
        cacheKey.push_back('|');
        cacheKey.append(paramString);

        auto itCache = st.cache.find(cacheKey);
        if (itCache != st.cache.end()) {
            std::string out = itCache->second;
            if (out.empty() && !defaultText.empty()) out = defaultText;
            result.append(out);
            i = j + 1;
            continue;
        }

        // 查询候选：先上下文（多态），再服务器
        struct Candidate {
            std::vector<Caster>                                    chain;
            std::variant<AnyPtrReplacer, AnyPtrReplacerWithParams> fn;
        };
        std::vector<std::pair<int, Candidate>> candidates;

        // 读取占位符注册（只读锁期间构造候选，调用时释放锁）
        {
            std::shared_lock lk(mMutex);
            if (ctx.ptr != nullptr && ctx.typeId != 0) {
                auto plugin_it = mContextPlaceholders.find(pluginName);
                if (plugin_it != mContextPlaceholders.end()) {
                    auto ph_it = plugin_it->second.find(placeholderName);
                    if (ph_it != plugin_it->second.end()) {
                        for (auto& entry : ph_it->second) {
                            std::vector<Caster> chain;
                            if (entry.targetTypeId == ctx.typeId) {
                                candidates.push_back({
                                    0,
                                    Candidate{{}, entry.fn}
                                });
                            } else if (findUpcastChain(ctx.typeId, entry.targetTypeId, chain)) {
                                candidates.push_back({
                                    (int)chain.size(),
                                    Candidate{std::move(chain), entry.fn}
                                });
                            }
                        }
                    }
                }
            }
        }

        // 排序（最短上行链优先）
        if (!candidates.empty()) {
            std::sort(candidates.begin(), candidates.end(), [](auto& a, auto& b) { return a.first < b.first; });
        }

        // 读取“允许空字符串”参数，用于决定是否接受空值
        bool allowEmpty = false;
        {
            auto ps = parseParams(paramString);
            if (auto it = ps.find("allowempty"); it != ps.end()) {
                if (auto bv = parseBoolish(it->second)) allowEmpty = *bv;
            }
        }

        std::string replaced_val;
        bool        replaced = false;

        // 尝试上下文候选
        for (auto& [dist, cand] : candidates) {
            void* p = ctx.ptr;
            for (auto& cfun : cand.chain) {
                p = cfun(p);
                if (!p) break;
            }
            if (!p) continue;

            if (std::holds_alternative<AnyPtrReplacer>(cand.fn)) {
                auto& fn     = std::get<AnyPtrReplacer>(cand.fn);
                replaced_val = fn(p);
            } else {
                auto& fn     = std::get<AnyPtrReplacerWithParams>(cand.fn);
                replaced_val = fn(p, paramString);
            }
            if (!replaced_val.empty() || allowEmpty) {
                replaced = true;
                break;
            }
        }

        // 若未成功，尝试服务器占位符
        if (!replaced) {
            std::variant<ServerReplacer, ServerReplacerWithParams> serverFn;
            bool                                                   hasServer = false;
            {
                std::shared_lock lk(mMutex);
                auto             plugin_it = mServerPlaceholders.find(pluginName);
                if (plugin_it != mServerPlaceholders.end()) {
                    auto placeholder_it = plugin_it->second.find(placeholderName);
                    if (placeholder_it != plugin_it->second.end()) {
                        serverFn  = placeholder_it->second.fn;
                        hasServer = true;
                    }
                }
            }
            if (hasServer) {
                if (std::holds_alternative<ServerReplacer>(serverFn)) {
                    replaced_val = std::get<ServerReplacer>(serverFn)();
                } else {
                    replaced_val = std::get<ServerReplacerWithParams>(serverFn)(paramString);
                }
                if (!replaced_val.empty() || allowEmpty) {
                    replaced = true;
                }
            }
        }

        std::string finalOut;
        if (replaced) {
            // 格式化
            if (!paramString.empty()) {
                finalOut = applyFormatting(replaced_val, paramString);
            } else {
                finalOut = replaced_val;
            }
        } else {
            // 保留原样
            finalOut = "{" + inside + "}";
        }

        // 默认值（仅当最终为空时）
        if (finalOut.empty() && !defaultText.empty()) {
            finalOut = defaultText;
        }

        // 缓存
        st.cache.emplace(std::move(cacheKey), finalOut);

        result.append(finalOut);

        i = j + 1;
    }

    return result;
}

} // namespace PA
