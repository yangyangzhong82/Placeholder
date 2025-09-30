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

PlaceholderManager::PlaceholderManager() :
    mTypeSystem(std::make_shared<PlaceholderTypeSystem>()),
    mRegistry(std::make_shared<PlaceholderRegistry>(mTypeSystem)),
    mGlobalCache(ConfigManager::getInstance().get().globalCacheSize),
    mCompileCache(ConfigManager::getInstance().get().globalCacheSize),
    mParamsCache(ConfigManager::getInstance().get().globalCacheSize) {
    unsigned int hardwareConcurrency = std::thread::hardware_concurrency();
    if (hardwareConcurrency == 0) {
        hardwareConcurrency = 2; // 硬件并发未知时的默认值
    }

    mCombinerThreadPool = std::make_unique<ThreadPool>(1); // 合并线程池只需要一个线程

    const auto&  config        = ConfigManager::getInstance().get();
    int          asyncPoolSize = config.asyncThreadPoolSize;
    if (asyncPoolSize <= 0) {
        asyncPoolSize = hardwareConcurrency;
    }
    mAsyncThreadPool =
        std::make_unique<ThreadPool>(asyncPoolSize, static_cast<size_t>(config.asyncThreadPoolQueueSize));


    // 注册一个回调，当配置重新加载时，更新缓存大小
    ConfigManager::getInstance().onReload([this](const Config& newConfig) {
        mGlobalCache.setCapacity(newConfig.globalCacheSize);
        mCompileCache.setCapacity(newConfig.globalCacheSize);
        mParamsCache.setCapacity(newConfig.globalCacheSize);
        // 注意：线程池大小在运行时不便更改，因此这里不处理 asyncThreadPoolSize 的热重载
    });
}

// ==== 类型系统实现 ====

std::size_t PlaceholderManager::ensureTypeId(const std::string& typeKeyStr) {
    return mTypeSystem->ensureTypeId(typeKeyStr);
}

void PlaceholderManager::registerInheritanceByKeys(
    const std::string& derivedKey,
    const std::string& baseKey,
    Caster             caster
) {
    mTypeSystem->registerInheritanceByKeys(derivedKey, baseKey, caster);
}

void PlaceholderManager::registerInheritanceByKeysBatch(const std::vector<InheritancePair>& pairs) {
    mTypeSystem->registerInheritanceByKeysBatch(pairs);
}

void PlaceholderManager::registerTypeAlias(const std::string& alias, const std::string& typeKeyStr) {
    mTypeSystem->registerTypeAlias(alias, typeKeyStr);
}

bool PlaceholderManager::findUpcastChain(std::size_t fromTypeId, std::size_t toTypeId, std::vector<Caster>& outChain)
    const {
    return mTypeSystem->findUpcastChain(fromTypeId, toTypeId, outChain);
}

// ==== 占位符注册 ====

void PlaceholderManager::registerServerPlaceholder(
    const std::string&           pluginName,
    const std::string&           placeholder,
    ServerReplacer               replacer,
    std::optional<CacheDuration> cache_duration,
    CacheKeyStrategy             strategy
) {
    mRegistry->registerServerPlaceholder(pluginName, placeholder, replacer, cache_duration, strategy);
}

void PlaceholderManager::registerServerPlaceholderWithParams(
    const std::string&           pluginName,
    const std::string&           placeholder,
    ServerReplacerWithParams&&   replacer,
    std::optional<CacheDuration> cache_duration,
    CacheKeyStrategy             strategy
) {
    mRegistry->registerServerPlaceholderWithParams(pluginName, placeholder, std::move(replacer), cache_duration, strategy);
}


void PlaceholderManager::registerPlaceholderForTypeKey(
    const std::string& pluginName,
    const std::string& placeholder,
    const std::string& typeKeyStr,
    AnyPtrReplacer     replacer
) {
    auto id = ensureTypeId(typeKeyStr);
    mRegistry->registerPlaceholderForTypeId(
        pluginName,
        placeholder,
        id,
        std::move(replacer),
        std::nullopt,
        CacheKeyStrategy::Default
    );
}

