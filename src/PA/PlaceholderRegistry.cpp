// src/PA/PlaceholderRegistry.cpp
#include "PA/PlaceholderRegistry.h"

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
    auto newSnapshot = std::make_shared<Snapshot>(*mSnapshot.load());

    const uint64_t ctxId = p->contextTypeId();
    if (ctxId == kServerContextId) {
        newSnapshot->server[key] = {p, owner};
        newSnapshot->ownerIndex[owner].push_back({true, false, false, 0, 0, 0, key}); // isServer, isRelational, isCached
    } else {
        newSnapshot->typed[ctxId][key] = {p, owner};
        newSnapshot->ownerIndex[owner].push_back({false, false, false, 0, 0, ctxId, key}); // isServer, isRelational, isCached
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
    auto newSnapshot = std::make_shared<Snapshot>(*mSnapshot.load());

    const uint64_t ctxId = p->contextTypeId();
    if (ctxId == kServerContextId) {
        newSnapshot->cached_server[key] = {p, owner, cacheDuration, "", std::chrono::steady_clock::time_point()};
        newSnapshot->ownerIndex[owner].push_back({true, false, true, 0, 0, 0, key}); // isServer, isRelational, isCached
    } else {
        newSnapshot->cached_typed[ctxId][key] = {p, owner, cacheDuration, "", std::chrono::steady_clock::time_point()};
        newSnapshot->ownerIndex[owner].push_back({false, false, true, 0, 0, ctxId, key}); // isServer, isRelational, isCached
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
    auto newSnapshot = std::make_shared<Snapshot>(*mSnapshot.load());

    newSnapshot->relational[mainContextTypeId][relationalContextTypeId][key] = {p, owner};
    newSnapshot->ownerIndex[owner].push_back({false, true, false, mainContextTypeId, relationalContextTypeId, 0, key}); // isServer, isRelational, isCached
    mSnapshot.store(newSnapshot);
}

void PlaceholderRegistry::unregisterByOwner(void* owner) {
    std::lock_guard<std::mutex> lk(mWriteMutex);
    auto currentSnapshot = mSnapshot.load();
    auto it = currentSnapshot->ownerIndex.find(owner);
    if (it == currentSnapshot->ownerIndex.end()) return;

    auto newSnapshot = std::make_shared<Snapshot>(*currentSnapshot);

    for (const Handle& h : it->second) {
        if (h.isCached) {
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
    auto snapshot = mSnapshot.load();
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
    auto snapshot = mSnapshot.load();
    std::vector<std::pair<std::string, std::shared_ptr<const IPlaceholder>>> serverList;
    serverList.reserve(snapshot->server.size());
    for (auto& kv : snapshot->server) {
        serverList.emplace_back(kv.first, kv.second.ptr);
    }
    return serverList;
}

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
