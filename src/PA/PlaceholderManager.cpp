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

// --- 新：模板编译系统实现 ---

// 为支持 unique_ptr 的移动语义
CompiledTemplate::CompiledTemplate()                        = default;
CompiledTemplate::~CompiledTemplate()                       = default;
CompiledTemplate::CompiledTemplate(CompiledTemplate&&) noexcept = default;
CompiledTemplate& CompiledTemplate::operator=(CompiledTemplate&&) noexcept = default;


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
    mUpcastCache.clear(); // 继承关系变更，清空缓存
}

// BFS 查询 from -> to 的“最短上行路径”
bool PlaceholderManager::findUpcastChain(std::size_t fromTypeId, std::size_t toTypeId, std::vector<Caster>& outChain)
    const {
    outChain.clear();
    if (fromTypeId == 0 || toTypeId == 0) return false;
    if (fromTypeId == toTypeId) return true; // 空链表示已是同型

    uint64_t cacheKey = (static_cast<uint64_t>(fromTypeId) << 32) | toTypeId;

    // 1. 检查缓存
    {
        std::shared_lock lk(mMutex);
        auto             it = mUpcastCache.find(cacheKey);
        if (it != mUpcastCache.end()) {
            outChain = it->second.chain;
            return it->second.success;
        }
    }

    // 2. 缓存未命中，执行 BFS
    std::vector<Caster> resultChain;
    bool                success = false;
    {
        std::shared_lock lk(mMutex);
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
                    std::size_t x = nxt;
                    while (x != fromTypeId) {
                        auto [p, c] = prev[x];
                        resultChain.push_back(c);
                        x = p;
                    }
                    std::reverse(resultChain.begin(), resultChain.end());
                    success = true;
                    goto bfs_end; // 找到路径，跳出循环
                }
                q.push(nxt);
            }
        }
    }
bfs_end:

    // 3. 结果写入缓存
    {
        std::unique_lock lk(mMutex);
        mUpcastCache[cacheKey] = {success, resultChain};
    }

    outChain = std::move(resultChain);
    return success;
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
    mUpcastCache.clear(); // 插件注销可能影响类型，为安全起见清空
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

