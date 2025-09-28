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
#include <future>
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


#include "PA/Config/ConfigManager.h" // 引入 ConfigManager


namespace PA {

// --- 新：模板编译系统实现 ---

// 为支持 unique_ptr 的移动语义
CompiledTemplate::CompiledTemplate()                                       = default;
CompiledTemplate::~CompiledTemplate()                                      = default;
CompiledTemplate::CompiledTemplate(CompiledTemplate&&) noexcept            = default;
CompiledTemplate& CompiledTemplate::operator=(CompiledTemplate&&) noexcept = default;


// 单例
PlaceholderManager& PlaceholderManager::getInstance() {
    static PlaceholderManager instance;
    return instance;
}

PlaceholderManager::PlaceholderManager() : mGlobalCache(ConfigManager::getInstance().get().globalCacheSize) {
    unsigned int hardwareConcurrency = std::thread::hardware_concurrency();
    if (hardwareConcurrency == 0) {
        hardwareConcurrency = 2; // 硬件并发未知时的默认值
    }

    mCombinerThreadPool = std::make_unique<ThreadPool>(1); // 合并线程池只需要一个线程

    int asyncPoolSize = ConfigManager::getInstance().get().asyncThreadPoolSize;
    if (asyncPoolSize <= 0) {
        asyncPoolSize = hardwareConcurrency;
    }
    mAsyncThreadPool = std::make_unique<ThreadPool>(asyncPoolSize);


    // 注册一个回调，当配置重新加载时，更新缓存大小
    ConfigManager::getInstance().onReload([this](const Config& newConfig) {
        mGlobalCache.setCapacity(newConfig.globalCacheSize);
        // 注意：线程池大小在运行时不便更改，因此这里不处理 asyncThreadPoolSize 的热重载
    });
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

void PlaceholderManager::registerTypeAlias(const std::string& alias, const std::string& typeKeyStr) {
    auto id = ensureTypeId(typeKeyStr);
    std::unique_lock lk(mMutex);
    mIdToAlias[id] = alias;
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
        std::shared_lock                                                lk(mMutex);
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
    const std::string&           pluginName,
    const std::string&           placeholder,
    ServerReplacer               replacer,
    std::optional<CacheDuration> cache_duration,
    CacheKeyStrategy             strategy
) {
    std::unique_lock lk(mMutex);
    mServerPlaceholders[pluginName][placeholder] = ServerReplacerEntry{replacer, cache_duration, strategy};
}

void PlaceholderManager::registerServerPlaceholderWithParams(
    const std::string&           pluginName,
    const std::string&           placeholder,
    ServerReplacerWithParams&&   replacer,
    std::optional<CacheDuration> cache_duration,
    CacheKeyStrategy             strategy
) {
    std::unique_lock lk(mMutex);
    mServerPlaceholders[pluginName][placeholder] =
        ServerReplacerEntry{ServerReplacerWithParams(std::move(replacer)), cache_duration, strategy};
}

void PlaceholderManager::registerPlaceholderForTypeId(
    const std::string&           pluginName,
    const std::string&           placeholder,
    std::size_t                  targetTypeId,
    AnyPtrReplacer               replacer,
    std::optional<CacheDuration> cache_duration,
    CacheKeyStrategy             strategy
) {
    std::unique_lock lk(mMutex);
    mContextPlaceholders[pluginName][placeholder].push_back(TypedReplacer{
        targetTypeId,
        std::variant<AnyPtrReplacer, AnyPtrReplacerWithParams>(std::move(replacer)),
        cache_duration,
        strategy,
    });
}

void PlaceholderManager::registerPlaceholderForTypeId(
    const std::string&           pluginName,
    const std::string&           placeholder,
    std::size_t                  targetTypeId,
    AnyPtrReplacerWithParams&&   replacer,
    std::optional<CacheDuration> cache_duration,
    CacheKeyStrategy             strategy
) {
    std::unique_lock lk(mMutex);
    mContextPlaceholders[pluginName][placeholder].push_back(TypedReplacer{
        targetTypeId,
        std::variant<AnyPtrReplacer, AnyPtrReplacerWithParams>(std::move(replacer)),
        cache_duration,
        strategy,
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
    const std::string&           pluginName,
    const std::string&           placeholder,
    AsyncServerReplacer&&        replacer,
    std::optional<CacheDuration> cache_duration,
    CacheKeyStrategy             strategy
) {
    std::unique_lock lk(mMutex);
    mAsyncServerPlaceholders[pluginName][placeholder] =
        AsyncServerReplacerEntry{std::move(replacer), cache_duration, strategy};
}

void PlaceholderManager::registerAsyncServerPlaceholderWithParams(
    const std::string&              pluginName,
    const std::string&              placeholder,
    AsyncServerReplacerWithParams&& replacer,
    std::optional<CacheDuration>    cache_duration,
    CacheKeyStrategy                strategy
) {
    std::unique_lock lk(mMutex);
    mAsyncServerPlaceholders[pluginName][placeholder] =
        AsyncServerReplacerEntry{std::move(replacer), cache_duration, strategy};
}

void PlaceholderManager::registerAsyncPlaceholderForTypeId(
    const std::string&           pluginName,
    const std::string&           placeholder,
    std::size_t                  targetTypeId,
    AsyncAnyPtrReplacer&&        replacer,
    std::optional<CacheDuration> cache_duration,
    CacheKeyStrategy             strategy
) {
    std::unique_lock lk(mMutex);
    mAsyncContextPlaceholders[pluginName][placeholder].push_back(AsyncTypedReplacer{
        targetTypeId,
        std::variant<AsyncAnyPtrReplacer, AsyncAnyPtrReplacerWithParams>(std::move(replacer)),
        cache_duration,
        strategy
    });
}

void PlaceholderManager::registerAsyncPlaceholderForTypeId(
    const std::string&              pluginName,
    const std::string&              placeholder,
    std::size_t                     targetTypeId,
    AsyncAnyPtrReplacerWithParams&& replacer,
    std::optional<CacheDuration>    cache_duration,
    CacheKeyStrategy                strategy
) {
    std::unique_lock lk(mMutex);
    mAsyncContextPlaceholders[pluginName][placeholder].push_back(AsyncTypedReplacer{
        targetTypeId,
        std::variant<AsyncAnyPtrReplacer, AsyncAnyPtrReplacerWithParams>(std::move(replacer)),
        cache_duration,
        strategy
    });
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
    const std::string&              pluginName,
    const std::string&              placeholder,
    const std::string&              typeKeyStr,
    AsyncAnyPtrReplacerWithParams&& replacer
) {
    auto id = ensureTypeId(typeKeyStr);
    registerAsyncPlaceholderForTypeId(pluginName, placeholder, id, std::move(replacer));
}

// ==== 注销 & 缓存清理 ====

void PlaceholderManager::clearCache() { mGlobalCache.clear(); }

void PlaceholderManager::clearCache(const std::string& pluginName) {
    std::string prefix = "#" + pluginName + ":";
    mGlobalCache.remove_if([&prefix](const std::string& key) {
        // 键格式: ctxptr#typeId#plugin:ph|params
        // 或: #plugin:ph|params (ServerOnly)
        auto pos = key.find(prefix);
        // 确保找到的前缀是在 #...# 之后，或者是 key 的开头
        if (pos == std::string::npos) {
            return false;
        }
        auto secondHashPos = key.find('#', key.find('#') + 1);
        return pos == secondHashPos + 1 || (key.rfind('#', pos) == std::string::npos);
    });
}

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
            if (!entries.empty()) {
                // 假设同一个占位符的所有重载都指向相同的逻辑类型，因此我们只取第一个。
                auto        targetId = entries.front().targetTypeId;
                std::string typeName;

                // 优先使用别名
                auto aliasIt = mIdToAlias.find(targetId);
                if (aliasIt != mIdToAlias.end()) {
                    typeName = aliasIt->second;
                } else {
                    // 否则回退到内部类型键
                    auto keyIt = mIdToTypeKey.find(targetId);
                    if (keyIt != mIdToTypeKey.end()) {
                        typeName = keyIt->second;
                    } else {
                        typeName = "UnknownTypeId(" + std::to_string(targetId) + ")";
                    }
                }
                result.contextPlaceholders[pluginName].push_back({placeholderName, typeName});
            }
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
    auto tpl = compileTemplate(text);
    return replacePlaceholders(tpl, ctx);
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
    auto futures = std::make_shared<std::vector<std::future<std::string>>>();
    futures->reserve(tpl.tokens.size());

    for (const auto& token : tpl.tokens) {
        if (auto* literal = std::get_if<LiteralToken>(&token)) {
            std::promise<std::string> p;
            std::string               unescaped;
            std::string_view          text = literal->text;
            unescaped.reserve(text.size());
            for (size_t i = 0; i < text.size(); ++i) {
                if (mEnableDoubleBraceEscape && i + 1 < text.size()) {
                    if (text[i] == '{' && text[i + 1] == '{') {
                        unescaped.push_back('{');
                        i++; // Skip the second brace
                        continue;
                    }
                    if (text[i] == '}' && text[i + 1] == '}') {
                        unescaped.push_back('}');
                        i++; // Skip the second brace
                        continue;
                    }
                }
                unescaped.push_back(text[i]);
            }
            p.set_value(std::move(unescaped));
            futures->push_back(p.get_future());
        } else if (auto* ph_token = std::get_if<PlaceholderToken>(&token)) {
            // Resolve params and default value SYNCHRONOUSLY on the calling thread.
            std::string paramString;
            if (ph_token->paramsTemplate) {
                paramString = replacePlaceholdersSync(*ph_token->paramsTemplate, ctx, 1);
            }

            std::string defaultText;
            if (ph_token->defaultTemplate) {
                defaultText = replacePlaceholdersSync(*ph_token->defaultTemplate, ctx, 1);
            }

            // Execute the placeholder asynchronously and get a future.
            auto ph_future =
                executePlaceholderAsync(ph_token->pluginName, ph_token->placeholderName, paramString, defaultText, ctx);
            futures->push_back(std::move(ph_future));
        }
    }

    // Enqueue a final task that waits for all futures and concatenates the results.
    return mCombinerThreadPool->enqueue([futures, sourceSize = tpl.source.size()]() -> std::string {
        std::string result;
        result.reserve(sourceSize);
        for (auto& f : *futures) {
            result.append(f.get());
        }
        return result;
    });
}

// [新] 同步替换实现
std::string
PlaceholderManager::replacePlaceholdersSync(const CompiledTemplate& tpl, const PlaceholderContext& ctx, int depth) {
    if (depth > mMaxRecursionDepth) {
        return tpl.source; // 超出深度，返回原始文本
    }

    std::string result;
    result.reserve(tpl.source.size());

    ReplaceState st; // 同步执行的本地状态
    st.depth = depth;

    for (const auto& token : tpl.tokens) {
        if (auto* literal = std::get_if<LiteralToken>(&token)) {
            std::string_view text = literal->text;
            for (size_t i = 0; i < text.size(); ++i) {
                if (mEnableDoubleBraceEscape && i + 1 < text.size()) {
                    if (text[i] == '{' && text[i + 1] == '{') {
                        result.push_back('{');
                        i++; // Skip the second brace
                        continue;
                    }
                    if (text[i] == '}' && text[i + 1] == '}') {
                        result.push_back('}');
                        i++; // Skip the second brace
                        continue;
                    }
                }
                result.push_back(text[i]);
            }
        } else if (auto* placeholder = std::get_if<PlaceholderToken>(&token)) {
            std::string paramString;
            if (placeholder->paramsTemplate) {
                paramString = replacePlaceholdersSync(*placeholder->paramsTemplate, ctx, depth + 1);
            }

            std::string defaultText;
            if (placeholder->defaultTemplate) {
                defaultText = replacePlaceholdersSync(*placeholder->defaultTemplate, ctx, depth + 1);
            }

            result.append(executePlaceholder(
                placeholder->pluginName,
                placeholder->placeholderName,
                paramString,
                defaultText,
                ctx,
                st
            ));
        }
    }
    return result;
}

// [新] 编译模板
CompiledTemplate PlaceholderManager::compileTemplate(const std::string& text) {
    CompiledTemplate tpl;
    tpl.source         = text; // Keep the source string alive
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
                logger.warn(
                    "Placeholder format error: '{}' is not a valid placeholder format. It seems to be missing a "
                    "colon ':'. The correct format is {{plugin:placeholder}}.",
                    std::string(inside)
                );
            }
            tpl.tokens.emplace_back(LiteralToken{s.substr(placeholderStart, j - placeholderStart + 1)});
            i = j + 1;
            continue;
        }

        PlaceholderToken token;
        token.pluginName      = PA::Utils::trim_sv(inside.substr(0, *colonPosOpt));
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

// [新] 私有辅助函数：构造缓存键
std::string PlaceholderManager::buildCacheKey(
    const PlaceholderContext& ctx,
    std::string_view          pluginName,
    std::string_view          placeholderName,
    const std::string&        paramString,
    CacheKeyStrategy          strategy
) {
    std::string cacheKey;
    cacheKey.reserve(pluginName.size() + placeholderName.size() + paramString.size() + 40);
    if (strategy == CacheKeyStrategy::ServerOnly) {
        cacheKey.push_back('#');
        cacheKey.append(pluginName);
        cacheKey.push_back(':');
        cacheKey.append(placeholderName);
        cacheKey.push_back('|');
        cacheKey.append(paramString);
    } else { // Default
        if (ctx.ptr) {
            cacheKey.append(std::to_string(reinterpret_cast<uintptr_t>(ctx.ptr)));
            cacheKey.push_back('#');
            cacheKey.append(std::to_string(ctx.typeId));
            cacheKey.push_back('#');
        }
        cacheKey.append(pluginName);
        cacheKey.push_back(':');
        cacheKey.append(placeholderName);
        cacheKey.push_back('|');
        cacheKey.append(paramString);
    }
    return cacheKey;
}

// [新] 私有辅助函数：执行单个占位符的查找与替换
std::string PlaceholderManager::executePlaceholder(
    std::string_view             pluginName,
    std::string_view             placeholderName,
    const std::string&           paramString,
    const std::string&           defaultText,
    const PlaceholderContext&    ctx,
    ReplaceState&                st,
    std::optional<CacheDuration> cache_duration_override
) {
    const auto startTime = std::chrono::high_resolution_clock::now();
    enum class PlaceholderType { None, Server, Context };
    PlaceholderType type = PlaceholderType::None;

    // --- 1. Find Candidate ---
    struct Candidate {
        std::vector<Caster>                                    chain;
        std::variant<AnyPtrReplacer, AnyPtrReplacerWithParams> fn;
        std::optional<CacheDuration>                           cacheDuration;
        CacheKeyStrategy                                       strategy;
    };
    std::optional<Candidate>               bestCandidate;
    std::vector<std::pair<int, Candidate>> candidates;

    if (ctx.ptr != nullptr && ctx.typeId != 0) {
        std::vector<TypedReplacer> potentialReplacers;
        {
            std::shared_lock lk(mMutex);
            auto             plugin_it = mContextPlaceholders.find(std::string(pluginName));
            if (plugin_it != mContextPlaceholders.end()) {
                auto ph_it = plugin_it->second.find(std::string(placeholderName));
                if (ph_it != plugin_it->second.end()) {
                    potentialReplacers = ph_it->second;
                }
            }
        }
        for (auto& entry : potentialReplacers) {
            std::vector<Caster> chain;
            if (entry.targetTypeId == ctx.typeId) {
                candidates.push_back({0, Candidate{{}, entry.fn, entry.cacheDuration, entry.strategy}});
            } else if (findUpcastChain(ctx.typeId, entry.targetTypeId, chain)) {
                candidates.push_back(
                    {(int)chain.size(), Candidate{std::move(chain), entry.fn, entry.cacheDuration, entry.strategy}}
                );
            }
        }
        if (!candidates.empty()) {
            std::sort(candidates.begin(), candidates.end(), [](auto& a, auto& b) { return a.first < b.first; });
            bestCandidate = std::move(candidates.front().second);
            type          = PlaceholderType::Context;
        }
    }

    std::optional<ServerReplacerEntry> serverEntry;
    if (!bestCandidate) {
        std::shared_lock lk(mMutex);
        auto             plugin_it = mServerPlaceholders.find(std::string(pluginName));
        if (plugin_it != mServerPlaceholders.end()) {
            auto placeholder_it = plugin_it->second.find(std::string(placeholderName));
            if (placeholder_it != plugin_it->second.end()) {
                serverEntry = placeholder_it->second;
                type        = PlaceholderType::Server;
            }
        }
    }

    // --- 2. Build Cache Key & Check Cache ---
    std::string                  cacheKey;
    std::optional<CacheDuration> cacheDuration = cache_duration_override;
    bool                         hasCandidate  = bestCandidate.has_value() || serverEntry.has_value();

    if (hasCandidate) {
        CacheKeyStrategy strategy = bestCandidate ? bestCandidate->strategy
                                    : serverEntry ? serverEntry->strategy
                                                  : CacheKeyStrategy::Default;
        cacheKey = buildCacheKey(ctx, pluginName, placeholderName, paramString, strategy);

        if (!cacheDuration) {
            cacheDuration = bestCandidate ? bestCandidate->cacheDuration : serverEntry->cacheDuration;
        }

        auto itCache = st.cache.find(cacheKey);
        if (itCache != st.cache.end()) {
            if (ConfigManager::getInstance().get().debugMode) {
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                                    std::chrono::high_resolution_clock::now() - startTime)
                                    .count();
                logger.info(
                    "Placeholder '{}:{}' | Cache Hit: Yes (local) | Type: {} | Time: {}us",
                    std::string(pluginName),
                    std::string(placeholderName),
                    type == PlaceholderType::Context ? "Context" : "Server",
                    duration
                );
            }
            std::string out = itCache->second;
            if (out.empty() && !defaultText.empty()) out = defaultText;
            return out;
        }

        if (cacheDuration) {
            auto cached = mGlobalCache.get(cacheKey);
            if (cached && cached->expiresAt > std::chrono::steady_clock::now()) {
                if (ConfigManager::getInstance().get().debugMode) {
                    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                                        std::chrono::high_resolution_clock::now() - startTime)
                                        .count();
                    logger.info(
                        "Placeholder '{}:{}' | Cache Hit: Yes (global) | Type: {} | Time: {}us",
                        std::string(pluginName),
                        std::string(placeholderName),
                        type == PlaceholderType::Context ? "Context" : "Server",
                        duration
                    );
                }
                st.cache.emplace(cacheKey, cached->result);
                std::string out = cached->result;
                if (out.empty() && !defaultText.empty()) out = defaultText;
                return out;
            }
        }
    }

