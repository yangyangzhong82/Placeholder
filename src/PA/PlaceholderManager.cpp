#include "PlaceholderManager.h"
#include "ll/api/service/Bedrock.h"
#include "mc/network/ServerNetworkHandler.h"
#include "mc/server/ServerPlayer.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/Level.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "PlaceholderManager.h"
#include "ll/api/service/Bedrock.h"
#include "mc/network/ServerNetworkHandler.h"
#include "mc/server/ServerPlayer.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/Level.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>


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

inline std::optional<bool> parseBoolish(const std::string& s) {
    auto v = toLower(trim(s));
    if (v == "true" || v == "yes" || v == "y" || v == "1") return true;
    if (v == "false" || v == "no" || v == "n" || v == "0") return false;
    return std::nullopt;
}

inline std::optional<int> parseInt(const std::string& s) {
    try {
        size_t idx = 0;
        int    v   = std::stoi(trim(s), &idx);
        if (idx == trim(s).size()) return v;
    } catch (...) {}
    return std::nullopt;
}

inline std::optional<double> parseDouble(const std::string& s) {
    try {
        size_t idx = 0;
        double v   = std::stod(trim(s), &idx);
        if (idx == trim(s).size() || (idx > 0 && trim(s).substr(idx) == "")) return v;
    } catch (...) {}
    return std::nullopt;
}

// 颜色名/样式 -> §码 映射
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
        // 样式
        {"bold",          "§l"},
        {"italic",        "§o"},
        {"underline",     "§n"},
        {"strikethrough", "§m"},
        {"obfuscated",    "§k"},
    };
    return m;
}

