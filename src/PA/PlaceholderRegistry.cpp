// src/PA/PlaceholderRegistry.cpp
#include "PA/PlaceholderRegistry.h"

namespace PA {

void PlaceholderRegistry::registerPlaceholder(std::string_view prefix, std::shared_ptr<const IPlaceholder> p, void* owner) {
    if (!p) return;

    std::string      key;
    std::string_view token_sv = p->token();

    if (prefix.empty()) {
        key = token_sv;
    } else {
        if (token_sv.length() > 2 && token_sv.front() == '{' && token_sv.back() == '}') {
            std::string_view inner_token = token_sv.substr(1, token_sv.length() - 2);
            key                          = "{" + std::string(prefix) + ":" + std::string(inner_token) + "}";
        } else {
            key = token_sv;
        }
    }

    std::lock_guard<std::mutex> lk(mMutex);
    const uint64_t              ctxId = p->contextTypeId();

    if (ctxId == kServerContextId) {
        mServer[key] = {p, owner};
        mOwnerIndex[owner].push_back({true, false, 0, 0, 0, key});
    } else {
        mTyped[ctxId][key] = {p, owner};
        mOwnerIndex[owner].push_back({false, false, 0, 0, ctxId, key});
    }
}

void PlaceholderRegistry::registerRelationalPlaceholder(std::string_view prefix, std::shared_ptr<const IPlaceholder> p, void* owner, uint64_t mainContextTypeId, uint64_t relationalContextTypeId) {
    if (!p) return;

    std::string      key;
    std::string_view token_sv = p->token();

    if (prefix.empty()) {
        key = token_sv;
    } else {
        if (token_sv.length() > 2 && token_sv.front() == '{' && token_sv.back() == '}') {
            std::string_view inner_token = token_sv.substr(1, token_sv.length() - 2);
            key                          = "{" + std::string(prefix) + ":" + std::string(inner_token) + "}";
        } else {
            key = token_sv;
        }
    }

    std::lock_guard<std::mutex> lk(mMutex);
    mRelational[mainContextTypeId][relationalContextTypeId][key] = {p, owner};
    mOwnerIndex[owner].push_back({false, true, mainContextTypeId, relationalContextTypeId, 0, key});
}

void PlaceholderRegistry::unregisterByOwner(void* owner) {
    std::lock_guard<std::mutex> lk(mMutex);
    auto                        it = mOwnerIndex.find(owner);
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

std::vector<std::pair<std::string, std::shared_ptr<const IPlaceholder>>> PlaceholderRegistry::getTypedPlaceholders(const IContext* ctx) const {
    std::vector<std::pair<std::string, std::shared_ptr<const IPlaceholder>>> typedList;
    if (!ctx) return typedList;

    std::lock_guard<std::mutex> lk(mMutex);
    
    std::unordered_map<std::string, std::shared_ptr<const IPlaceholder>> tempTypedMap;
    std::vector<uint64_t> inheritedTypeIds = ctx->getInheritedTypeIds();
    
    for (uint64_t id : inheritedTypeIds) {
        auto tit = mTyped.find(id);
        if (tit != mTyped.end()) {
            for (auto& kv : tit->second) {
                tempTypedMap.try_emplace(kv.first, kv.second.ptr);
            }
        }
    }

    uint64_t mainCtxId = ctx->typeId();
    auto mainIt = mRelational.find(mainCtxId);
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

std::vector<std::pair<std::string, std::shared_ptr<const IPlaceholder>>> PlaceholderRegistry::getServerPlaceholders() const {
    std::vector<std::pair<std::string, std::shared_ptr<const IPlaceholder>>> serverList;
    std::lock_guard<std::mutex> lk(mMutex);
    serverList.reserve(mServer.size());
    for (auto& kv : mServer) {
        serverList.emplace_back(kv.first, kv.second.ptr);
    }
    return serverList;
}

} // namespace PA
