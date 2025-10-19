// src/PA/PlaceholderRegistry.cpp
#include "PA/PlaceholderRegistry.h"
#include "PA/ParameterParser.h"      // For splitParamString
#include "PA/PlaceholderProcessor.h" // 用于在别名占位符内部二次解析内层表达式

namespace PA {

PlaceholderRegistry::PlaceholderRegistry() : mSnapshot(std::make_shared<const Snapshot>()) {}

void PlaceholderRegistry::registerPlaceholder(
    std::string_view                    prefix,
    std::shared_ptr<const IPlaceholder> p,
    void*                               owner
) {
    if (!p) return;

    std::string      key;
    std::string_view token_sv = p->token();

    std::string inner_token_str;
    if (token_sv.length() > 2 && token_sv.front() == '{' && token_sv.back() == '}') {
        inner_token_str = token_sv.substr(1, token_sv.length() - 2);
    } else {
        inner_token_str = token_sv;
    }

    if (prefix.empty()) {
        key = inner_token_str;
    } else {
        key = std::string(prefix) + ":" + inner_token_str;
    }

    std::lock_guard<std::mutex> lk(mWriteMutex);
    auto                        newSnapshot = std::make_shared<Snapshot>(*mSnapshot.load());

    const uint64_t ctxId = p->contextTypeId();
    if (ctxId == kServerContextId) {
        newSnapshot->server[key] = {p, owner};
        newSnapshot->ownerIndex[owner].push_back({true, false, false, false, 0, 0, 0, key}
        ); // isServer, isRelational, isCached, isAdapter
    } else {
        newSnapshot->typed[ctxId][key] = {p, owner};
        newSnapshot->ownerIndex[owner].push_back({false, false, false, false, 0, 0, ctxId, key}
        ); // isServer, isRelational, isCached, isAdapter
    }
    mSnapshot.store(newSnapshot);
}

void PlaceholderRegistry::registerCachedPlaceholder(
    std::string_view                    prefix,
    std::shared_ptr<const IPlaceholder> p,
    void*                               owner,
    unsigned int                        cacheDuration
) {
    if (!p || cacheDuration == 0) return; // 缓存持续时间为0表示不缓存

    std::string      key;
    std::string_view token_sv = p->token();

    std::string inner_token_str;
    if (token_sv.length() > 2 && token_sv.front() == '{' && token_sv.back() == '}') {
        inner_token_str = token_sv.substr(1, token_sv.length() - 2);
    } else {
        inner_token_str = token_sv;
    }

    if (prefix.empty()) {
        key = inner_token_str;
    } else {
        key = std::string(prefix) + ":" + inner_token_str;
    }

    std::lock_guard<std::mutex> lk(mWriteMutex);
    auto                        newSnapshot = std::make_shared<Snapshot>(*mSnapshot.load());

    const uint64_t ctxId = p->contextTypeId();
    if (ctxId == kServerContextId) {
        newSnapshot->cached_server[key] = {p, owner, cacheDuration, "", std::chrono::steady_clock::time_point()};
        newSnapshot->ownerIndex[owner].push_back({true, false, true, false, 0, 0, 0, key}
        ); // isServer, isRelational, isCached, isAdapter
    } else {
        newSnapshot->cached_typed[ctxId][key] = {p, owner, cacheDuration, "", std::chrono::steady_clock::time_point()};
        newSnapshot->ownerIndex[owner].push_back({false, false, true, false, 0, 0, ctxId, key}
        ); // isServer, isRelational, isCached, isAdapter
    }
    mSnapshot.store(newSnapshot);
}


void PlaceholderRegistry::registerRelationalPlaceholder(
    std::string_view                    prefix,
    std::shared_ptr<const IPlaceholder> p,
    void*                               owner,
    uint64_t                            mainContextTypeId,
    uint64_t                            relationalContextTypeId
) {
    if (!p) return;

    std::string      key;
    std::string_view token_sv = p->token();

    std::string inner_token_str;
    if (token_sv.length() > 2 && token_sv.front() == '{' && token_sv.back() == '}') {
        inner_token_str = token_sv.substr(1, token_sv.length() - 2);
    } else {
        inner_token_str = token_sv;
    }

    if (prefix.empty()) {
        key = inner_token_str;
    } else {
        key = std::string(prefix) + ":" + inner_token_str;
    }

    std::lock_guard<std::mutex> lk(mWriteMutex);
    auto                        newSnapshot = std::make_shared<Snapshot>(*mSnapshot.load());

    newSnapshot->relational[mainContextTypeId][relationalContextTypeId][key] = {p, owner};
    newSnapshot->ownerIndex[owner].push_back(
        {false, true, false, false, mainContextTypeId, relationalContextTypeId, 0, key}
    ); // isServer, isRelational, isCached, isAdapter
    mSnapshot.store(newSnapshot);
}

void PlaceholderRegistry::registerContextAlias(
    std::string_view  alias,
    uint64_t          fromContextTypeId,
    uint64_t          toContextTypeId,
    ContextResolverFn resolver,
    void*             owner
) {
    if (alias.empty() || !resolver) return;

    std::lock_guard<std::mutex> lk(mWriteMutex);
    auto                        current = mSnapshot.load();
    auto                        snap    = std::make_shared<Snapshot>(*current);

    auto& vec = snap->adapters[std::string(alias)];
    vec.push_back(Adapter{fromContextTypeId, toContextTypeId, resolver, owner});

    // 记录 owner -> handle
    snap->ownerIndex[owner].push_back(
        {false, // isServer
         false, // isRelational
         false, // isCached
         true,  // isAdapter
         fromContextTypeId,
         toContextTypeId,
         0,
         std::string(alias)}
    );

    mSnapshot.store(snap);
}

void PlaceholderRegistry::unregisterByOwner(void* owner) {
    std::lock_guard<std::mutex> lk(mWriteMutex);
    auto                        currentSnapshot = mSnapshot.load();
    auto                        it              = currentSnapshot->ownerIndex.find(owner);
    if (it == currentSnapshot->ownerIndex.end()) return;

    auto newSnapshot = std::make_shared<Snapshot>(*currentSnapshot);

    for (const Handle& h : it->second) {
        if (h.isAdapter) {
            auto ait = newSnapshot->adapters.find(h.token);
            if (ait != newSnapshot->adapters.end()) {
                auto& vec = ait->second;
                vec.erase(
                    std::remove_if(
                        vec.begin(),
                        vec.end(),
                        [&](const Adapter& a) {
                            return a.owner == owner && a.fromCtxId == h.mainCtxId && a.toCtxId == h.relCtxId;
                        }
                    ),
                    vec.end()
                );
                if (vec.empty()) {
                    newSnapshot->adapters.erase(ait);
                }
            }
        } else if (h.isCached) {
            if (h.isServer) {
                auto sit = newSnapshot->cached_server.find(h.token);
                if (sit != newSnapshot->cached_server.end() && sit->second.owner == owner) {
                    newSnapshot->cached_server.erase(sit);
                }
            } else {
                auto tit = newSnapshot->cached_typed.find(h.ctxId);
                if (tit != newSnapshot->cached_typed.end()) {
                    auto pit = tit->second.find(h.token);
                    if (pit != tit->second.end() && pit->second.owner == owner) {
                        tit->second.erase(pit);
                        if (tit->second.empty()) newSnapshot->cached_typed.erase(tit);
                    }
                }
            }
        } else if (h.isServer) {
            auto sit = newSnapshot->server.find(h.token);
            if (sit != newSnapshot->server.end() && sit->second.owner == owner) {
                newSnapshot->server.erase(sit);
            }
        } else if (h.isRelational) {
            auto mainIt = newSnapshot->relational.find(h.mainCtxId);
            if (mainIt != newSnapshot->relational.end()) {
                auto relIt = mainIt->second.find(h.relCtxId);
                if (relIt != mainIt->second.end()) {
                    auto pit = relIt->second.find(h.token);
                    if (pit != relIt->second.end() && pit->second.owner == owner) {
                        relIt->second.erase(pit);
                        if (relIt->second.empty()) mainIt->second.erase(relIt);
                        if (mainIt->second.empty()) newSnapshot->relational.erase(mainIt);
                    }
                }
            }
        } else { // Typed (non-cached)
            auto tit = newSnapshot->typed.find(h.ctxId);
            if (tit != newSnapshot->typed.end()) {
                auto pit = tit->second.find(h.token);
                if (pit != tit->second.end() && pit->second.owner == owner) {
                    tit->second.erase(pit);
                    if (tit->second.empty()) newSnapshot->typed.erase(tit);
                }
            }
        }
    }
    newSnapshot->ownerIndex.erase(owner);
    mSnapshot.store(newSnapshot);
}

std::vector<std::pair<std::string, std::shared_ptr<const IPlaceholder>>>
PlaceholderRegistry::getTypedPlaceholders(const IContext* ctx) const {
    auto                                                                     snapshot = mSnapshot.load();
    std::vector<std::pair<std::string, std::shared_ptr<const IPlaceholder>>> typedList;
    if (!ctx) return typedList;

    std::unordered_map<std::string, std::shared_ptr<const IPlaceholder>> tempTypedMap;
    std::vector<uint64_t>                                                inheritedTypeIds = ctx->getInheritedTypeIds();
    std::reverse(inheritedTypeIds.begin(), inheritedTypeIds.end()); // 反转顺序，派生类在前

    for (uint64_t id : inheritedTypeIds) {
        auto tit = snapshot->typed.find(id);
        if (tit != snapshot->typed.end()) {
            for (auto& kv : tit->second) {
                tempTypedMap.try_emplace(kv.first, kv.second.ptr);
            }
        }
    }

    uint64_t mainCtxId = ctx->typeId();
    auto     mainIt    = snapshot->relational.find(mainCtxId);
    if (mainIt != snapshot->relational.end()) {
        for (uint64_t relId : inheritedTypeIds) { // 这里也需要反转，因为 relId 也是继承类型
            auto relIt = mainIt->second.find(relId);
            if (relIt != mainIt->second.end()) {
                for (auto& kv : relIt->second) {
                    tempTypedMap.try_emplace(kv.first, kv.second.ptr);
                }
            }
        }
    }

    typedList.reserve(tempTypedMap.size());
    for (auto& kv : tempTypedMap) {
        typedList.emplace_back(kv.first, kv.second);
    }

    return typedList;
}

std::vector<std::pair<std::string, std::shared_ptr<const IPlaceholder>>>
PlaceholderRegistry::getServerPlaceholders() const {
    auto                                                                     snapshot = mSnapshot.load();
    std::vector<std::pair<std::string, std::shared_ptr<const IPlaceholder>>> serverList;
    serverList.reserve(snapshot->server.size());
    for (auto& kv : snapshot->server) {
        serverList.emplace_back(kv.first, kv.second.ptr);
    }
    return serverList;
}

// 动态“别名占位符”，把来源上下文适配为目标上下文，并在目标上下文下再次解析内层表达式
class AdapterAliasPlaceholder final : public IPlaceholder {
public:
    AdapterAliasPlaceholder(
        std::string                alias,
        uint64_t                   fromId,
        uint64_t                   toId,
        ContextResolverFn          resolver,
        const PlaceholderRegistry& reg
    )
    : mAlias(std::move(alias)),
      mFrom(fromId),
      mTo(toId),
      mResolver(resolver),
      mReg(reg) {}

