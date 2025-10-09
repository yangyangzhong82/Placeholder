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
        newSnapshot->ownerIndex[owner].push_back({true, false, 0, 0, 0, key});
    } else {
        newSnapshot->typed[ctxId][key] = {p, owner};
        newSnapshot->ownerIndex[owner].push_back({false, false, 0, 0, ctxId, key});
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
    newSnapshot->ownerIndex[owner].push_back({false, true, mainContextTypeId, relationalContextTypeId, 0, key});
    mSnapshot.store(newSnapshot);
}

void PlaceholderRegistry::unregisterByOwner(void* owner) {
    std::lock_guard<std::mutex> lk(mWriteMutex);
    auto currentSnapshot = mSnapshot.load();
    auto it = currentSnapshot->ownerIndex.find(owner);
    if (it == currentSnapshot->ownerIndex.end()) return;

    auto newSnapshot = std::make_shared<Snapshot>(*currentSnapshot);

    for (const Handle& h : it->second) {
        if (h.isServer) {
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
        } else {
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
        for (uint64_t relId : inheritedTypeIds) {
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

std::shared_ptr<const IPlaceholder>
PlaceholderRegistry::findPlaceholder(const std::string& token, const IContext* ctx) const {
    auto snapshot = mSnapshot.load();

    // 1. Check server placeholders (case-insensitive for token)
    for (const auto& kv : snapshot->server) {
        if (_stricmp(kv.first.c_str(), token.c_str()) == 0) {
            return kv.second.ptr;
        }
    }

    if (ctx) {
        // 2. Check typed placeholders
        auto inheritedTypeIds = ctx->getInheritedTypeIds();
        for (uint64_t id : inheritedTypeIds) {
            auto it = snapshot->typed.find(id);
            if (it != snapshot->typed.end()) {
                for (const auto& kv : it->second) {
                    if (_stricmp(kv.first.c_str(), token.c_str()) == 0) {
                        return kv.second.ptr;
                    }
                }
            }
        }

        // 3. Check relational placeholders
        uint64_t mainCtxId = ctx->typeId();
        auto     mainIt    = snapshot->relational.find(mainCtxId);
        if (mainIt != snapshot->relational.end()) {
            for (uint64_t relId : inheritedTypeIds) {
                auto relIt = mainIt->second.find(relId);
                if (relIt != mainIt->second.end()) {
                    for (const auto& kv : relIt->second) {
                        if (_stricmp(kv.first.c_str(), token.c_str()) == 0) {
                            return kv.second.ptr;
                        }
                    }
                }
            }
        }
    }

    return nullptr;
}

} // namespace PA
