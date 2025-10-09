// src/PA/PlaceholderRegistry.cpp
#include "PA/PlaceholderRegistry.h"

namespace PA {

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

    std::unique_lock<std::shared_mutex> lk(mMutex);
    const uint64_t                      ctxId = p->contextTypeId();

    if (ctxId == kServerContextId) {
        mServer[key] = {p, owner};
        mOwnerIndex[owner].push_back({true, false, 0, 0, 0, key});
    } else {
        mTyped[ctxId][key] = {p, owner};
        mOwnerIndex[owner].push_back({false, false, 0, 0, ctxId, key});
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

    std::unique_lock<std::shared_mutex> lk(mMutex);
    mRelational[mainContextTypeId][relationalContextTypeId][key] = {p, owner};
    mOwnerIndex[owner].push_back({false, true, mainContextTypeId, relationalContextTypeId, 0, key});
}

void PlaceholderRegistry::unregisterByOwner(void* owner) {
    std::unique_lock<std::shared_mutex> lk(mMutex);
    auto                                it = mOwnerIndex.find(owner);
    if (it == mOwnerIndex.end()) return;

    for (const Handle& h : it->second) {
        if (h.isServer) {
            auto sit = mServer.find(h.token);
            if (sit != mServer.end() && sit->second.owner == owner) {
                mServer.erase(sit);
            }
        } else if (h.isRelational) {
            auto mainIt = mRelational.find(h.mainCtxId);
            if (mainIt != mRelational.end()) {
                auto relIt = mainIt->second.find(h.relCtxId);
                if (relIt != mainIt->second.end()) {
                    auto pit = relIt->second.find(h.token);
                    if (pit != relIt->second.end() && pit->second.owner == owner) {
                        relIt->second.erase(pit);
                        if (relIt->second.empty()) mainIt->second.erase(relIt);
                        if (mainIt->second.empty()) mRelational.erase(mainIt);
                    }
                }
            }
        } else {
            auto tit = mTyped.find(h.ctxId);
            if (tit != mTyped.end()) {
                auto pit = tit->second.find(h.token);
                if (pit != tit->second.end() && pit->second.owner == owner) {
                    tit->second.erase(pit);
                    if (tit->second.empty()) mTyped.erase(tit);
                }
            }
        }
    }
    mOwnerIndex.erase(it);
}

std::vector<std::pair<std::string, std::shared_ptr<const IPlaceholder>>>
PlaceholderRegistry::getTypedPlaceholders(const IContext* ctx) const {
    std::vector<std::pair<std::string, std::shared_ptr<const IPlaceholder>>> typedList;
    if (!ctx) return typedList;

    std::shared_lock<std::shared_mutex> lk(mMutex);

    std::unordered_map<std::string, std::shared_ptr<const IPlaceholder>> tempTypedMap;
    std::vector<uint64_t>                                                inheritedTypeIds = ctx->getInheritedTypeIds();

    for (uint64_t id : inheritedTypeIds) {
        auto tit = mTyped.find(id);
        if (tit != mTyped.end()) {
            for (auto& kv : tit->second) {
                tempTypedMap.try_emplace(kv.first, kv.second.ptr);
            }
        }
    }

    uint64_t mainCtxId = ctx->typeId();
    auto     mainIt    = mRelational.find(mainCtxId);
    if (mainIt != mRelational.end()) {
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
    std::vector<std::pair<std::string, std::shared_ptr<const IPlaceholder>>> serverList;
    std::shared_lock<std::shared_mutex>                                      lk(mMutex);
    serverList.reserve(mServer.size());
    for (auto& kv : mServer) {
        serverList.emplace_back(kv.first, kv.second.ptr);
    }
    return serverList;
}

std::shared_ptr<const IPlaceholder>
PlaceholderRegistry::findPlaceholder(const std::string& token, const IContext* ctx) const {
    std::shared_lock<std::shared_mutex> lk(mMutex);

    // 1. Check server placeholders (case-insensitive for token)
    for (const auto& kv : mServer) {
        if (_stricmp(kv.first.c_str(), token.c_str()) == 0) {
            return kv.second.ptr;
        }
    }

    if (ctx) {
        // 2. Check typed placeholders
        auto inheritedTypeIds = ctx->getInheritedTypeIds();
        for (uint64_t id : inheritedTypeIds) {
            auto it = mTyped.find(id);
            if (it != mTyped.end()) {
                for (const auto& kv : it->second) {
                    if (_stricmp(kv.first.c_str(), token.c_str()) == 0) {
                        return kv.second.ptr;
                    }
                }
            }
        }

        // 3. Check relational placeholders
        uint64_t mainCtxId = ctx->typeId();
        auto     mainIt    = mRelational.find(mainCtxId);
        if (mainIt != mRelational.end()) {
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
