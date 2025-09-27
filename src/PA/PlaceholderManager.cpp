#include "PlaceholderManager.h"
#include "ThreadPool.h"
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
#include <future>

#include "PA/Config/ConfigManager.h" // 引入 ConfigManager
#include "PA/logger.h"               // 引入 logger

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

PlaceholderManager::PlaceholderManager() {
    unsigned int concurrency = std::thread::hardware_concurrency();
    if (concurrency == 0) {
        concurrency = 2; // 硬件并发未知时的默认值
    }
    mThreadPool = std::make_unique<ThreadPool>(concurrency);
}

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


// --- 新：异步占位符注册 ---

void PlaceholderManager::registerAsyncServerPlaceholder(
    const std::string&    pluginName,
    const std::string&    placeholder,
    AsyncServerReplacer&& replacer
) {
    std::unique_lock lk(mMutex);
    mAsyncServerPlaceholders[pluginName][placeholder] = AsyncServerReplacerEntry{std::move(replacer)};
}

void PlaceholderManager::registerAsyncServerPlaceholderWithParams(
    const std::string&           pluginName,
    const std::string&           placeholder,
    AsyncServerReplacerWithParams&& replacer
) {
    std::unique_lock lk(mMutex);
    mAsyncServerPlaceholders[pluginName][placeholder] = AsyncServerReplacerEntry{std::move(replacer)};
}

void PlaceholderManager::registerAsyncPlaceholderForTypeId(
    const std::string&    pluginName,
    const std::string&    placeholder,
    std::size_t           targetTypeId,
    AsyncAnyPtrReplacer&& replacer
) {
    std::unique_lock lk(mMutex);
    mAsyncContextPlaceholders[pluginName][placeholder].push_back(
        AsyncTypedReplacer{targetTypeId, std::variant<AsyncAnyPtrReplacer, AsyncAnyPtrReplacerWithParams>(std::move(replacer))}
    );
}

void PlaceholderManager::registerAsyncPlaceholderForTypeId(
    const std::string&           pluginName,
    const std::string&           placeholder,
    std::size_t                  targetTypeId,
    AsyncAnyPtrReplacerWithParams&& replacer
) {
    std::unique_lock lk(mMutex);
    mAsyncContextPlaceholders[pluginName][placeholder].push_back(
        AsyncTypedReplacer{targetTypeId, std::variant<AsyncAnyPtrReplacer, AsyncAnyPtrReplacerWithParams>(std::move(replacer))}
    );
}

void PlaceholderManager::registerAsyncPlaceholderForTypeKey(
    const std::string&    pluginName,
    const std::string&    placeholder,
    const std::string&    typeKeyStr,
    AsyncAnyPtrReplacer&& replacer
) {
    auto id = ensureTypeId(typeKeyStr);
    registerAsyncPlaceholderForTypeId(pluginName, placeholder, id, std::move(replacer));
}

void PlaceholderManager::registerAsyncPlaceholderForTypeKeyWithParams(
    const std::string&           pluginName,
    const std::string&           placeholder,
    const std::string&           typeKeyStr,
    AsyncAnyPtrReplacerWithParams&& replacer
) {
    auto id = ensureTypeId(typeKeyStr);
    registerAsyncPlaceholderForTypeId(pluginName, placeholder, id, std::move(replacer));
}

// ==== 注销 ====

void PlaceholderManager::unregisterPlaceholders(const std::string& pluginName) {
    std::unique_lock lk(mMutex);
    mServerPlaceholders.erase(pluginName);
    mContextPlaceholders.erase(pluginName);
    mUpcastCache.clear(); // 插件注销可能影响类型，为安全起见清空
}

void PlaceholderManager::unregisterAsyncPlaceholders(const std::string& pluginName) {
    std::unique_lock lk(mMutex);
    mAsyncServerPlaceholders.erase(pluginName);
    mAsyncContextPlaceholders.erase(pluginName);
}

