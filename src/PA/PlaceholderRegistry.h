// src/PA/PlaceholderRegistry.h
#pragma once

#include "PA/PlaceholderAPI.h"
#include <algorithm>
#include <atomic>
#include <cctype>
#include <memory>
#include <mutex>
#include <string>
#include <string.h>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace PA {

// 将 CachedEntry 结构体移到类外部，使其在 findPlaceholder 声明时可见
struct CachedEntry {
    std::shared_ptr<const IPlaceholder> ptr{};
    void*                               owner{};
    unsigned int                        cacheDuration{}; // 缓存持续时间（秒）
    mutable std::string                 cachedValue;     // 缓存的值
    mutable std::chrono::steady_clock::time_point lastEvaluated; // 上次评估时间
};

class PlaceholderRegistry {
public:
    PlaceholderRegistry();
    void registerPlaceholder(std::string_view prefix, std::shared_ptr<const IPlaceholder> p, void* owner);
    void registerCachedPlaceholder(std::string_view prefix, std::shared_ptr<const IPlaceholder> p, void* owner, unsigned int cacheDuration); // 添加 cacheDuration 参数
    void registerRelationalPlaceholder(std::string_view prefix, std::shared_ptr<const IPlaceholder> p, void* owner, uint64_t mainContextTypeId, uint64_t relationalContextTypeId);
    void unregisterByOwner(void* owner);

    std::vector<std::pair<std::string, std::shared_ptr<const IPlaceholder>>> getTypedPlaceholders(const IContext* ctx) const;
    std::vector<std::pair<std::string, std::shared_ptr<const IPlaceholder>>> getServerPlaceholders() const;

    // 修改 findPlaceholder 的返回类型，以支持缓存
    std::pair<std::shared_ptr<const IPlaceholder>, const CachedEntry*> findPlaceholder(const std::string& token, const IContext* ctx) const;

private:
    struct Entry {
        std::shared_ptr<const IPlaceholder> ptr{};
        void*                               owner{};
    };

    struct Handle {
        bool        isServer{};
        bool        isRelational{};
        bool        isCached{}; // 新增：是否为缓存占位符
        uint64_t    mainCtxId{};
        uint64_t    relCtxId{};
        uint64_t    ctxId{};
        std::string token;
    };

    struct ci_hash {
        size_t operator()(const std::string& s) const {
            std::string lower_s;
            lower_s.resize(s.size());
            std::transform(s.begin(), s.end(), lower_s.begin(), [](unsigned char c) { return std::tolower(c); });
            return std::hash<std::string>()(lower_s);
        }
    };

    struct ci_equal {
        bool operator()(const std::string& s1, const std::string& s2) const {
            return _stricmp(s1.c_str(), s2.c_str()) == 0;
        }
    };

    struct Snapshot {
        std::unordered_map<uint64_t, std::unordered_map<std::string, Entry, ci_hash, ci_equal>>             typed;
        std::unordered_map<uint64_t, std::unordered_map<uint64_t, std::unordered_map<std::string, Entry, ci_hash, ci_equal>>> relational;
        std::unordered_map<std::string, Entry, ci_hash, ci_equal>                                           server;
        std::unordered_map<uint64_t, std::unordered_map<std::string, CachedEntry, ci_hash, ci_equal>> cached_typed;   // 新增：缓存的 Typed 占位符
        std::unordered_map<std::string, CachedEntry, ci_hash, ci_equal>                               cached_server;  // 新增：缓存的 Server 占位符
        std::unordered_map<void*, std::vector<Handle>>                                                      ownerIndex;
    };

    mutable std::mutex                                     mWriteMutex;
    std::atomic<std::shared_ptr<const Snapshot>>           mSnapshot;
};

} // namespace PA
