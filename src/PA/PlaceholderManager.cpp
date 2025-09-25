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
#include <queue>
#include <sstream>
#include <string>
#include <unordered_map>

namespace PA {

// 工具函数（匿名命名空间）保持不变（从原版迁移）
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

// 颜色/样式映射，同原版
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
                auto key = toLower(trim(pair));
                if (!key.empty()) params[key] = "";
            }
        }
        if (sep == std::string::npos) break;
        start = sep + 1;
    }
    return params;
}

inline std::string formatNumber(double x, int decimals, bool doRound) {
    if (decimals < 0) return std::to_string(x);
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

inline std::string applyFormatting(const std::string& rawValue, const std::string& paramStr) {
    if (paramStr.empty()) return rawValue;
    auto params = parseParams(paramStr);

    auto maybeBool = parseBoolish(rawValue);
    auto maybeNum  = parseDouble(rawValue);

    std::string out = rawValue;

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
        }
    }

    if (maybeNum.has_value()) {
        double v = *maybeNum;

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
                        out = *matched;
                    }
                }
            }
        }
    } else {
        if (auto it = params.find("map"); it != params.end()) {
            if (auto mapped = evalMap(out, it->second)) out = *mapped;
        }
    }

    if (auto it = params.find("color"); it != params.end()) {
        std::string codes = styleSpecToCodes(it->second);
        if (!codes.empty()) out = codes + out;
    }

    if (auto it = params.find("prefix"); it != params.end()) {
        out = it->second + out;
    }
    if (auto it = params.find("suffix"); it != params.end()) {
        out += it->second;
    }

    if (out.empty()) {
        if (auto it = params.find("emptytext"); it != params.end()) out = it->second;
    }

    return out;
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
    auto it = mTypeKeyToId.find(typeKeyStr);
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
    auto d             = ensureTypeId(derivedKey);
    auto b             = ensureTypeId(baseKey);
    mUpcastEdges[d][b] = caster; // 派生 -> 基类 的上行边
}

// BFS 查询 from -> to 的“最短上行路径”
bool PlaceholderManager::findUpcastChain(std::size_t fromTypeId, std::size_t toTypeId, std::vector<Caster>& outChain)
    const {
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
    mServerPlaceholders[pluginName][placeholder] = std::move(replacer);
}

void PlaceholderManager::registerPlaceholderForTypeId(
    const std::string& pluginName,
    const std::string& placeholder,
    std::size_t        targetTypeId,
    AnyPtrReplacer     replacer
) {
    mContextPlaceholders[pluginName][placeholder].push_back(TypedReplacer{
        targetTypeId,
        std::move(replacer),
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

// ==== 注销 ====

void PlaceholderManager::unregisterPlaceholders(const std::string& pluginName) {
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

// ==== 替换 ====

std::string PlaceholderManager::replacePlaceholders(const std::string& text) {
    return replacePlaceholders(text, std::any{});
}

std::string PlaceholderManager::replacePlaceholders(const std::string& text, std::any contextObject) {
    // 尽量兼容旧用法
    if (!contextObject.has_value()) {
        return replacePlaceholders(text, PlaceholderContext{nullptr, 0});
    }

    // 1) 若 any 里就是 PlaceholderContext
    if (contextObject.type() == typeid(PlaceholderContext)) {
        auto ctx = std::any_cast<PlaceholderContext>(contextObject);
        return replacePlaceholders(text, ctx);
    }

    // 2) 若 any 里是内置常见类型指针（示例：Player*），自动转为 Context
    //    注意：这只是有限的兼容，其他自定义类型请改用 makeContextRaw/makeContext。
    try {
        if (contextObject.type() == typeid(Player*)) {
            auto p = std::any_cast<Player*>(contextObject);
            return replacePlaceholders(text, makeContext(p));
        }
    } catch (...) {}

    // 3) 退化为“历史行为”：无法多态，仅尝试精确匹配（会走不到 Context 版本的派发）
    //    建议迁移调用方到新的 Context/模板重载。
    // 为了保持兼容，这里直接把上下文忽略，后续只会尝试服务器占位符。
    return replacePlaceholders(text, PlaceholderContext{nullptr, 0});
}

std::string PlaceholderManager::replacePlaceholders(const std::string& text, const PlaceholderContext& ctx) {
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

            // 支持参数：name|k=v;...
            std::string placeholderName = placeholderPart;
            std::string paramString;
            if (auto pipe = placeholderPart.find('|'); pipe != std::string::npos) {
                placeholderName = placeholderPart.substr(0, pipe);
                paramString     = placeholderPart.substr(pipe + 1);
            }

            // 1. 多态上下文占位符
            if (ctx.ptr != nullptr && ctx.typeId != 0) {
                auto plugin_it = mContextPlaceholders.find(pluginName);
                if (plugin_it != mContextPlaceholders.end()) {
                    auto ph_it = plugin_it->second.find(placeholderName);
                    if (ph_it != plugin_it->second.end()) {
                        // 将所有“可达的目标类型”候选收集出来，并按“转换步数”从小到大尝试
                        struct Candidate {
                            std::vector<Caster> chain;
                            AnyPtrReplacer      fn;
                        };
                        std::vector<std::pair<int, Candidate>> candidates;

                        for (auto& entry : ph_it->second) {
                            std::vector<Caster> chain;
                            if (entry.targetTypeId == ctx.typeId) {
                                candidates.push_back({
                                    0,
                                    Candidate{std::vector<Caster>{}, entry.fn}
                                });
                            } else if (findUpcastChain(ctx.typeId, entry.targetTypeId, chain)) {
                                candidates.push_back({
                                    (int)chain.size(),
                                    Candidate{std::move(chain), entry.fn}
                                });
                            }
                        }

                        if (!candidates.empty()) {
                            std::sort(candidates.begin(), candidates.end(), [](auto& a, auto& b) {
                                return a.first < b.first;
                            });

                            for (auto& [dist, cand] : candidates) {
                                void* p = ctx.ptr;
                                for (auto& c : cand.chain) {
                                    p = c(p); // 逐步上行，确保 p 指向目标类型子对象
                                    if (!p) break;
                                }
                                if (!p) continue;

                                std::string replaced_val = cand.fn(p);
                                if (!replaced_val.empty()) {
                                    if (!paramString.empty()) {
                                        replaced_val = applyFormatting(replaced_val, paramString);
                                    }
                                    result.append(replaced_val);
                                    replaced = true;
                                    break;
                                }
                            }
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

// 便利重载：Player*
std::string PlaceholderManager::replacePlaceholders(const std::string& text, Player* player) {
    return replacePlaceholders(text, makeContext(player));
}

} // namespace PA