PlaceholderManager::AllPlaceholders PlaceholderManager::getAllPlaceholders() const {
    AllPlaceholders  result;
    std::shared_lock lk(mMutex);

    for (const auto& [pluginName, placeholders] : mServerPlaceholders) {
        for (const auto& [placeholderName, entry] : placeholders) {
            result.serverPlaceholders[pluginName].push_back(placeholderName);
        }
    }

    for (const auto& [pluginName, placeholders] : mContextPlaceholders) {
        for (const auto& [placeholderName, entries] : placeholders) {
            result.contextPlaceholders[pluginName].push_back(placeholderName);
        }
    }

    return result;
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

// --- 新：异步替换 ---

std::future<std::string>
PlaceholderManager::replacePlaceholdersAsync(const std::string& text, const PlaceholderContext& ctx) {
    auto tpl = compileTemplate(text);
    return replacePlaceholdersAsync(tpl, ctx);
}

std::future<std::string>
PlaceholderManager::replacePlaceholdersAsync(const CompiledTemplate& tpl, const PlaceholderContext& ctx) {
    // 使用线程池在后台执行整个替换过程
    return mThreadPool->enqueue([this, &tpl, &ctx]() -> std::string {
        ReplaceState                                st;
        std::vector<std::future<std::string>>       futures;
        std::function<void(const CompiledTemplate&)> process;

        // 递归函数，用于处理模板和其嵌套的模板
        process = [&](const CompiledTemplate& currentTpl) {
            if (st.depth > mMaxRecursionDepth) {
                return;
            }
            for (const auto& token : currentTpl.tokens) {
                if (auto* literal = std::get_if<LiteralToken>(&token)) {
                    // 对于字面量，创建一个已就绪的 future
                    std::promise<std::string> p;
                    p.set_value(std::string(literal->text));
                    futures.push_back(p.get_future());
                } else if (auto* placeholder = std::get_if<PlaceholderToken>(&token)) {
                    // 对于占位符，我们需要异步地解析其参数和默认值，然后执行它
                    auto placeholderFuture =
                        mThreadPool->enqueue([this, placeholder, &ctx, &st, &process]() -> std::string {
                            st.depth++;

                            // 异步解析参数
                            std::string paramString;
                            if (placeholder->paramsTemplate) {
                                auto fut = replacePlaceholdersAsync(*placeholder->paramsTemplate, ctx);
                                paramString = fut.get();
                            }

                            // 异步解析默认值
                            std::string defaultText;
                            if (placeholder->defaultTemplate) {
                                auto fut = replacePlaceholdersAsync(*placeholder->defaultTemplate, ctx);
                                defaultText = fut.get();
                            }

                            st.depth--;

                            // 执行占位符并等待结果
                            auto resultFut = executePlaceholderAsync(
                                placeholder->pluginName,
                                placeholder->placeholderName,
                                paramString,
                                defaultText,
                                ctx,
                                st
                            );
                            return resultFut.get();
                        });
                    futures.push_back(std::move(placeholderFuture));
                }
            }
        };

        process(tpl);

        // 等待所有 future 完成并拼接结果
        std::string result;
        result.reserve(tpl.source.size());
        for (auto& f : futures) {
            result.append(f.get());
        }
        return result;
    });
}

// [新] 编译模板
CompiledTemplate PlaceholderManager::compileTemplate(const std::string& text) {
    CompiledTemplate tpl;
    tpl.source       = text; // Keep the source string alive
    std::string_view s = tpl.source;
    size_t           n = s.size();

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
            tpl.tokens.emplace_back(LiteralToken{s.substr(i, literalEnd - i)});
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
            tpl.tokens.emplace_back(LiteralToken{s.substr(i, literalEnd - i)});
            i = literalEnd;
            continue;
        }

        // 寻找占位符的开始 '{'
        size_t placeholderStart = s.find('{', i);
        if (placeholderStart == std::string::npos) {
            // 剩余全是文本
            if (i < n) {
                tpl.tokens.emplace_back(LiteralToken{s.substr(i)});
            }
            break;
        }

        // i 到 placeholderStart 之间是文本
        if (placeholderStart > i) {
            tpl.tokens.emplace_back(LiteralToken{s.substr(i, placeholderStart - i)});
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
            tpl.tokens.emplace_back(LiteralToken{s.substr(placeholderStart)});
            break;
        }

        // 提取占位符内部
        std::string_view inside = s.substr(placeholderStart + 1, j - (placeholderStart + 1));

        // 解析
        auto colonPosOpt = PA::Utils::findSepOutside(inside, ":");
        if (!colonPosOpt) {
            // 非法格式，视为文本
            if (ConfigManager::getInstance().get().debugMode) {
                logger.warn("Placeholder format error: '{}' is not a valid placeholder format. Expected 'plugin:placeholder'.", std::string(inside));
            }
            tpl.tokens.emplace_back(LiteralToken{s.substr(placeholderStart, j - placeholderStart + 1)});
            i = j + 1;
            continue;
        }

        PlaceholderToken token;
        token.pluginName = PA::Utils::trim_sv(inside.substr(0, *colonPosOpt));
        std::string_view rest = inside.substr(*colonPosOpt + 1);

        auto defaultPosOpt = PA::Utils::findSepOutside(rest, ":-");
        auto pipePosOpt    = PA::Utils::findSepOutside(rest, "|");

        size_t nameEnd        = std::min(defaultPosOpt.value_or(rest.size()), pipePosOpt.value_or(rest.size()));
        token.placeholderName = PA::Utils::trim_sv(rest.substr(0, nameEnd));

        if (defaultPosOpt) {
            size_t defaultStart = *defaultPosOpt + 2;
            size_t defaultEnd   = pipePosOpt ? *pipePosOpt : rest.size();
            if (defaultStart < defaultEnd) {
                token.defaultTemplate = std::make_unique<CompiledTemplate>(
                    compileTemplate(std::string(rest.substr(defaultStart, defaultEnd - defaultStart)))
                );
            }
        }

        if (pipePosOpt) {
            size_t paramStart = *pipePosOpt + 1;
            if (paramStart < rest.size()) {
                token.paramsTemplate =
                    std::make_unique<CompiledTemplate>(compileTemplate(std::string(rest.substr(paramStart))));
            }
        }

        tpl.tokens.emplace_back(std::move(token));
        i = j + 1;
    }
    return tpl;
}