// 将 "red+bold" / "§c+bold" / "§c" 解析为前缀码串
inline std::string styleSpecToCodes(const std::string& spec) {
    auto s = trim(spec);
    if (s.empty()) return "";

    // 直接是 §码 且不含 '+' 的情况
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

// params: 仅以分号 ';' 分隔顶层 key=value，避免与 thresholds 内部逗号冲突
inline std::unordered_map<std::string, std::string> parseParams(const std::string& paramStr) {
    std::unordered_map<std::string, std::string> params;
    std::string                                  s     = paramStr;
    size_t                                       start = 0;
    while (start < s.size()) {
        size_t      sep  = s.find(';', start);
        std::string pair = sep == std::string::npos ? s.substr(start) : s.substr(start, sep - start);
        if (!pair.empty()) {
            auto eq = pair.find('=');
            if (eq != std::string::npos) {
                auto key = toLower(trim(pair.substr(0, eq)));
                auto val = trim(pair.substr(eq + 1));
                if (!key.empty()) params[key] = val;
            } else {
                // 只有key无值的情况，视为空字符串
                auto key = toLower(trim(pair));
                if (!key.empty()) params[key] = "";
            }
        }
        if (sep == std::string::npos) break;
        start = sep + 1;
    }
    return params;
}

// 数值格式化
inline std::string formatNumber(double x, int decimals, bool doRound) {
    if (decimals < 0) return std::to_string(x);
    double             factor = std::pow(10.0, (double)decimals);
    double             y      = doRound ? std::round(x * factor) / factor : std::trunc(x * factor) / factor;
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(decimals) << y;
    return oss.str();
}

// 解析 thresholds 条件
inline bool matchCond(double v, const std::string& condRaw) {
    auto cond = trim(condRaw);
    if (cond.empty()) return false;

    // 区间 a-b
    auto dash = cond.find('-');
    if (dash != std::string::npos) {
        auto a = parseDouble(cond.substr(0, dash));
        auto b = parseDouble(cond.substr(dash + 1));
        if (a && b) return v >= *a && v <= *b; // 闭区间
    }

    // 比较符
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

    // 仅数字，等于
    if (auto num = parseDouble(cond)) {
        return v == *num;
    }

    return false;
}

// thresholds 求值：返回第一个匹配项的“输出”字符串；支持 default
inline std::optional<std::string> evalThresholds(double v, const std::string& spec) {
    std::string       defaultVal;
    std::stringstream ss(spec);
    std::string       item;

    while (std::getline(ss, item, ',')) {
        auto part = trim(item);
        if (part.empty()) continue;
        auto col = part.find(':');
        if (col == std::string::npos) continue;

        auto lhs = trim(part.substr(0, col));  // 条件
        auto rhs = trim(part.substr(col + 1)); // 输出

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

// map=key1:val1,key2:val2,default:valx
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

// 应用参数格式化（核心）
inline std::string applyFormatting(const std::string& rawValue, const std::string& paramStr) {
    if (paramStr.empty()) return rawValue;
    auto params = parseParams(paramStr);

    // 数值/布尔识别
    auto maybeBool = parseBoolish(rawValue);
    auto maybeNum  = parseDouble(rawValue);

    std::string out = rawValue;

    // 1) 布尔映射
    if (maybeBool.has_value()) {
        bool b   = *maybeBool;
        auto itT = params.find("truetext");
        auto itF = params.find("falsetext");
        if (itT != params.end() || itF != params.end()) {
            out = b ? (itT != params.end() ? itT->second : "true") : (itF != params.end() ? itF->second : "false");
        } else if (auto itMap = params.find("map"); itMap != params.end()) {
            // 兼容 map=true:是,false:否
            auto mapped = evalMap(b ? "true" : "false", itMap->second);
            if (!mapped) mapped = evalMap(b ? "1" : "0", itMap->second);
            if (mapped) out = *mapped;
        }
    }

    // 2) 数值格式与阈值
    if (maybeNum.has_value()) {
        double v = *maybeNum;

        // decimals & round
        int  decimals = -1;
        bool doRound  = true;
        if (auto it = params.find("decimals"); it != params.end()) {
            if (auto iv = parseInt(it->second)) decimals = *iv;
        }
        if (auto it = params.find("round"); it != params.end()) {
            if (auto bv = parseBoolish(it->second)) doRound = *bv;
        }
        if (decimals >= 0) {
            out = formatNumber(v, decimals, doRound);
        }

        // thresholds
        if (auto it = params.find("thresholds"); it != params.end()) {
            if (auto matched = evalThresholds(v, it->second)) {
                // 如果是颜色/样式，作为着色；否则视为替换文本
                std::string codes = styleSpecToCodes(*matched);
                if (!codes.empty()) {
                    out = codes + out;
                } else {
                    // 替换文本
                    bool showValue = true;
                    if (auto sv = params.find("showvalue"); sv != params.end()) {
                        if (auto bv = parseBoolish(sv->second)) showValue = *bv;
                    }
                    if (!showValue) {
                        out = *matched;
                    } else {
                        // showValue=true 时，仍保留原值（不强制拼接标签，保持简单一致的行为）
                        out = *matched; // 若你希望“保留数值”，可改成 ( *matched + out ) 或其他风格
                    }
                }
            }
        }
    } else {
        // 非数值的 map
        if (auto it = params.find("map"); it != params.end()) {
            if (auto mapped = evalMap(out, it->second)) out = *mapped;
        }
    }

    // 3) 通用着色（若 thresholds 未着色）
    if (auto it = params.find("color"); it != params.end()) {
        std::string codes = styleSpecToCodes(it->second);
        if (!codes.empty()) out = codes + out;
    }

    // 4) 前后缀
    if (auto it = params.find("prefix"); it != params.end()) {
        out = it->second + out;
    }
    if (auto it = params.find("suffix"); it != params.end()) {
        out += it->second;
    }

    // 5) 空文本替代
    if (out.empty()) {
        if (auto it = params.find("emptytext"); it != params.end()) out = it->second;
    }

    return out;
}

} // namespace

// 定义静态实例
PlaceholderManager& PlaceholderManager::getInstance() {
    static PlaceholderManager instance;
    return instance;
}

// 构造函数现在为空，所有注册逻辑已移至 registerBuiltinPlaceholders()
PlaceholderManager::PlaceholderManager() = default;

void PlaceholderManager::registerServerPlaceholder(
    const std::string& pluginName,
    const std::string& placeholder,
    ServerReplacer     replacer
) {
    mServerPlaceholders[pluginName][placeholder] = replacer;
}

void PlaceholderManager::unregisterPlaceholders(const std::string& pluginName) {
    mServerPlaceholders.erase(pluginName);
    mContextPlaceholders.erase(pluginName);
}

// 无上下文对象的版本，调用主函数并传入空的 std::any
std::string PlaceholderManager::replacePlaceholders(const std::string& text) {
    return replacePlaceholders(text, std::any{});
}

// 带有上下文对象的主替换函数（已支持参数语法）
std::string PlaceholderManager::replacePlaceholders(const std::string& text, std::any contextObject) {
    if (text.find('{') == std::string::npos) {
        return text;
    }

    std::string result;
    result.reserve(text.length() * 1.5);

    size_t last_pos = 0;
    size_t find_pos;
    while ((find_pos = text.find('{', last_pos)) != std::string::npos) {
        result.append(text, last_pos, find_pos - last_pos);

        size_t end_pos = text.find('}', find_pos + 1);
        if (end_pos == std::string::npos) {
            last_pos = find_pos;
            break;
        }

        const std::string full_placeholder = text.substr(find_pos, end_pos - find_pos + 1);
        const std::string key              = text.substr(find_pos + 1, end_pos - find_pos - 1);
        size_t            colon_pos        = key.find(':');
        bool              replaced         = false;

        if (colon_pos != std::string::npos) {
            std::string pluginName      = key.substr(0, colon_pos);
            std::string placeholderPart = key.substr(colon_pos + 1);

            // 解析参数段：占位符名后用 '|' 分隔
            std::string placeholderName = placeholderPart;
            std::string paramString;
            if (auto pipe = placeholderPart.find('|'); pipe != std::string::npos) {
                placeholderName = placeholderPart.substr(0, pipe);
                paramString     = placeholderPart.substr(pipe + 1);
            }

            // 1. 优先尝试上下文占位符
            if (contextObject.has_value()) {
                auto plugin_it = mContextPlaceholders.find(pluginName);
                if (plugin_it != mContextPlaceholders.end()) {
                    auto placeholder_it = plugin_it->second.find(placeholderName);
                    if (placeholder_it != plugin_it->second.end()) {
                        std::string replaced_val = placeholder_it->second(contextObject);
                        if (!replaced_val.empty()) {
                            // 应用参数格式化
                            if (!paramString.empty()) {
                                replaced_val = applyFormatting(replaced_val, paramString);
                            }
                            result.append(replaced_val);
                            replaced = true;
                        }
                    }
                }
            }

            // 2. 若未成功，尝试服务器占位符
            if (!replaced) {
                auto plugin_it = mServerPlaceholders.find(pluginName);
                if (plugin_it != mServerPlaceholders.end()) {
                    auto placeholder_it = plugin_it->second.find(placeholderName);
                    if (placeholder_it != plugin_it->second.end()) {
                        std::string replaced_val = placeholder_it->second();
                        if (!paramString.empty()) {
                            replaced_val = applyFormatting(replaced_val, paramString);
                        }
                        result.append(replaced_val);
                        replaced = true;
                    }
                }
            }
        }

        // 未匹配成功，保留原样
        if (!replaced) {
            result.append(full_placeholder);
        }

        last_pos = end_pos + 1;
    }

    if (last_pos < text.length()) {
        result.append(text, last_pos, std::string::npos);
    }
    return result;
}

// Player* 的便利重载版本
std::string PlaceholderManager::replacePlaceholders(const std::string& text, Player* player) {
    return replacePlaceholders(text, std::any{player});
}

} // namespace PA