    // --- 3. Execute Placeholder ---
    PA::Utils::ParsedParams params(paramString);
    bool                    allowEmpty = params.getBool("allowempty").value_or(false);
    std::string             replaced_val;
    bool                    replaced = false;

    if (bestCandidate) {
        void* p = ctx.ptr;
        for (auto& cfun : bestCandidate->chain) {
            p = cfun(p);
        }
        if (p) {
            if (std::holds_alternative<AnyPtrReplacer>(bestCandidate->fn)) {
                replaced_val = std::get<AnyPtrReplacer>(bestCandidate->fn)(p);
            } else {
                replaced_val = std::get<AnyPtrReplacerWithParams>(bestCandidate->fn)(p, params);
            }
            if (!replaced_val.empty() || allowEmpty) {
                replaced = true;
            }
        }
    } else if (serverEntry) {
        if (std::holds_alternative<ServerReplacer>(serverEntry->fn)) {
            replaced_val = std::get<ServerReplacer>(serverEntry->fn)();
        } else {
            replaced_val = std::get<ServerReplacerWithParams>(serverEntry->fn)(params);
        }
        if (!replaced_val.empty() || allowEmpty) {
            replaced = true;
        }
    }

    // --- 4. Finalize & Cache ---
    std::string formatted_val;
    if (replaced) {
        formatted_val = PA::Utils::applyFormatting(replaced_val, params);
    }

