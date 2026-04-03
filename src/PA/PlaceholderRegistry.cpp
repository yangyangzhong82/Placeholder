// src/PA/PlaceholderRegistry.cpp
#include "PA/PlaceholderRegistry.h"
#include "PA/AdapterAliasPlaceholder.h"

namespace PA {

PlaceholderRegistry::PlaceholderRegistry() : mSnapshot(std::make_shared<const Snapshot>()) {}

void PlaceholderRegistry::registerPlaceholder(
    std::string_view                    prefix,
    std::shared_ptr<const IPlaceholder> p,
    void*                               owner
) {
    if (!p) return;

    unsigned int cacheDuration = p->getCacheDuration();

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
    if (cacheDuration > 0) {
        CachedEntry entry;
        entry.ptr = p;
        entry.owner = owner;
        entry.cacheDuration = cacheDuration;
        if (ctxId == kServerContextId) {
            newSnapshot->cached_server.emplace(key, std::move(entry));
            newSnapshot->ownerIndex[owner].push_back({true, false, true, false, false, 0, 0, 0, key});
        } else {
            newSnapshot->cached_typed[ctxId].emplace(key, std::move(entry));
            newSnapshot->ownerIndex[owner].push_back({false, false, true, false, false, 0, 0, ctxId, key});
        }
    } else {
        if (ctxId == kServerContextId) {
            newSnapshot->server[key] = {p, owner};
            newSnapshot->ownerIndex[owner].push_back({true, false, false, false, false, 0, 0, 0, key});
        } else {
            newSnapshot->typed[ctxId][key] = {p, owner};
            newSnapshot->ownerIndex[owner].push_back({false, false, false, false, false, 0, 0, ctxId, key});
        }
    }
    mSnapshot.store(newSnapshot);
}

void PlaceholderRegistry::registerCachedPlaceholder(
    std::string_view                    prefix,
    std::shared_ptr<const IPlaceholder> p,
    void*                               owner,
    unsigned int /* cacheDuration */
) {
    // This is now a convenience function that forwards to the main registration logic.
    // Note: The `cacheDuration` parameter here is technically redundant if the placeholder
    // itself returns a valid duration from `getCacheDuration()`. The primary mechanism
    // should be the placeholder's own method. This API is kept for compatibility.
    if (!p) return;
    if (p->getCacheDuration() > 0) {
        registerPlaceholder(prefix, p, owner);
    } else {
        // If the placeholder itself doesn't specify a cache duration, we can't treat it as cached.
        // Forcing it into the cached map would be inconsistent.
        // We will register it as a normal placeholder instead.
        registerPlaceholder(prefix, p, owner);
    }
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
        {false, true, false, false, false, mainContextTypeId, relationalContextTypeId, 0, key}
    ); // isServer, isRelational, isCached, isAdapter, isFactory
    mSnapshot.store(newSnapshot);
}

void PlaceholderRegistry::registerCachedRelationalPlaceholder(
    std::string_view                    prefix,
    std::shared_ptr<const IPlaceholder> p,
    void*                               owner,
    uint64_t                            mainContextTypeId,
    uint64_t                            relationalContextTypeId,
    unsigned int                        cacheDuration
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

    CachedEntry entry;
    entry.ptr           = p;
    entry.owner         = owner;
    entry.cacheDuration = cacheDuration;

    newSnapshot->cached_relational[mainContextTypeId][relationalContextTypeId].emplace(key, std::move(entry));
    newSnapshot->ownerIndex[owner].push_back(
        {false, true, true, false, false, mainContextTypeId, relationalContextTypeId, 0, key}
    ); // isServer, isRelational, isCached, isAdapter, isFactory
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
         false, // isFactory
         fromContextTypeId,
         toContextTypeId,
         0,
         std::string(alias)}
    );

    mSnapshot.store(snap);
}