void PlaceholderManager::registerPlaceholderForTypeKeyWithParams(
    const std::string&         pluginName,
    const std::string&         placeholder,
    const std::string&         typeKeyStr,
    AnyPtrReplacerWithParams&& replacer
) {
    auto id = ensureTypeId(typeKeyStr);
    mRegistry->registerPlaceholderForTypeId(
        pluginName,
        placeholder,
        id,
        std::move(replacer),
        std::nullopt,
        CacheKeyStrategy::Default
    );
}


// --- 新：异步占位符注册 ---

void PlaceholderManager::registerAsyncServerPlaceholder(
    const std::string&           pluginName,
    const std::string&           placeholder,
    AsyncServerReplacer&&        replacer,
    std::optional<CacheDuration> cache_duration,
    CacheKeyStrategy             strategy
) {
    mRegistry->registerAsyncServerPlaceholder(pluginName, placeholder, std::move(replacer), cache_duration, strategy);
}

void PlaceholderManager::registerAsyncServerPlaceholderWithParams(
    const std::string&              pluginName,
    const std::string&              placeholder,
    AsyncServerReplacerWithParams&& replacer,
    std::optional<CacheDuration>    cache_duration,
    CacheKeyStrategy                strategy
) {
    mRegistry->registerAsyncServerPlaceholderWithParams(pluginName, placeholder, std::move(replacer), cache_duration, strategy);
}


void PlaceholderManager::registerAsyncPlaceholderForTypeKey(
    const std::string&    pluginName,
    const std::string&    placeholder,
    const std::string&    typeKeyStr,
    AsyncAnyPtrReplacer&& replacer
) {
    auto id = ensureTypeId(typeKeyStr);
    mRegistry->registerAsyncPlaceholderForTypeId(
        pluginName,
        placeholder,
        id,
        std::move(replacer),
        std::nullopt,
        CacheKeyStrategy::Default
    );
}

void PlaceholderManager::registerAsyncPlaceholderForTypeKeyWithParams(
    const std::string&              pluginName,
    const std::string&              placeholder,
    const std::string&              typeKeyStr,
    AsyncAnyPtrReplacerWithParams&& replacer
) {
    auto id = ensureTypeId(typeKeyStr);
    mRegistry->registerAsyncPlaceholderForTypeId(
        pluginName,
        placeholder,
        id,
        std::move(replacer),
        std::nullopt,
        CacheKeyStrategy::Default
    );
}

// ==== 注销 & 缓存清理 ====

void PlaceholderManager::clearCache() { mGlobalCache.clear(); }

void PlaceholderManager::clearCache(const std::string& pluginName) {
    mGlobalCache.remove_if([&pluginName](const std::string& key) {
        // Key formats:
        // - ServerOnly: #plugin:ph|params
        // - No context: plugin:ph|params
        // - Context:    ctx#type#plugin:ph|params
        // - Relational: ctx#type#relctx#reltype#plugin:ph|params
        // The plugin name is always located between the last '#' (or start) and the first ':' before '|'.

        auto pipePos = key.find('|');
        if (pipePos == std::string::npos) {
            return false; // Not a valid placeholder cache key.
        }

        std::string_view keyView = std::string_view(key).substr(0, pipePos);

        auto colonPos = keyView.rfind(':');
        if (colonPos == std::string::npos) {
            return false; // Invalid key format, no placeholder name.
        }

        auto lastHashPos = keyView.rfind('#');

        std::string_view keyPluginName;
        if (lastHashPos == std::string::npos) {
            // Case: "plugin:ph" (no context)
            keyPluginName = keyView.substr(0, colonPos);
        } else {
            // Case: "...#plugin:ph" (server, context, relational)
            keyPluginName = keyView.substr(lastHashPos + 1, colonPos - (lastHashPos + 1));
        }

        return keyPluginName == pluginName;
    });
}

void PlaceholderManager::unregisterPlaceholders(const std::string& pluginName) {
    mRegistry->unregisterPlaceholders(pluginName);
}

void PlaceholderManager::unregisterAsyncPlaceholders(const std::string& pluginName) {
    mRegistry->unregisterAsyncPlaceholders(pluginName);
}

PlaceholderManager::AllPlaceholders PlaceholderManager::getAllPlaceholders() const {
    return mRegistry->getAllPlaceholders();
}