    std::string_view token() const noexcept override { return mAlias; }
    uint64_t         contextTypeId() const noexcept override { return mFrom; }

    void evaluate(const IContext* ctx, std::string& out) const override {
        // 无参数时无法知道要复用哪个内层占位符
        out.clear();
    }

    void
    evaluateWithArgs(const IContext* ctx, const std::vector<std::string_view>& args, std::string& out) const override {
        out.clear();
        if (!ctx || !mResolver) return;
        if (args.empty()) {
            out = PA_COLOR_RED "Usage: {" + mAlias + ":<inner_placeholder_spec>}" PA_COLOR_RESET;
            return;
        }

        // 合并所有参数为一个字符串，因为原始参数部分可能包含逗号
        std::string full_param_part;
        for (size_t i = 0; i < args.size(); ++i) {
            full_param_part.append(args[i]);
            if (i < args.size() - 1) {
                full_param_part.push_back(',');
            }
        }

        std::string_view              innerSpec_sv;
        std::vector<std::string_view> resolver_args;

        size_t last_colon_pos = full_param_part.rfind(':');
        if (last_colon_pos != std::string::npos) {
            std::string_view resolver_param_part = std::string_view(full_param_part).substr(0, last_colon_pos);
            innerSpec_sv                         = std::string_view(full_param_part).substr(last_colon_pos + 1);

            // 使用 splitParamString 来解析 resolver 的参数
            std::vector<std::string> resolver_params_str =
                ParameterParser::splitParamString(resolver_param_part, ',');
            // 注意：这里需要一个临时的 vector 来存储 string_view，因为 splitParamString 返回 vector<string>
            // 这是一个简化的处理，理想情况下需要避免 string 拷贝
            static thread_local std::vector<std::string> arg_storage;
            arg_storage = std::move(resolver_params_str);
            for (const auto& s : arg_storage) {
                resolver_args.push_back(s);
            }

        } else {
            innerSpec_sv = full_param_part;
        }

        // 1) 解析来源上下文 -> 目标底层对象指针
        void* raw = mResolver(ctx, resolver_args);
        if (!raw) {
            return;
        }

        // 2) 构造一个临时目标上下文对象（栈对象）
        // 3) 在目标上下文下对“内层占位符表达式”做一次完整解析
        std::string innerSpec(innerSpec_sv);
        std::string wrapped = "{" + innerSpec + "}";

        switch (mTo) {
        case ActorContext::kTypeId: {
            ActorContext rc;
            rc.actor = static_cast<Actor*>(raw);
            out      = PlaceholderProcessor::process(wrapped, &rc, mReg);
            break;
        }
        case MobContext::kTypeId: {
            MobContext rc;
            rc.mob   = static_cast<Mob*>(raw);
            rc.actor = static_cast<Actor*>(raw); // MobContext 继承自 ActorContext
            out      = PlaceholderProcessor::process(wrapped, &rc, mReg);
            break;
        }
        case PlayerContext::kTypeId: {
            PlayerContext rc;
            rc.player = static_cast<Player*>(raw);
            rc.mob    = static_cast<Mob*>(raw);
            rc.actor  = static_cast<Actor*>(raw);
            out       = PlaceholderProcessor::process(wrapped, &rc, mReg);
            break;
        }
        case BlockContext::kTypeId: {
            BlockContext rc;
            rc.block = static_cast<const Block*>(raw); // raw 是 void*，需要转换为 const Block*
            out      = PlaceholderProcessor::process(wrapped, &rc, mReg);
            break;
        }
        default:
            // 其他目标上下文类型：暂不支持
            break;
        }
    }

private:
    std::string                mAlias;
    uint64_t                   mFrom{};
    uint64_t                   mTo{};
    ContextResolverFn          mResolver{};
    const PlaceholderRegistry& mReg;
};

std::pair<std::shared_ptr<const IPlaceholder>, const CachedEntry*>
PlaceholderRegistry::findPlaceholder(const std::string& token, const IContext* ctx) const {
    auto snapshot = mSnapshot.load();

    // 1. Check cached server placeholders
    auto cachedServerIt = snapshot->cached_server.find(token);
    if (cachedServerIt != snapshot->cached_server.end()) {
        return std::make_pair(cachedServerIt->second.ptr, &cachedServerIt->second);
    }

    // 2. Check non-cached server placeholders
    auto serverIt = snapshot->server.find(token);
    if (serverIt != snapshot->server.end()) {
        return {serverIt->second.ptr, nullptr};
    }

    if (ctx) {
        auto inheritedTypeIds = ctx->getInheritedTypeIds();
        std::reverse(inheritedTypeIds.begin(), inheritedTypeIds.end()); // 反转顺序，派生类在前

        // 2.5 Check context alias (adapter) by token == alias
        {
            auto ait = snapshot->adapters.find(token);
            if (ait != snapshot->adapters.end()) {
                // 选择与当前上下文匹配的 fromCtxId（最派生优先）
                for (uint64_t id : inheritedTypeIds) {
                    for (const auto& ad : ait->second) {
                        if (ad.fromCtxId == id) {
                            auto aliasPh = std::make_shared<AdapterAliasPlaceholder>(
                                token,
                                ad.fromCtxId,
                                ad.toCtxId,
                                ad.resolver,
                                *this
                            );
                            return {aliasPh, nullptr};
                        }
                    }
                }
            }
        }

        // 3. Check cached typed placeholders
        for (uint64_t id : inheritedTypeIds) {
            auto it = snapshot->cached_typed.find(id);
            if (it != snapshot->cached_typed.end()) {
                auto placeholderIt = it->second.find(token);
                if (placeholderIt != it->second.end()) {
                    return std::make_pair(placeholderIt->second.ptr, &placeholderIt->second);
                }
            }
        }

        // 4. Check non-cached typed placeholders
        for (uint64_t id : inheritedTypeIds) {
            auto it = snapshot->typed.find(id);
            if (it != snapshot->typed.end()) {
                auto placeholderIt = it->second.find(token);
                if (placeholderIt != it->second.end()) {
                    return {placeholderIt->second.ptr, nullptr};
                }
            }
        }

        // 5. Check relational placeholders (relational placeholders are not cached in this design)
        uint64_t mainCtxId = ctx->typeId();
        auto     mainIt    = snapshot->relational.find(mainCtxId);
        if (mainIt != snapshot->relational.end()) {
            for (uint64_t relId : inheritedTypeIds) {
                auto relIt = mainIt->second.find(relId);
                if (relIt != mainIt->second.end()) {
                    auto placeholderIt = relIt->second.find(token);
                    if (placeholderIt != relIt->second.end()) {
                        return {placeholderIt->second.ptr, nullptr};
                    }
                }
            }
        }
    }

    return {nullptr, nullptr};
}

} // namespace PA