// [新] 私有辅助函数：执行单个占位符的查找与替换
std::string PlaceholderManager::executePlaceholder(
    std::string_view        pluginName,
    std::string_view        placeholderName,
    const std::string&      paramString,
    const std::string&      defaultText,
    const PlaceholderContext& ctx,
    ReplaceState&           st
) {
    // 构造缓存 key
    std::string cacheKey;
    cacheKey.reserve(pluginName.size() + placeholderName.size() + paramString.size() + 40);
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
            auto plugin_it = mContextPlaceholders.find(std::string(pluginName));
            if (plugin_it != mContextPlaceholders.end()) {
                auto ph_it = plugin_it->second.find(std::string(placeholderName));
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

    // `executePlaceholder` 现在只处理同步占位符。
    // 异步占位符的同步调用由 `executePlaceholderAsync` 的回退逻辑处理，
    // 然后由 `replacePlaceholders` -> `replacePlaceholdersAsync` -> `future.get()` 阻塞。

    if (!replaced) {
        std::variant<ServerReplacer, ServerReplacerWithParams> serverFn;
        bool                                                   hasServer = false;
        {
            std::shared_lock lk(mMutex);
            auto             plugin_it = mServerPlaceholders.find(std::string(pluginName));
            if (plugin_it != mServerPlaceholders.end()) {
                auto placeholder_it = plugin_it->second.find(std::string(placeholderName));
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
        if (ConfigManager::getInstance().get().debugMode) {
            logger.warn("Placeholder '{}:{}' not found or returned empty. Context ptr: {}, typeId: {}. Params: '{}'.",
                        std::string(pluginName),
                        std::string(placeholderName),
                        reinterpret_cast<uintptr_t>(ctx.ptr),
                        ctx.typeId,
                        paramString);
        }
        // 保留原样
        finalOut.reserve(pluginName.size() + placeholderName.size() + defaultText.size() + paramString.size() + 5);
        finalOut.append("{").append(pluginName).append(":").append(placeholderName);
        if (!defaultText.empty()) finalOut.append(":-").append(defaultText);
        if (!paramString.empty()) finalOut.append("|").append(paramString);
        finalOut.append("}");
    }

    if (finalOut.empty() && !defaultText.empty()) {
        finalOut = defaultText;
    }

    st.cache.emplace(std::move(cacheKey), finalOut);
    return finalOut;
}

// [新] 私有辅助函数：执行单个占位符的异步查找与替换
std::future<std::string> PlaceholderManager::executePlaceholderAsync(
    std::string_view        pluginName,
    std::string_view        placeholderName,
    const std::string&      paramString,
    const std::string&      defaultText,
    const PlaceholderContext& ctx,
    ReplaceState&           st
) {
    // 1. 检查异步上下文占位符
    {
        std::shared_lock lk(mMutex);
        auto             plugin_it = mAsyncContextPlaceholders.find(std::string(pluginName));
        if (plugin_it != mAsyncContextPlaceholders.end()) {
            auto ph_it = plugin_it->second.find(std::string(placeholderName));
            if (ph_it != plugin_it->second.end()) {
                // (简化) 我们只处理第一个匹配的候选，不处理继承链以简化实现
                for (const auto& entry : ph_it->second) {
                    if (entry.targetTypeId == ctx.typeId) {
                        PA::Utils::ParsedParams params(paramString);
                        if (std::holds_alternative<AsyncAnyPtrReplacer>(entry.fn)) {
                            return std::get<AsyncAnyPtrReplacer>(entry.fn)(ctx.ptr);
                        } else {
                            return std::get<AsyncAnyPtrReplacerWithParams>(entry.fn)(ctx.ptr, params);
                        }
                    }
                }
            }
        }
    }

    // 2. 检查异步服务器占位符
    {
        std::shared_lock lk(mMutex);
        auto             plugin_it = mAsyncServerPlaceholders.find(std::string(pluginName));
        if (plugin_it != mAsyncServerPlaceholders.end()) {
            auto placeholder_it = plugin_it->second.find(std::string(placeholderName));
            if (placeholder_it != plugin_it->second.end()) {
                PA::Utils::ParsedParams params(paramString);
                const auto&             fn = placeholder_it->second.fn;
                if (std::holds_alternative<AsyncServerReplacer>(fn)) {
                    return std::get<AsyncServerReplacer>(fn)();
                } else {
                    return std::get<AsyncServerReplacerWithParams>(fn)(params);
                }
            }
        }
    }

    // 3. 如果没有找到异步占位符，则回退到同步执行，并将其结果包装在 future 中
    std::promise<std::string> promise;
    promise.set_value(executePlaceholder(pluginName, placeholderName, paramString, defaultText, ctx, st));
    return promise.get_future();
}

// [新] 使用编译模板进行替换
std::string
PlaceholderManager::replacePlaceholders(const CompiledTemplate& tpl, const PlaceholderContext& ctx) {
    // 同步调用现在可以利用异步接口并阻塞等待结果
    auto future = replacePlaceholdersAsync(tpl, ctx);
    return future.get();
}


// 内部实现：带嵌套/转义/默认值/参数
std::string
PlaceholderManager::replacePlaceholdersImpl(const std::string& text, const PlaceholderContext& ctx, ReplaceState& st) {
    // 旧的实现方式，现在通过编译和同步执行新模板系统来完成
    if (st.depth > mMaxRecursionDepth) {
        return text;
    }
    auto tpl = compileTemplate(text);
    return replacePlaceholders(tpl, ctx);
}

} // namespace PA