bool PlaceholderManager::hasPlaceholder(
    const std::string&                pluginName,
    const std::string&                placeholderName,
    const std::optional<std::string>& typeKey
) const {
    return mRegistry->hasPlaceholder(pluginName, placeholderName, typeKey);
}


// ==== Context 构造 ====

PlaceholderContext
PlaceholderManager::makeContextRaw(void* ptr, const std::string& typeKeyStr, const PlaceholderContext* rel_ctx) {
    PlaceholderContext ctx;
    ctx.ptr               = ptr;
    ctx.typeId            = getInstance().ensureTypeId(typeKeyStr);
    ctx.relationalContext = rel_ctx;
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
    if (auto cachedTpl = mCompileCache.get(text)) {
        return replacePlaceholders(**cachedTpl, ctx);
    }
    auto tpl = std::make_shared<CompiledTemplate>(compileTemplate(text));
    mCompileCache.put(text, tpl);
    return replacePlaceholders(*tpl, ctx);
}

std::string PlaceholderManager::replacePlaceholders(const std::string& text, Player* player) {
    return replacePlaceholders(text, makeContext(player));
}


// --- 新：关系型占位符注册 ---


void PlaceholderManager::registerRelationalPlaceholderForTypeKey(
    const std::string&           pluginName,
    const std::string&           placeholder,
    const std::string&           typeKeyStr,
    const std::string&           relationalTypeKeyStr,
    AnyPtrRelationalReplacer&&   replacer,
    std::optional<CacheDuration> cache_duration,
    CacheKeyStrategy             strategy
) {
    auto targetId     = ensureTypeId(typeKeyStr);
    auto relationalId = ensureTypeId(relationalTypeKeyStr);
    mRegistry->registerRelationalPlaceholderForTypeId(
        pluginName,
        placeholder,
        targetId,
        relationalId,
        std::move(replacer),
        cache_duration,
        strategy
    );
}

void PlaceholderManager::registerRelationalPlaceholderForTypeKeyWithParams(
    const std::string&                   pluginName,
    const std::string&                   placeholder,
    const std::string&                   typeKeyStr,
    const std::string&                   relationalTypeKeyStr,
    AnyPtrRelationalReplacerWithParams&& replacer,
    std::optional<CacheDuration>         cache_duration,
    CacheKeyStrategy                     strategy
) {
    auto targetId     = ensureTypeId(typeKeyStr);
    auto relationalId = ensureTypeId(relationalTypeKeyStr);
    mRegistry->registerRelationalPlaceholderForTypeId(
        pluginName,
        placeholder,
        targetId,
        relationalId,
        std::move(replacer),
        cache_duration,
        strategy
    );
}

// --- 新：异步替换 ---

