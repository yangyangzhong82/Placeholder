// src/PA/PlaceholderRegistry.h
#pragma once

#include "PA/PlaceholderAPI.h"
#include <algorithm>
#include <atomic>
#include <cctype>
#include <memory>
#include <mutex>
#include <string.h>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>


namespace PA {

// 别名适配器条目
struct Adapter {
    uint64_t          fromCtxId{};
    uint64_t          toCtxId{};
    ContextResolverFn resolver{};
    void*             owner{};
};

// 将 CachedEntry 结构体移到类外部，使其在 findPlaceholder 声明时可见
struct CachedEntry {
    std::shared_ptr<const IPlaceholder> ptr{};
    void*                               owner{};
    unsigned int                        cacheDuration{}; // 缓存持续时间（秒）

    // 内部缓存值结构体
    struct Value {
        std::string                           value;
        std::chrono::steady_clock::time_point lastEvaluated;
    };

    // 线程安全的缓存存储
    // Key: context_instance_key + ":" + args_key
    mutable std::mutex                                  cacheMutex;
    mutable std::unordered_map<std::string, Value>      cachedValues;

    CachedEntry() = default;
    CachedEntry(CachedEntry&& other) noexcept
        : ptr(std::move(other.ptr)), owner(other.owner), cacheDuration(other.cacheDuration),
          cachedValues(std::move(other.cachedValues)) {}
    CachedEntry& operator=(CachedEntry&& other) noexcept {
        if (this != &other) {
            ptr           = std::move(other.ptr);
            owner         = other.owner;
            cacheDuration = other.cacheDuration;
            cachedValues  = std::move(other.cachedValues);
        }
        return *this;
    }

    // Delete copy constructor and copy assignment operator
    CachedEntry(const CachedEntry&) = delete;
    CachedEntry& operator=(const CachedEntry&) = delete;
};

class PlaceholderRegistry; // Forward declaration

struct LookupResult {
    std::shared_ptr<const IPlaceholder> placeholder;
    const CachedEntry*                  entry = nullptr;
    std::shared_ptr<const void>         snapshot_guard;
};

// ScopedPlaceholderRegistrar 的实现
class ScopedPlaceholderRegistrar : public IScopedPlaceholderRegistrar {
public:
    ScopedPlaceholderRegistrar(PlaceholderRegistry* registry, void* owner) : mRegistry(registry), mOwner(owner) {}
    ~ScopedPlaceholderRegistrar() override;

    void registerPlaceholder(std::string_view prefix, std::shared_ptr<const IPlaceholder> p) override;
    void registerCachedPlaceholder(
        std::string_view                    prefix,
        std::shared_ptr<const IPlaceholder> p,
        unsigned int                        cacheDuration
    ) override;
    void registerRelationalPlaceholder(
        std::string_view                    prefix,
        std::shared_ptr<const IPlaceholder> p,
        uint64_t                            mainContextTypeId,
        uint64_t                            relationalContextTypeId
    ) override;
    void registerCachedRelationalPlaceholder(
        std::string_view                    prefix,
        std::shared_ptr<const IPlaceholder> p,
        uint64_t                            mainContextTypeId,
        uint64_t                            relationalContextTypeId,
        unsigned int                        cacheDuration
    ) override;
    void registerContextAlias(
        std::string_view  alias,
        uint64_t          fromContextTypeId,
        uint64_t          toContextTypeId,
        ContextResolverFn resolver
    ) override;
    void registerContextFactory(uint64_t contextTypeId, ContextFactoryFn factory) override;

private:
    PlaceholderRegistry* mRegistry;
    void*                mOwner;
};

class PlaceholderRegistry {
public:
    PlaceholderRegistry();
    void registerPlaceholder(std::string_view prefix, std::shared_ptr<const IPlaceholder> p, void* owner);
    void registerCachedPlaceholder(
        std::string_view                    prefix,
        std::shared_ptr<const IPlaceholder> p,
        void*                               owner,
        unsigned int                        cacheDuration
    ); // 添加 cacheDuration 参数
    void registerRelationalPlaceholder(
        std::string_view                    prefix,
        std::shared_ptr<const IPlaceholder> p,
        void*                               owner,
        uint64_t                            mainContextTypeId,
        uint64_t                            relationalContextTypeId
    );
    void registerCachedRelationalPlaceholder(
        std::string_view                    prefix,
        std::shared_ptr<const IPlaceholder> p,
        void*                               owner,
        uint64_t                            mainContextTypeId,
        uint64_t                            relationalContextTypeId,
        unsigned int                        cacheDuration
    );
    void unregisterByOwner(void* owner);