    std::string finalOut;
    if (replaced && (!formatted_val.empty() || allowEmpty)) {
        finalOut = formatted_val;
    } else {
        finalOut = defaultText;
    }

    if (ConfigManager::getInstance().get().debugMode) {
        auto duration =
            std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - startTime)
                .count();
        const char* typeStr = "Unknown";
        if (type == PlaceholderType::Context) typeStr = "Context";
        if (type == PlaceholderType::Server) typeStr = "Server";

        if (!replaced) {
            logger.warn(
                "Placeholder '{}:{}' not found or returned empty, and no default value provided. Using default: '{}'. "
                "Context ptr: {}, typeId: {}. Params: '{}'. Time: {}us",
                std::string(pluginName),
                std::string(placeholderName),
                defaultText,
                reinterpret_cast<uintptr_t>(ctx.ptr),
                ctx.typeId,
                paramString,
                duration
            );
        } else {
            logger.info(
                "Placeholder '{}:{}' | Cache Hit: No | Type: {} | Time: {}us",
                std::string(pluginName),
                std::string(placeholderName),
                typeStr,
                duration
            );
        }
    }

    if (finalOut.empty() && defaultText.empty() && !replaced && ConfigManager::getInstance().get().debugMode) {
        finalOut.reserve(pluginName.size() + placeholderName.size() + paramString.size() + 4);
        finalOut.append("{").append(pluginName).append(":").append(placeholderName);
        if (!paramString.empty()) finalOut.append("|").append(paramString);
        finalOut.append("}");
    }

    if (replaced) {
        st.cache.emplace(cacheKey, finalOut);
        if (cacheDuration) {
            mGlobalCache.put(cacheKey, {finalOut, std::chrono::steady_clock::now() + *cacheDuration});
        }
    }

    return finalOut;
}


