#include "PlaceholderManager.h"
#include "Utils.h"

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
        auto colonPosOpt = PA::Utils::findSepOutside(inside, ":");
        if (!colonPosOpt) {
            // 非法：按原样输出
            result.append("{").append(inside).append("}");
            i = j + 1;
            continue;
        }
        size_t      colonPos   = *colonPosOpt;
        std::string pluginName = PA::Utils::trim(inside.substr(0, colonPos));
        std::string rest       = inside.substr(colonPos + 1);

        // 找 default 与 | 的位置（均是深度 0）
        auto defaultPosOpt = PA::Utils::findSepOutside(rest, ":-");
        auto pipePosOpt    = PA::Utils::findSepOutside(rest, "|");

        size_t      nameEnd         = std::min(defaultPosOpt.value_or(rest.size()), pipePosOpt.value_or(rest.size()));
        std::string placeholderName = PA::Utils::trim(rest.substr(0, nameEnd));

        std::string defaultText;
        if (defaultPosOpt) {
            size_t defaultStart = *defaultPosOpt + 2;
            size_t defaultEnd   = pipePosOpt ? *pipePosOpt : rest.size();
            if (defaultStart < defaultEnd) defaultText = PA::Utils::trim(rest.substr(defaultStart, defaultEnd - defaultStart));
        }

        std::string paramString;
        if (pipePosOpt) {
            size_t paramStart = *pipePosOpt + 1;
            if (paramStart < rest.size()) paramString = PA::Utils::trim(rest.substr(paramStart));
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
            auto ps = PA::Utils::parseParams(paramString);
            if (auto it = ps.find("allowempty"); it != ps.end()) {
                if (auto bv = PA::Utils::parseBoolish(it->second)) allowEmpty = *bv;
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
                finalOut = PA::Utils::applyFormatting(replaced_val, paramString);
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