// [新] 编译模板
CompiledTemplate PlaceholderManager::compileTemplate(const std::string& text) {
    CompiledTemplate tpl;
    const char*      s = text.c_str();
    size_t           n = text.size();

    for (size_t i = 0; i < n;) {
        char c = s[i];

        // 转义
        if (mEnableDoubleBraceEscape && c == '{' && i + 1 < n && s[i + 1] == '{') {
            // 找到下一个非 {{ 的位置
            size_t literalEnd = i + 2;
            while (literalEnd < n) {
                if (s[literalEnd] == '{' && literalEnd + 1 < n && s[literalEnd + 1] == '{') {
                    literalEnd += 2;
                } else {
                    break;
                }
            }
            tpl.tokens.emplace_back(LiteralToken{text.substr(i, literalEnd - i)});
            i = literalEnd;
            continue;
        }
        if (mEnableDoubleBraceEscape && c == '}' && i + 1 < n && s[i + 1] == '}') {
            size_t literalEnd = i + 2;
            while (literalEnd < n) {
                if (s[literalEnd] == '}' && literalEnd + 1 < n && s[literalEnd + 1] == '}') {
                    literalEnd += 2;
                } else {
                    break;
                }
            }
            tpl.tokens.emplace_back(LiteralToken{text.substr(i, literalEnd - i)});
            i = literalEnd;
            continue;
        }

        // 寻找占位符的开始 '{'
        size_t placeholderStart = text.find('{', i);
        if (placeholderStart == std::string::npos) {
            // 剩余全是文本
            if (i < n) {
                tpl.tokens.emplace_back(LiteralToken{text.substr(i)});
            }
            break;
        }

        // i 到 placeholderStart 之间是文本
        if (placeholderStart > i) {
            tpl.tokens.emplace_back(LiteralToken{text.substr(i, placeholderStart - i)});
        }

        // 寻找匹配的 '}'
        size_t j       = placeholderStart + 1;
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
            } else if (s[j] == '}') {
                --depth;
                if (depth == 0) {
                    matched = true;
                    break;
                }
            }
            ++j;
        }

        if (!matched) {
            // 无匹配，后面全是文本
            tpl.tokens.emplace_back(LiteralToken{text.substr(placeholderStart)});
            break;
        }

        // 提取占位符内部
        std::string inside(text.substr(placeholderStart + 1, j - (placeholderStart + 1)));

        // 解析
        auto colonPosOpt = PA::Utils::findSepOutside(inside, ":");
        if (!colonPosOpt) {
            // 非法格式，视为文本
            tpl.tokens.emplace_back(LiteralToken{"{" + inside + "}"});
            i = j + 1;
            continue;
        }

        PlaceholderToken token;
        token.pluginName = PA::Utils::trim(inside.substr(0, *colonPosOpt));
        std::string rest = inside.substr(*colonPosOpt + 1);

        auto defaultPosOpt = PA::Utils::findSepOutside(rest, ":-");
        auto pipePosOpt    = PA::Utils::findSepOutside(rest, "|");

        size_t nameEnd         = std::min(defaultPosOpt.value_or(rest.size()), pipePosOpt.value_or(rest.size()));
        token.placeholderName = PA::Utils::trim(rest.substr(0, nameEnd));

        if (defaultPosOpt) {
            size_t defaultStart = *defaultPosOpt + 2;
            size_t defaultEnd   = pipePosOpt ? *pipePosOpt : rest.size();
            if (defaultStart < defaultEnd) {
                token.defaultTemplate =
                    std::make_unique<CompiledTemplate>(compileTemplate(rest.substr(defaultStart, defaultEnd - defaultStart)));
            }
        }

        if (pipePosOpt) {
            size_t paramStart = *pipePosOpt + 1;
            if (paramStart < rest.size()) {
                token.paramsTemplate = std::make_unique<CompiledTemplate>(compileTemplate(rest.substr(paramStart)));
            }
        }

        tpl.tokens.emplace_back(std::move(token));
        i = j + 1;
    }
    return tpl;
}

// [新] 私有辅助函数：执行单个占位符的查找与替换
std::string PlaceholderManager::executePlaceholder(
    const std::string&      pluginName,
    const std::string&      placeholderName,
    const std::string&      paramString,
    const std::string&      defaultText,
    const PlaceholderContext& ctx,
    ReplaceState&           st
) {
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
        return out;
    }

    // 查询候选
    struct Candidate {
        std::vector<Caster>                                    chain;
        std::variant<AnyPtrReplacer, AnyPtrReplacerWithParams> fn;
    };
    std::vector<std::pair<int, Candidate>> candidates;

    std::vector<TypedReplacer> potentialReplacers;
    {
        std::shared_lock lk(mMutex);
        if (ctx.ptr != nullptr && ctx.typeId != 0) {
            auto plugin_it = mContextPlaceholders.find(pluginName);
            if (plugin_it != mContextPlaceholders.end()) {
                auto ph_it = plugin_it->second.find(placeholderName);
                if (ph_it != plugin_it->second.end()) {
                    potentialReplacers = ph_it->second;
                }
            }
        }
    }

    for (auto& entry : potentialReplacers) {
        std::vector<Caster> chain;
        if (entry.targetTypeId == ctx.typeId) {
            candidates.push_back({0, Candidate{{}, entry.fn}});
        } else if (findUpcastChain(ctx.typeId, entry.targetTypeId, chain)) {
            candidates.push_back({(int)chain.size(), Candidate{std::move(chain), entry.fn}});
        }
    }

    if (!candidates.empty()) {
        std::sort(candidates.begin(), candidates.end(), [](auto& a, auto& b) { return a.first < b.first; });
    }

    PA::Utils::ParsedParams params(paramString);
    bool                    allowEmpty = params.getBool("allowempty").value_or(false);

    std::string replaced_val;
    bool        replaced = false;

    for (auto& [dist, cand] : candidates) {
        void* p = ctx.ptr;
        for (auto& cfun : cand.chain) {
            p = cfun(p);
            if (!p) break;
        }
        if (!p) continue;

        if (std::holds_alternative<AnyPtrReplacer>(cand.fn)) {
            replaced_val = std::get<AnyPtrReplacer>(cand.fn)(p);
        } else {
            replaced_val = std::get<AnyPtrReplacerWithParams>(cand.fn)(p, params);
        }
        if (!replaced_val.empty() || allowEmpty) {
            replaced = true;
            break;
        }
    }

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
                replaced_val = std::get<ServerReplacerWithParams>(serverFn)(params);
            }
            if (!replaced_val.empty() || allowEmpty) {
                replaced = true;
            }
        }
    }

    std::string finalOut;
    if (replaced) {
        finalOut = PA::Utils::applyFormatting(replaced_val, params);
    } else {
        // 保留原样
        finalOut = "{" + pluginName + ":" + placeholderName;
        if (!defaultText.empty()) finalOut += ":-" + defaultText;
        if (!paramString.empty()) finalOut += "|" + paramString;
        finalOut += "}";
    }

    if (finalOut.empty() && !defaultText.empty()) {
        finalOut = defaultText;
    }

    st.cache.emplace(std::move(cacheKey), finalOut);
    return finalOut;
}