// [新] 私有辅助函数：执行单个占位符的异步查找与替换
std::future<std::string> PlaceholderManager::executePlaceholderAsync(
    std::string_view             pluginName,
    std::string_view             placeholderName,
    const std::string&           paramString,
    const std::string&           defaultText,
    const PlaceholderContext&    ctx,
    std::optional<CacheDuration> cache_duration_override
) {
    const auto startTime = std::chrono::high_resolution_clock::now();
    enum class PlaceholderType { None, Server, Context, AsyncServer, AsyncContext, SyncFallback };
    PlaceholderType type = PlaceholderType::None;

    // --- 1. Find Async Candidate ---
    struct AsyncCandidate {
        std::vector<Caster>                                              chain;
        std::variant<AsyncAnyPtrReplacer, AsyncAnyPtrReplacerWithParams> fn;
        std::optional<CacheDuration>                                     cacheDuration;
        CacheKeyStrategy                                                 strategy;
    };
    std::optional<AsyncCandidate> bestAsyncCandidate;

    if (ctx.ptr != nullptr && ctx.typeId != 0) {
        std::vector<std::pair<int, AsyncCandidate>> asyncCandidates;
        std::vector<AsyncTypedReplacer>             potentialAsyncReplacers;
        {
            std::shared_lock lk(mMutex);
            auto             plugin_it = mAsyncContextPlaceholders.find(std::string(pluginName));
            if (plugin_it != mAsyncContextPlaceholders.end()) {
                auto ph_it = plugin_it->second.find(std::string(placeholderName));
                if (ph_it != plugin_it->second.end()) {
                    potentialAsyncReplacers = ph_it->second;
                }
            }
        }
        for (auto& entry : potentialAsyncReplacers) {
            std::vector<Caster> chain;
            if (entry.targetTypeId == ctx.typeId) {
                asyncCandidates.push_back({0, AsyncCandidate{{}, entry.fn, entry.cacheDuration, entry.strategy}});
            } else if (findUpcastChain(ctx.typeId, entry.targetTypeId, chain)) {
                asyncCandidates.push_back(
                    {(int)chain.size(),
                     AsyncCandidate{std::move(chain), entry.fn, entry.cacheDuration, entry.strategy}}
                );
            }
        }
        if (!asyncCandidates.empty()) {
            std::sort(
                asyncCandidates.begin(),
                asyncCandidates.end(),
                [](auto& a, auto& b) { return a.first < b.first; }
            );
            bestAsyncCandidate = std::move(asyncCandidates.front().second);
            type               = PlaceholderType::AsyncContext;
        }
    }

    std::optional<AsyncServerReplacerEntry> asyncServerEntry;
    if (!bestAsyncCandidate) {
        std::shared_lock lk(mMutex);
        auto             plugin_it = mAsyncServerPlaceholders.find(std::string(pluginName));
        if (plugin_it != mAsyncServerPlaceholders.end()) {
            auto placeholder_it = plugin_it->second.find(std::string(placeholderName));
            if (placeholder_it != plugin_it->second.end()) {
                asyncServerEntry = placeholder_it->second;
                type             = PlaceholderType::AsyncServer;
            }
        }
    }

    // --- 2. Build Cache Key & Check Cache ---
    std::string                  cacheKey;
    std::optional<CacheDuration> cacheDuration     = cache_duration_override;
    bool                         hasAsyncCandidate = bestAsyncCandidate.has_value() || asyncServerEntry.has_value();

    if (hasAsyncCandidate) {
        CacheKeyStrategy strategy = bestAsyncCandidate ? bestAsyncCandidate->strategy
                                    : asyncServerEntry ? asyncServerEntry->strategy
                                                       : CacheKeyStrategy::Default;
        cacheKey = buildCacheKey(ctx, pluginName, placeholderName, paramString, strategy);

        if (!cacheDuration) {
            cacheDuration = bestAsyncCandidate ? bestAsyncCandidate->cacheDuration : asyncServerEntry->cacheDuration;
        }

        if (cacheDuration) {
            auto cached = mGlobalCache.get(cacheKey);
            if (cached && cached->expiresAt > std::chrono::steady_clock::now()) {
                if (ConfigManager::getInstance().get().debugMode) {
                    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                                        std::chrono::high_resolution_clock::now() - startTime)
                                        .count();
                    logger.info(
                        "Placeholder '{}:{}' | Cache Hit: Yes (global) | Type: {} | Time: {}us",
                        std::string(pluginName),
                        std::string(placeholderName),
                        type == PlaceholderType::AsyncContext ? "AsyncContext" : "AsyncServer",
                        duration
                    );
                }
                std::promise<std::string> promise;
                promise.set_value(cached->result);
                return promise.get_future();
            }
        }
    }

    // --- 3. Execute Placeholder ---
    std::future<std::string> future;
    if (bestAsyncCandidate) {
        PA::Utils::ParsedParams params(paramString);
        void*                   p = ctx.ptr;
        for (auto& cfun : bestAsyncCandidate->chain) {
            p = cfun(p);
        }
        if (p) {
            if (std::holds_alternative<AsyncAnyPtrReplacer>(bestAsyncCandidate->fn)) {
                future = std::get<AsyncAnyPtrReplacer>(bestAsyncCandidate->fn)(p);
            } else {
                future = std::get<AsyncAnyPtrReplacerWithParams>(bestAsyncCandidate->fn)(p, params);
            }
        } else {
            std::promise<std::string> p_promise;
            p_promise.set_value("");
            future = p_promise.get_future();
        }
    } else if (asyncServerEntry) {
        PA::Utils::ParsedParams params(paramString);
        if (std::holds_alternative<AsyncServerReplacer>(asyncServerEntry->fn)) {
            future = std::get<AsyncServerReplacer>(asyncServerEntry->fn)();
        } else {
            future = std::get<AsyncServerReplacerWithParams>(asyncServerEntry->fn)(params);
        }
    } else {
        type = PlaceholderType::SyncFallback;
        std::promise<std::string> promise;
        ReplaceState              st;
        promise.set_value(
            executePlaceholder(pluginName, placeholderName, paramString, defaultText, ctx, st, cache_duration_override)
        );
        return promise.get_future();
    }

    // --- 4. Finalize & Cache (for async paths) ---
    PA::Utils::ParsedParams params(paramString);
    bool                    allowEmpty = params.getBool("allowempty").value_or(false);

    return mCombinerThreadPool->enqueue([this,
                                 startTime,
                                 type,
                                 fut             = std::move(future),
                                 pluginName      = std::string(pluginName),
                                 placeholderName = std::string(placeholderName),
                                 paramString,
                                 defaultText,
                                 allowEmpty,
                                 cacheKey,
                                 cacheDuration]() mutable {
        std::string replaced_val = fut.get();
        bool        replaced     = !replaced_val.empty() || allowEmpty;

        PA::Utils::ParsedParams params(paramString);
        std::string             formatted_val;
        if (replaced) {
            formatted_val = PA::Utils::applyFormatting(replaced_val, params);
        }

        std::string finalOut;
        if (replaced && (!formatted_val.empty() || allowEmpty)) {
            finalOut = formatted_val;
        } else {
            finalOut = defaultText;
        }

        if (ConfigManager::getInstance().get().debugMode) {
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                                std::chrono::high_resolution_clock::now() - startTime)
                                .count();
            const char* typeStr = "Unknown";
            if (type == PlaceholderType::AsyncContext) typeStr = "AsyncContext";
            if (type == PlaceholderType::AsyncServer) typeStr = "AsyncServer";

            logger.info(
                "Placeholder '{}:{}' | Cache Hit: No | Type: {} | Time: {}us",
                pluginName,
                placeholderName,
                typeStr,
                duration
            );
        }

        if (finalOut.empty() && defaultText.empty() && !replaced && ConfigManager::getInstance().get().debugMode) {
            finalOut.reserve(pluginName.size() + placeholderName.size() + paramString.size() + 4);
            finalOut.append("{").append(pluginName).append(":").append(placeholderName);
            if (!paramString.empty()) finalOut.append("|").append(paramString);
            finalOut.append("}");
        }

        if (replaced && cacheDuration) {
            mGlobalCache.put(cacheKey, {finalOut, std::chrono::steady_clock::now() + *cacheDuration});
        }
        return finalOut;
    });
}

// [新] 使用编译模板进行替换
std::string PlaceholderManager::replacePlaceholders(const CompiledTemplate& tpl, const PlaceholderContext& ctx) {
    return replacePlaceholdersSync(tpl, ctx, 0);
}

} // namespace PA
