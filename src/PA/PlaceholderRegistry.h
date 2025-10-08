// src/PA/PlaceholderRegistry.h
#pragma once

#include "PA/PlaceholderAPI.h"
#include <memory>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace PA {

class PlaceholderRegistry {
public:
    void registerPlaceholder(std::string_view prefix, std::shared_ptr<const IPlaceholder> p, void* owner);
    void registerRelationalPlaceholder(std::string_view prefix, std::shared_ptr<const IPlaceholder> p, void* owner, uint64_t mainContextTypeId, uint64_t relationalContextTypeId);
    void unregisterByOwner(void* owner);

    std::vector<std::pair<std::string, std::shared_ptr<const IPlaceholder>>> getTypedPlaceholders(const IContext* ctx) const;
    std::vector<std::pair<std::string, std::shared_ptr<const IPlaceholder>>> getServerPlaceholders() const;

    const IPlaceholder* findPlaceholder(const std::string& token, const IContext* ctx) const;

private:
    struct Entry {
        std::shared_ptr<const IPlaceholder> ptr{};
        void*                               owner{};
    };
    struct Handle {
        bool        isServer{};
        bool        isRelational{};
        uint64_t    mainCtxId{};
        uint64_t    relCtxId{};
        uint64_t    ctxId{};
        std::string token;
    };

    mutable std::shared_mutex                                                        mMutex;
    std::unordered_map<uint64_t, std::unordered_map<std::string, Entry>>             mTyped;
    std::unordered_map<uint64_t, std::unordered_map<uint64_t, std::unordered_map<std::string, Entry>>> mRelational;
    std::unordered_map<std::string, Entry>                                           mServer;
    std::unordered_map<void*, std::vector<Handle>>                                   mOwnerIndex;
};

} // namespace PA