void PlaceholderRegistry::registerContextFactory(uint64_t contextTypeId, ContextFactoryFn factory, void* owner) {
    if (!factory) return;

    std::lock_guard<std::mutex> lk(mWriteMutex);
    auto                        current = mSnapshot.load();
    auto                        snap    = std::make_shared<Snapshot>(*current);

    snap->contextFactories[contextTypeId] = {factory, owner};

    // 记录 owner -> handle
    snap->ownerIndex[owner].push_back(
        {false, // isServer
         false, // isRelational
         false, // isCached
         false, // isAdapter
         true,  // isFactory
         0,
         0,
         contextTypeId,
         std::to_string(contextTypeId)}
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
        if (h.isFactory) {
            auto fit = newSnapshot->contextFactories.find(h.ctxId);
            if (fit != newSnapshot->contextFactories.end() && fit->second.owner == owner) {
                newSnapshot->contextFactories.erase(fit);
            }
        } else if (h.isAdapter) {
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
            } else if (h.isRelational) {
                auto mainIt = newSnapshot->cached_relational.find(h.mainCtxId);
                if (mainIt != newSnapshot->cached_relational.end()) {
                    auto relIt = mainIt->second.find(h.relCtxId);
                    if (relIt != mainIt->second.end()) {
                        auto pit = relIt->second.find(h.token);
                        if (pit != relIt->second.end() && pit->second.owner == owner) {
                            relIt->second.erase(pit);
                            if (relIt->second.empty()) mainIt->second.erase(relIt);
                            if (mainIt->second.empty()) newSnapshot->cached_relational.erase(mainIt);
                        }
                    }
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

LookupResult PlaceholderRegistry::findPlaceholder(const std::string& token, const IContext* ctx) const {
    auto snapshot = mSnapshot.load();

    // 1. Check cached server placeholders
    auto cachedServerIt = snapshot->cached_server.find(token);
    if (cachedServerIt != snapshot->cached_server.end()) {
        return {cachedServerIt->second.ptr, &cachedServerIt->second, snapshot};
    }

    // 2. Check non-cached server placeholders
    auto serverIt = snapshot->server.find(token);
    if (serverIt != snapshot->server.end()) {
        return {serverIt->second.ptr, nullptr, snapshot};
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
                            return {aliasPh, nullptr, snapshot};
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
                    return {placeholderIt->second.ptr, &placeholderIt->second, snapshot};
                }
            }
        }

        // 4. Check non-cached typed placeholders
        for (uint64_t id : inheritedTypeIds) {
            auto it = snapshot->typed.find(id);
            if (it != snapshot->typed.end()) {
                auto placeholderIt = it->second.find(token);
                if (placeholderIt != it->second.end()) {
                    return {placeholderIt->second.ptr, nullptr, snapshot};
                }
            }
        }

        // 5. Check cached relational placeholders
        uint64_t mainCtxId = ctx->typeId();
        auto     mainIt    = snapshot->cached_relational.find(mainCtxId);
        if (mainIt != snapshot->cached_relational.end()) {
            for (uint64_t relId : inheritedTypeIds) {
                auto relIt = mainIt->second.find(relId);
                if (relIt != mainIt->second.end()) {
                    auto placeholderIt = relIt->second.find(token);
                    if (placeholderIt != relIt->second.end()) {
                        return {placeholderIt->second.ptr, &placeholderIt->second, snapshot};
                    }
                }
            }
        }

        // 6. Check non-cached relational placeholders
        auto mainItNonCached = snapshot->relational.find(mainCtxId);
        if (mainItNonCached != snapshot->relational.end()) {
            for (uint64_t relId : inheritedTypeIds) {
                auto relIt = mainItNonCached->second.find(relId);
                if (relIt != mainItNonCached->second.end()) {
                    auto placeholderIt = relIt->second.find(token);
                    if (placeholderIt != relIt->second.end()) {
                        return {placeholderIt->second.ptr, nullptr, snapshot};
                    }
                }
            }
        }
    }

    // 7. Check for ContainerContext (if applicable)
    if (ctx && ctx->typeId() == ContainerContext::kTypeId) {
        auto containerCtx = static_cast<const ContainerContext*>(ctx);
        // Add specific logic for ContainerContext if needed, e.g., looking up placeholders
        // related to the container's properties or contents.
        // For now, we'll just let it fall through to the default.
    }

    return {nullptr, nullptr, nullptr};
}

const Adapter* PlaceholderRegistry::findContextAlias(std::string_view alias, uint64_t fromContextTypeId) const {
    auto snapshot = mSnapshot.load();
    auto it       = snapshot->adapters.find(std::string(alias)); // adapters 使用 std::string 作为 key
    if (it != snapshot->adapters.end()) {
        for (const auto& adapter : it->second) {
            if (adapter.fromCtxId == fromContextTypeId) {
                return &adapter;
            }
        }
    }
    return nullptr;
}

ContextFactoryFn PlaceholderRegistry::findContextFactory(uint64_t contextTypeId) const {
    auto snapshot = mSnapshot.load();
    auto it       = snapshot->contextFactories.find(contextTypeId);
    if (it != snapshot->contextFactories.end()) {
        return it->second.factory;
    }
    return nullptr;
}

// ScopedPlaceholderRegistrar implementation
ScopedPlaceholderRegistrar::~ScopedPlaceholderRegistrar() {
    if (mRegistry && mOwner) {
        mRegistry->unregisterByOwner(mOwner);
    }
}

void ScopedPlaceholderRegistrar::registerPlaceholder(
    std::string_view prefix, std::shared_ptr<const IPlaceholder> p
) {
    if (mRegistry) {
        mRegistry->registerPlaceholder(prefix, p, mOwner);
    }
}

void ScopedPlaceholderRegistrar::registerCachedPlaceholder(
    std::string_view                    prefix,
    std::shared_ptr<const IPlaceholder> p,
    unsigned int                        cacheDuration
) {
    if (mRegistry) {
        mRegistry->registerCachedPlaceholder(prefix, p, mOwner, cacheDuration);
    }
}

void ScopedPlaceholderRegistrar::registerRelationalPlaceholder(
    std::string_view                    prefix,
    std::shared_ptr<const IPlaceholder> p,
    uint64_t                            mainContextTypeId,
    uint64_t                            relationalContextTypeId
) {
    if (mRegistry) {
        mRegistry->registerRelationalPlaceholder(prefix, p, mOwner, mainContextTypeId, relationalContextTypeId);
    }
}

void ScopedPlaceholderRegistrar::registerCachedRelationalPlaceholder(
    std::string_view                    prefix,
    std::shared_ptr<const IPlaceholder> p,
    uint64_t                            mainContextTypeId,
    uint64_t                            relationalContextTypeId,
    unsigned int                        cacheDuration
) {
    if (mRegistry) {
        mRegistry->registerCachedRelationalPlaceholder(
            prefix,
            p,
            mOwner,
            mainContextTypeId,
            relationalContextTypeId,
            cacheDuration
        );
    }
}

void ScopedPlaceholderRegistrar::registerContextAlias(
    std::string_view  alias,
    uint64_t          fromContextTypeId,
    uint64_t          toContextTypeId,
    ContextResolverFn resolver
) {
    if (mRegistry) {
        mRegistry->registerContextAlias(alias, fromContextTypeId, toContextTypeId, resolver, mOwner);
    }
}

void ScopedPlaceholderRegistrar::registerContextFactory(uint64_t contextTypeId, ContextFactoryFn factory) {
    if (mRegistry) {
        mRegistry->registerContextFactory(contextTypeId, factory, mOwner);
    }
}

} // namespace PA