// [新] 使用编译模板进行替换
std::string
PlaceholderManager::replacePlaceholders(const CompiledTemplate& tpl, const PlaceholderContext& ctx) {
    ReplaceState st; // 每个顶层调用都有自己的状态
    std::string  result;

    // 预估大小
    size_t reserveSize = 0;
    for (const auto& token : tpl.tokens) {
        if (auto* literal = std::get_if<LiteralToken>(&token)) {
            reserveSize += literal->text.size();
        } else {
            reserveSize += 32; // 估算占位符长度
        }
    }
    result.reserve(reserveSize);

    std::function<void(const CompiledTemplate&, ReplaceState&)> process =
        [&](const CompiledTemplate& currentTpl, ReplaceState& currentState) {
            if (currentState.depth > mMaxRecursionDepth) {
                return;
            }
            for (const auto& token : currentTpl.tokens) {
                if (auto* literal = std::get_if<LiteralToken>(&token)) {
                    result.append(literal->text);
                } else if (auto* placeholder = std::get_if<PlaceholderToken>(&token)) {
                    // 渲染嵌套模板
                    std::string defaultText;
                    if (placeholder->defaultTemplate) {
                        currentState.depth++;
                        // 嵌套渲染需要独立的 result string
                        std::string tempResult = result;
                        result.clear();
                        process(*placeholder->defaultTemplate, currentState);
                        defaultText = result;
                        result      = tempResult; // 恢复
                        currentState.depth--;
                    }
                    std::string paramString;
                    if (placeholder->paramsTemplate) {
                        currentState.depth++;
                        std::string tempResult = result;
                        result.clear();
                        process(*placeholder->paramsTemplate, currentState);
                        paramString = result;
                        result      = tempResult; // 恢复
                        currentState.depth--;
                    }

                    // 执行替换
                    result.append(executePlaceholder(
                        placeholder->pluginName,
                        placeholder->placeholderName,
                        paramString,
                        defaultText,
                        ctx,
                        currentState
                    ));
                }
            }
        };

    process(tpl, st);
    return result;
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

        // 提取占位符内部并编译执行
        // 注意：这里为了兼容旧接口，每次都即时编译和执行，性能较低
        // 新的流程应优先使用 compileTemplate + replacePlaceholders(tpl)
        auto tpl = compileTemplate(text.substr(i, j - i + 1));
        result.append(replacePlaceholders(tpl, ctx));

        i = j + 1;
    }

    return result;
}

} // namespace PA