std::future<std::string>
PlaceholderManager::replacePlaceholdersAsync(const std::string& text, const PlaceholderContext& ctx) {
    if (auto cachedTpl = mCompileCache.get(text)) {
        return replacePlaceholdersAsync(**cachedTpl, ctx);
    }
    auto tpl = std::make_shared<CompiledTemplate>(compileTemplate(text));
    mCompileCache.put(text, tpl);
    return replacePlaceholdersAsync(*tpl, ctx);
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

std::vector<std::string> PlaceholderManager::replacePlaceholdersBatch(
    const std::vector<std::reference_wrapper<const CompiledTemplate>>& tpls,
    const PlaceholderContext&                                          ctx
) {
    std::vector<std::string> results;
    results.reserve(tpls.size());

    // Create a shared state for this batch operation
    ReplaceState st;
    st.depth = 0;

    for (const auto& tpl_ref : tpls) {
        const auto& tpl = tpl_ref.get();
        if (st.depth > mMaxRecursionDepth) {
            results.push_back(tpl.source);
            continue;
        }

        std::string result;
        result.reserve(tpl.source.size());

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
                    // Note: Nested replacements in batch mode won't share the batch's cache.
                    // This is a limitation to avoid complexity.
                    paramString = replacePlaceholdersSync(*placeholder->paramsTemplate, ctx, st.depth + 1);
                }

                std::string defaultText;
                if (placeholder->defaultTemplate) {
                    defaultText = replacePlaceholdersSync(*placeholder->defaultTemplate, ctx, st.depth + 1);
                }

                // Use the shared state 'st'
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
        results.push_back(std::move(result));
    }

    return results;
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
                } else if (s[literalEnd] == '%' && literalEnd + 1 < n && s[literalEnd + 1] == '%') {
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
                } else if (s[literalEnd] == '%' && literalEnd + 1 < n && s[literalEnd + 1] == '%') {
                    literalEnd += 2;
                } else {
                    break;
                }
            }
            tpl.tokens.emplace_back(LiteralToken{s.substr(i, literalEnd - i)});
            i = literalEnd;
            continue;
        }
        // 新增：百分号转义
        if (c == '%' && i + 1 < n && s[i + 1] == '%') {
            size_t literalEnd = i + 2;
            while (literalEnd < n) {
                if (s[literalEnd] == '%' && literalEnd + 1 < n && s[literalEnd + 1] == '%') {
                    literalEnd += 2;
                } else if (mEnableDoubleBraceEscape && s[literalEnd] == '{' && literalEnd + 1 < n && s[literalEnd + 1] == '{') {
                    literalEnd += 2;
                } else if (mEnableDoubleBraceEscape && s[literalEnd] == '}' && literalEnd + 1 < n && s[literalEnd + 1] == '}') {
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
    cacheKey.reserve(pluginName.size() + placeholderName.size() + paramString.size() + 80);
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
        // Add relational context to cache key
        if (ctx.relationalContext && ctx.relationalContext->ptr) {
            cacheKey.append(std::to_string(reinterpret_cast<uintptr_t>(ctx.relationalContext->ptr)));
            cacheKey.push_back('#');
            cacheKey.append(std::to_string(ctx.relationalContext->typeId));
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
// --- 新：列表/集合型占位符注册 ---

void PlaceholderManager::registerServerListPlaceholder(
    const std::string&           pluginName,
    const std::string&           placeholder,
    ServerListReplacer&&         replacer,
    std::optional<CacheDuration> cache_duration,
    CacheKeyStrategy             strategy
) {
    mRegistry->registerServerListPlaceholder(pluginName, placeholder, std::move(replacer), cache_duration, strategy);
}

void PlaceholderManager::registerServerListPlaceholderWithParams(
    const std::string&             pluginName,
    const std::string&             placeholder,
    ServerListReplacerWithParams&& replacer,
    std::optional<CacheDuration>   cache_duration,
    CacheKeyStrategy               strategy
) {
    mRegistry->registerServerListPlaceholderWithParams(pluginName, placeholder, std::move(replacer), cache_duration, strategy);
}


// --- 新：对象列表/集合型占位符注册 ---

void PlaceholderManager::registerServerObjectListPlaceholder(
    const std::string&           pluginName,
    const std::string&           placeholder,
    ServerObjectListReplacer&&   replacer,
    std::optional<CacheDuration> cache_duration,
    CacheKeyStrategy             strategy
) {
    mRegistry->registerServerObjectListPlaceholder(pluginName, placeholder, std::move(replacer), cache_duration, strategy);
}

void PlaceholderManager::registerServerObjectListPlaceholderWithParams(
    const std::string&                   pluginName,
    const std::string&                   placeholder,
    ServerObjectListReplacerWithParams&& replacer,
    std::optional<CacheDuration>         cache_duration,
    CacheKeyStrategy                     strategy
) {
    mRegistry->registerServerObjectListPlaceholderWithParams(pluginName, placeholder, std::move(replacer), cache_duration, strategy);
}


// [新] 私有辅助函数：获取解析后的参数
std::shared_ptr<Utils::ParsedParams> PlaceholderManager::getParsedParams(const std::string& paramString) {
    if (auto cachedParams = mParamsCache.get(paramString)) {
        return *cachedParams;
    }
    auto paramsPtr = std::make_shared<Utils::ParsedParams>(paramString);
    mParamsCache.put(paramString, paramsPtr);
    return paramsPtr;
}

// [新] 私有辅助函数：执行找到的替换器
std::string PlaceholderManager::executeFoundReplacer(
    const PlaceholderRegistry::ReplacerMatch& match,
    void*                                     p,
    void*                                     p_rel,
    const Utils::ParsedParams&                params,
    bool                                      allowEmpty,
    ReplaceState&                             st
) {
    std::string replaced_val;
    bool        replaced = false;

    switch (match.type) {
    case PlaceholderRegistry::PlaceholderType::Relational: {
        const auto& entry = std::get<PlaceholderRegistry::RelationalTypedReplacer>(match.entry);
        if (p && p_rel) {
            try {
                if (std::holds_alternative<AnyPtrRelationalReplacer>(entry.fn)) {
                    replaced_val = std::get<AnyPtrRelationalReplacer>(entry.fn)(p, p_rel);
                } else {
                    replaced_val = std::get<AnyPtrRelationalReplacerWithParams>(entry.fn)(p, p_rel, params);
                }
                if (!replaced_val.empty() || allowEmpty) {
                    replaced = true;
                }
            } catch (const std::exception& e) {
                logger.error("Relational replacer threw an exception: {}", e.what());
                replaced_val = ""; // Return empty string on exception
            } catch (...) {
                logger.error("Relational replacer threw an unknown exception.");
                replaced_val = ""; // Return empty string on unknown exception
            }
        }
        break;
    }
    case PlaceholderRegistry::PlaceholderType::Context: {
        const auto& entry = std::get<PlaceholderRegistry::TypedReplacer>(match.entry);
        if (p) {
            try {
                if (std::holds_alternative<AnyPtrReplacer>(entry.fn)) {
                    replaced_val = std::get<AnyPtrReplacer>(entry.fn)(p);
                } else {
                    replaced_val = std::get<AnyPtrReplacerWithParams>(entry.fn)(p, params);
                }
                if (!replaced_val.empty() || allowEmpty) {
                    replaced = true;
                }
            } catch (const std::exception& e) {
                logger.error("Context replacer threw an exception: {}", e.what());
                replaced_val = "";
            } catch (...) {
                logger.error("Context replacer threw an unknown exception.");
                replaced_val = "";
            }
        }
        break;
    }
    case PlaceholderRegistry::PlaceholderType::ListContext: {
        const auto& entry = std::get<PlaceholderRegistry::TypedListReplacer>(match.entry);
        if (p) {
            std::vector<std::string> result_list;
            try {
                if (std::holds_alternative<AnyPtrListReplacer>(entry.fn)) {
                    result_list = std::get<AnyPtrListReplacer>(entry.fn)(p);
                } else {
                    result_list = std::get<AnyPtrListReplacerWithParams>(entry.fn)(p, params);
                }
                if (!result_list.empty() || allowEmpty) {
                    std::string separator = std::string(params.get("separator").value_or(", "));
                    replaced_val          = PA::Utils::join(result_list, separator);
                    replaced              = true;
                }
            } catch (const std::exception& e) {
                logger.error("ListContext replacer threw an exception: {}", e.what());
                replaced_val = "";
            } catch (...) {
                logger.error("ListContext replacer threw an unknown exception.");
                replaced_val = "";
            }
        }
        break;
    }
    case PlaceholderRegistry::PlaceholderType::ObjectListContext: {
        const auto& entry = std::get<PlaceholderRegistry::TypedObjectListReplacer>(match.entry);
        if (p) {
            std::vector<PlaceholderContext> object_list;
            try {
                if (std::holds_alternative<AnyPtrObjectListReplacer>(entry.fn)) {
                    object_list = std::get<AnyPtrObjectListReplacer>(entry.fn)(p);
                } else {
                    object_list = std::get<AnyPtrObjectListReplacerWithParams>(entry.fn)(p, params);
                }

                if (!object_list.empty() || allowEmpty) {
                    std::string join_separator = std::string(params.get("join").value_or(""));
                    std::string template_str   = std::string(params.get("template").value_or(""));

                    std::vector<std::string> replaced_objects;
                    replaced_objects.reserve(object_list.size());

                    if (!template_str.empty()) {
                        auto compiled_template = compileTemplate(template_str);
                        for (const auto& obj_ctx : object_list) {
                            replaced_objects.push_back(replacePlaceholdersSync(compiled_template, obj_ctx, st.depth + 1));
                        }
                    } else {
                        for (const auto& obj_ctx : object_list) {
                            replaced_objects.push_back(mTypeSystem->getTypeName(obj_ctx.typeId));
                        }
                    }
                    replaced_val = PA::Utils::join(replaced_objects, join_separator);
                    replaced     = true;
                }
            } catch (const std::exception& e) {
                logger.error("ObjectListContext replacer threw an exception: {}", e.what());
                replaced_val = "";
            } catch (...) {
                logger.error("ObjectListContext replacer threw an unknown exception.");
                replaced_val = "";
            }
        }
        break;
    }
    case PlaceholderRegistry::PlaceholderType::Server: {
        const auto& entry = std::get<PlaceholderRegistry::ServerReplacerEntry>(match.entry);
        try {
            if (std::holds_alternative<ServerReplacer>(entry.fn)) {
                replaced_val = std::get<ServerReplacer>(entry.fn)();
            } else {
                replaced_val = std::get<ServerReplacerWithParams>(entry.fn)(params);
            }
            if (!replaced_val.empty() || allowEmpty) {
                replaced = true;
            }
        } catch (const std::exception& e) {
            logger.error("Server replacer threw an exception: {}", e.what());
            replaced_val = "";
        } catch (...) {
            logger.error("Server replacer threw an unknown exception.");
            replaced_val = "";
        }
        break;
    }
    case PlaceholderRegistry::PlaceholderType::ListServer: {
        const auto& entry = std::get<PlaceholderRegistry::ServerListReplacerEntry>(match.entry);
        std::vector<std::string> result_list;
        try {
            if (std::holds_alternative<ServerListReplacer>(entry.fn)) {
                result_list = std::get<ServerListReplacer>(entry.fn)();
            } else {
                result_list = std::get<ServerListReplacerWithParams>(entry.fn)(params);
            }
            if (!result_list.empty() || allowEmpty) {
                std::string separator = std::string(params.get("separator").value_or(", "));
                replaced_val          = PA::Utils::join(result_list, separator);
                replaced              = true;
            }
        } catch (const std::exception& e) {
            logger.error("ListServer replacer threw an exception: {}", e.what());
            replaced_val = "";
        } catch (...) {
            logger.error("ListServer replacer threw an unknown exception.");
            replaced_val = "";
        }
        break;
    }
    case PlaceholderRegistry::PlaceholderType::ObjectListServer: {
        const auto& entry = std::get<PlaceholderRegistry::ServerObjectListReplacerEntry>(match.entry);
        std::vector<PlaceholderContext> object_list;
        try {
            if (std::holds_alternative<ServerObjectListReplacer>(entry.fn)) {
                object_list = std::get<ServerObjectListReplacer>(entry.fn)();
            } else {
                object_list = std::get<ServerObjectListReplacerWithParams>(entry.fn)(params);
            }

            if (!object_list.empty() || allowEmpty) {
                std::string join_separator = std::string(params.get("join").value_or(""));
                std::string template_str   = std::string(params.get("template").value_or(""));

                std::vector<std::string> replaced_objects;
                replaced_objects.reserve(object_list.size());

                if (!template_str.empty()) {
                    auto compiled_template = compileTemplate(template_str);
                    for (const auto& obj_ctx : object_list) {
                        replaced_objects.push_back(replacePlaceholdersSync(compiled_template, obj_ctx, st.depth + 1));
                    }
                } else {
                    for (const auto& obj_ctx : object_list) {
                        replaced_objects.push_back(mTypeSystem->getTypeName(obj_ctx.typeId));
                    }
                }
                replaced_val = PA::Utils::join(replaced_objects, join_separator);
                replaced     = true;
            }
        } catch (const std::exception& e) {
            logger.error("ObjectListServer replacer threw an exception: {}", e.what());
            replaced_val = "";
        } catch (...) {
            logger.error("ObjectListServer replacer threw an unknown exception.");
            replaced_val = "";
        }
        break;
    }
    case PlaceholderRegistry::PlaceholderType::AsyncServer: {
        const auto& entry = std::get<PlaceholderRegistry::AsyncServerReplacerEntry>(match.entry);
        std::future<std::string> future_val;
        try {
            if (std::holds_alternative<AsyncServerReplacer>(entry.fn)) {
                future_val = std::get<AsyncServerReplacer>(entry.fn)();
            } else {
                future_val = std::get<AsyncServerReplacerWithParams>(entry.fn)(params);
            }
            replaced_val = future_val.get(); // Block for async result in sync call
            if (!replaced_val.empty() || allowEmpty) {
                replaced = true;
            }
        } catch (const std::exception& e) {
            logger.error("AsyncServer replacer threw an exception: {}", e.what());
            replaced_val = "";
        } catch (...) {
            logger.error("AsyncServer replacer threw an unknown exception.");
            replaced_val = "";
        }
        break;
    }
    case PlaceholderRegistry::PlaceholderType::AsyncContext: {
        const auto& entry = std::get<PlaceholderRegistry::AsyncTypedReplacer>(match.entry);
        if (p) {
            std::future<std::string> future_val;
            try {
                if (std::holds_alternative<AsyncAnyPtrReplacer>(entry.fn)) {
                    future_val = std::get<AsyncAnyPtrReplacer>(entry.fn)(p);
                } else {
                    future_val = std::get<AsyncAnyPtrReplacerWithParams>(entry.fn)(p, params);
                }
                replaced_val = future_val.get(); // Block for async result in sync call
                if (!replaced_val.empty() || allowEmpty) {
                    replaced = true;
                }
            } catch (const std::exception& e) {
                logger.error("AsyncContext replacer threw an exception: {}", e.what());
                replaced_val = "";
            } catch (...) {
                logger.error("AsyncContext replacer threw an unknown exception.");
                replaced_val = "";
            }
        }
        break;
    }
    case PlaceholderRegistry::PlaceholderType::None:
    case PlaceholderRegistry::PlaceholderType::SyncFallback:
        // These types are not handled by this synchronous execution function.
        // They should be handled by the caller or are fallback types.
        break;
    }
    return replaced ? replaced_val : "";
}

// [新] 私有辅助函数：应用格式化、处理缓存和日志记录
std::string PlaceholderManager::applyFormattingAndCache(
    const std::string&                   originalResult,
    const Utils::ParsedParams&           params,
    const std::string&                   defaultText,
    bool                                 allowEmpty,
    const std::string&                   cacheKey,
    std::optional<CacheDuration>         cacheDuration,
    PlaceholderRegistry::PlaceholderType type,
    std::chrono::steady_clock::time_point startTime,
    ReplaceState&                        st,
    std::string_view                     pluginName,
    std::string_view                     placeholderName,
    const std::string&                   paramString,
    const PlaceholderContext&            ctx,
    bool                                 replaced
) {
    std::string formatted_val;
    if (replaced) {
        formatted_val = PA::Utils::applyFormatting(originalResult, params);
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
        if (type == PlaceholderRegistry::PlaceholderType::Relational) typeStr = "Relational";
        else if (type == PlaceholderRegistry::PlaceholderType::Context) typeStr = "Context";
        else if (type == PlaceholderRegistry::PlaceholderType::Server) typeStr = "Server";
        else if (type == PlaceholderRegistry::PlaceholderType::ListContext) typeStr = "ListContext";
        else if (type == PlaceholderRegistry::PlaceholderType::ListServer) typeStr = "ListServer";
        else if (type == PlaceholderRegistry::PlaceholderType::ObjectListContext) typeStr = "ObjectListContext";
        else if (type == PlaceholderRegistry::PlaceholderType::ObjectListServer) typeStr = "ObjectListServer";

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

    // 1. 查找最匹配的替换器
    PlaceholderRegistry::ReplacerMatch match = mRegistry->findBestReplacer(pluginName, placeholderName, ctx);

    // 2. 构建缓存键并检查缓存
    std::string cacheKey;
    std::optional<CacheDuration> cacheDuration = cache_duration_override;
    bool hasCandidate = match.type != PlaceholderRegistry::PlaceholderType::None;

    if (hasCandidate) {
        cacheKey = buildCacheKey(ctx, pluginName, placeholderName, paramString, match.strategy);

        if (!cacheDuration) {
            cacheDuration = match.cacheDuration;
        }

        auto itCache = st.cache.find(cacheKey);
        if (itCache != st.cache.end()) {
            if (ConfigManager::getInstance().get().debugMode) {
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                                    std::chrono::high_resolution_clock::now() - startTime
                )
                                    .count();
                logger.info(
                    "Placeholder '{}:{}' | Cache Hit: Yes (local) | Type: {} | Time: {}us",
                    std::string(pluginName),
                    std::string(placeholderName),
                    static_cast<int>(match.type), // Log type as int for now
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
                                        std::chrono::high_resolution_clock::now() - startTime
                    )
                                        .count();
                    logger.info(
                        "Placeholder '{}:{}' | Cache Hit: Yes (global) | Type: {} | Time: {}us",
                        std::string(pluginName),
                        std::string(placeholderName),
                        static_cast<int>(match.type), // Log type as int for now
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

    // 3. Request Coalescing: Check if another thread is already computing this placeholder.
    {
        std::lock_guard<std::mutex> lock(mFuturesMutex);
        auto                        fit = mComputingFutures.find(cacheKey);
        if (fit != mComputingFutures.end()) {
            // Another thread is computing. Wait for its result.
            auto future = fit->second;
            lock.~lock_guard(); // Unlock before waiting
            if (ConfigManager::getInstance().get().debugMode) {
                logger.info("Placeholder '{}:{}' is being computed by another thread, waiting...", std::string(pluginName), std::string(placeholderName));
            }
            return future.get(); // This might throw, which is acceptable.
        }
    }

    // 4. If not cached and not being computed, compute it now.
    std::promise<std::string> promise;
    {
        std::lock_guard<std::mutex> lock(mFuturesMutex);
        mComputingFutures[cacheKey] = promise.get_future().share();
    }

    try {
        // 5. 获取解析后的参数
        std::shared_ptr<Utils::ParsedParams> paramsPtr = getParsedParams(paramString);
        const auto&                          params    = *paramsPtr;
        bool                                 allowEmpty = params.getBool("allowempty").value_or(false);

        // 6. 执行替换器
        void* p = ctx.ptr;
        for (auto& cfun : match.chain) {
            p = cfun(p);
        }
        void* p_rel = nullptr;
        if (ctx.relationalContext) {
            p_rel = ctx.relationalContext->ptr;
            for (auto& cfun : match.relationalChain) {
                p_rel = cfun(p_rel);
            }
        }

        std::string originalResult = executeFoundReplacer(match, p, p_rel, params, allowEmpty, st);
        bool        replaced       = !originalResult.empty() || allowEmpty;

        // 7. 应用格式化、处理缓存和日志记录
        std::string finalResult = applyFormattingAndCache(
            originalResult,
            params,
            defaultText,
            allowEmpty,
            cacheKey,
            cacheDuration,
            match.type,
            startTime,
            st,
            pluginName,
            placeholderName,
            paramString,
            ctx,
            replaced
        );

        promise.set_value(finalResult);
        {
            std::lock_guard<std::mutex> lock(mFuturesMutex);
            mComputingFutures.erase(cacheKey);
        }
        return finalResult;

    } catch (...) {
        // If any exception occurs during computation, set the exception on the promise
        // and remove the future from the map to allow retries.
        promise.set_exception(std::current_exception());
        {
            std::lock_guard<std::mutex> lock(mFuturesMutex);
            mComputingFutures.erase(cacheKey);
        }
        throw; // Re-throw the exception for the current thread.
    }
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
    return mAsyncThreadPool->enqueue([this,
                                      pluginName = std::string(pluginName),
                                      placeholderName = std::string(placeholderName),
                                      paramString,
                                      defaultText,
                                      ctx,
                                      cache_duration_override]() -> std::string {
        ReplaceState st; // Create a state for this async execution
        // We are now on an async thread, so we can execute synchronously.
        // The overall operation remains asynchronous from the caller's perspective.
        return executePlaceholder(
            pluginName,
            placeholderName,
            paramString,
            defaultText,
            ctx,
            st,
            cache_duration_override
        );
    });
}

// [新] 使用编译模板进行替换
std::string PlaceholderManager::replacePlaceholders(const CompiledTemplate& tpl, const PlaceholderContext& ctx) {
    return replacePlaceholdersSync(tpl, ctx, 0);
}

} // namespace PA
