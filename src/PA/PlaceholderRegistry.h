// src/PA/PlaceholderRegistry.h
#pragma once

#include "PA/PlaceholderAPI.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace PA {

class PlaceholderRegistry {
public:
    PlaceholderRegistry();
    void registerPlaceholder(std::string_view prefix, std::shared_ptr<const IPlaceholder> p, void* owner);
    void registerRelationalPlaceholder(std::string_view prefix, std::shared_ptr<const IPlaceholder> p, void* owner, uint64_t mainContextTypeId, uint64_t relationalContextTypeId);
    void unregisterByOwner(void* owner);

    std::vector<std::pair<std::string, std::shared_ptr<const IPlaceholder>>> getTypedPlaceholders(const IContext* ctx) const;
    std::vector<std::pair<std::string, std::shared_ptr<const IPlaceholder>>> getServerPlaceholders() const;

    std::shared_ptr<const IPlaceholder> findPlaceholder(const std::string& token, const IContext* ctx) const;

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

    struct Snapshot {
        std::unordered_map<uint64_t, std::unordered_map<std::string, Entry>>             typed;
        std::unordered_map<uint64_t, std::unordered_map<uint64_t, std::unordered_map<std::string, Entry>>> relational;
        std::unordered_map<std::string, Entry>                                           server;
        std::unordered_map<void*, std::vector<Handle>>                                   ownerIndex;
    };

    mutable std::mutex                                     mWriteMutex;
    std::atomic<std::shared_ptr<const Snapshot>>           mSnapshot;
};

} // namespace PA