    // 注册上下文别名适配器（例如 look/last_hit 等）
    void registerContextAlias(
        std::string_view  alias,
        uint64_t          fromContextTypeId,
        uint64_t          toContextTypeId,
        ContextResolverFn resolver,
        void*             owner
    );

    // 注册上下文工厂
    void registerContextFactory(uint64_t contextTypeId, ContextFactoryFn factory, void* owner);

    std::vector<std::pair<std::string, std::shared_ptr<const IPlaceholder>>> getTypedPlaceholders(const IContext* ctx
    ) const;
    std::vector<std::pair<std::string, std::shared_ptr<const IPlaceholder>>> getServerPlaceholders() const;

    // 修改 findPlaceholder 的返回类型，以支持缓存
    LookupResult findPlaceholder(const std::string& token, const IContext* ctx) const;

    // 查找上下文别名
    const Adapter* findContextAlias(std::string_view alias, uint64_t fromContextTypeId) const;

    // 查找上下文工厂
    ContextFactoryFn findContextFactory(uint64_t contextTypeId) const;

private:
    struct Entry {
        std::shared_ptr<const IPlaceholder> ptr{};
        void*                               owner{};
    };

    struct Handle {
        bool        isServer{};
        bool        isRelational{};
        bool        isCached{};  // 是否为缓存占位符
        bool        isAdapter{}; // 是否为别名适配器
        bool        isFactory{}; // 是否为上下文工厂
        uint64_t    mainCtxId{};
        uint64_t    relCtxId{};
        uint64_t    ctxId{};
        std::string token; // 对于适配器，这里存 alias; 对于工厂，这里存 ctxId 的字符串形式
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
        std::unordered_map<uint64_t, std::unordered_map<std::string, Entry, ci_hash, ci_equal>> typed;
        std::unordered_map<
            uint64_t,
            std::unordered_map<uint64_t, std::unordered_map<std::string, Entry, ci_hash, ci_equal>>>
                                                                  relational;
        std::unordered_map<std::string, Entry, ci_hash, ci_equal> server;
        std::unordered_map<uint64_t, std::unordered_map<std::string, CachedEntry, ci_hash, ci_equal>>
            cached_typed; // 缓存的 Typed 占位符
        std::unordered_map<
            uint64_t,
            std::unordered_map<uint64_t, std::unordered_map<std::string, CachedEntry, ci_hash, ci_equal>>>
            cached_relational; // 缓存的关系型占位符
        std::unordered_map<std::string, CachedEntry, ci_hash, ci_equal> cached_server; // 缓存的 Server 占位符

        // alias -> adapters（大小写不敏感）
        std::unordered_map<std::string, std::vector<Adapter>, ci_hash, ci_equal> adapters;

        // contextTypeId -> factory
        struct FactoryEntry {
            ContextFactoryFn factory{};
            void*            owner{};
        };
        std::unordered_map<uint64_t, FactoryEntry> contextFactories;

        std::unordered_map<void*, std::vector<Handle>> ownerIndex;

        Snapshot() = default;

        Snapshot(const Snapshot& other)
        : typed(other.typed),
          relational(other.relational),
          server(other.server),
          adapters(other.adapters),
          contextFactories(other.contextFactories),
          ownerIndex(other.ownerIndex) {
            for (const auto& pair : other.cached_typed) {
                for (const auto& inner_pair : pair.second) {
                    CachedEntry new_entry;
                    new_entry.ptr           = inner_pair.second.ptr;
                    new_entry.owner         = inner_pair.second.owner;
                    new_entry.cacheDuration = inner_pair.second.cacheDuration;
                    cached_typed[pair.first].emplace(inner_pair.first, std::move(new_entry));
                }
            }
            for (const auto& pair : other.cached_relational) {
                for (const auto& inner_pair : pair.second) {
                    for (const auto& final_pair : inner_pair.second) {
                        CachedEntry new_entry;
                        new_entry.ptr           = final_pair.second.ptr;
                        new_entry.owner         = final_pair.second.owner;
                        new_entry.cacheDuration = final_pair.second.cacheDuration;
                        cached_relational[pair.first][inner_pair.first].emplace(final_pair.first, std::move(new_entry));
                    }
                }
            }
            for (const auto& pair : other.cached_server) {
                CachedEntry new_entry;
                new_entry.ptr           = pair.second.ptr;
                new_entry.owner         = pair.second.owner;
                new_entry.cacheDuration = pair.second.cacheDuration;
                cached_server.emplace(pair.first, std::move(new_entry));
            }
        }
    };

    mutable std::mutex                           mWriteMutex;
    std::atomic<std::shared_ptr<const Snapshot>> mSnapshot;
};

} // namespace PA
