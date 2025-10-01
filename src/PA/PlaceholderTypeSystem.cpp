#include "PlaceholderTypeSystem.h"
#include <algorithm>
#include <queue>
#include <unordered_set>

namespace PA {

PlaceholderTypeSystem::PlaceholderTypeSystem()  = default;
PlaceholderTypeSystem::~PlaceholderTypeSystem() = default;
std::size_t PlaceholderTypeSystem::findRootNoLock(std::size_t x) const {
    auto it = mUnionParent.find(x);
    while (it != mUnionParent.end() && it->second != x) {
        x  = it->second;
        it = mUnionParent.find(x);
    }
    return x;
}

void PlaceholderTypeSystem::uniteNoLock(std::size_t a, std::size_t b) {
    a = findRootNoLock(a);
    b = findRootNoLock(b);
    if (a == b) return;
    mUnionParent[a] = b;
    mUpcastCache.clear(); // 合并后，清空上行链缓存
}
std::size_t PlaceholderTypeSystem::ensureTypeId(const std::string& typeKeyStr) {
    {
        std::shared_lock lk_shared(mMutex);
        auto             it = mTypeKeyToId.find(typeKeyStr);
        if (it != mTypeKeyToId.end()) {
            // 返回“代表 id”，避免不同键拿到不同根
            return findRootNoLock(it->second);
        }
    }
    std::unique_lock lk_unique(mMutex);
    auto             it = mTypeKeyToId.find(typeKeyStr);
    if (it != mTypeKeyToId.end()) {
        return findRootNoLock(it->second);
    }
    auto id                  = mNextTypeId++;
    mTypeKeyToId[typeKeyStr] = id;
    mIdToTypeKey[id]         = typeKeyStr;
    // 初始时自己是自己的根
    mUnionParent.emplace(id, id);
    return id;
}

void PlaceholderTypeSystem::registerInheritanceByKeys(
    const std::string& derivedKey,
    const std::string& baseKey,
    Caster             caster
) {
    auto             d = ensureTypeId(derivedKey);
    auto             b = ensureTypeId(baseKey);
    std::unique_lock lk(mMutex);
    d                  = findRootNoLock(d);
    b                  = findRootNoLock(b);
    mUpcastEdges[d][b] = caster;
    mUpcastCache.clear();
}

void PlaceholderTypeSystem::registerInheritanceByKeysBatch(const std::vector<InheritancePair>& pairs) {
    std::vector<std::tuple<size_t, size_t, Caster>> id_pairs;
    id_pairs.reserve(pairs.size());
    for (const auto& pair : pairs) {
        id_pairs.emplace_back(ensureTypeId(pair.derivedKey), ensureTypeId(pair.baseKey), pair.caster);
    }

    std::unique_lock lk(mMutex);
    for (auto [d, b, caster] : id_pairs) {
        d                  = findRootNoLock(d);
        b                  = findRootNoLock(b);
        mUpcastEdges[d][b] = caster;
    }
    mUpcastCache.clear();
}

// 关键增强：alias 同时做“等价合并”
void PlaceholderTypeSystem::registerTypeAlias(const std::string& alias, const std::string& typeKeyStr) {
    auto             id = ensureTypeId(typeKeyStr); // 已是 root 或将被合并为 root
    std::unique_lock lk(mMutex);

    // 1) alias 也当作“类型键”可被 ensureTypeId 使用
    auto itAlias = mTypeKeyToId.find(alias);
    if (itAlias == mTypeKeyToId.end()) {
        mTypeKeyToId[alias]            = id;
        mIdToAlias[findRootNoLock(id)] = alias;
        return;
    }

    // 2) alias 已存在，合并两者为同一个 root
    auto aliasId = findRootNoLock(itAlias->second);
    auto realId  = findRootNoLock(id);
    if (aliasId != realId) {
        uniteNoLock(realId, aliasId);
    }
    mIdToAlias[findRootNoLock(realId)] = alias;
}

bool PlaceholderTypeSystem::findUpcastChain(std::size_t fromTypeId, std::size_t toTypeId, std::vector<Caster>& outChain)
    const {
    outChain.clear();
    if (fromTypeId == 0 || toTypeId == 0) return false;

    {
        std::shared_lock lk(mMutex);
        // 先映射到代表 id
        fromTypeId = findRootNoLock(fromTypeId);
        toTypeId   = findRootNoLock(toTypeId);
    }

    if (fromTypeId == toTypeId) return true; // 同类（包含 alias 合并后）

    uint64_t cacheKey = (static_cast<uint64_t>(fromTypeId) << 32) | toTypeId;

    {
        std::shared_lock lk(mMutex);
        auto             it = mUpcastCache.find(cacheKey);
        if (it != mUpcastCache.end()) {
            outChain = it->second.chain;
            return it->second.success;
        }
    }

    // BFS 保持不变，但用“代表 id”探测
    std::vector<Caster> resultChain;
    bool                success = false;
    {
        std::shared_lock                                                lk(mMutex);
        std::unordered_map<std::size_t, std::pair<std::size_t, Caster>> prev;
        std::queue<std::size_t>                                         q;

        prev[fromTypeId] = {0, nullptr};
        q.push(fromTypeId);

        while (!q.empty()) {
            auto cur = q.front();
            q.pop();

            auto it = mUpcastEdges.find(cur);
            if (it == mUpcastEdges.end()) continue;

            for (auto& [nxt, caster] : it->second) {
                if (prev.find(nxt) != prev.end()) continue;
                prev[nxt] = {cur, caster};
                if (nxt == toTypeId) {
                    std::size_t x = nxt;
                    while (x != fromTypeId) {
                        auto [p, c] = prev[x];
                        resultChain.push_back(c);
                        x = p;
                    }
                    std::reverse(resultChain.begin(), resultChain.end());
                    success = true;
                    goto bfs_end;
                }
                q.push(nxt);
            }
        }
    }
bfs_end:

{
    std::unique_lock lk(mMutex);
    mUpcastCache[cacheKey] = {success, resultChain};
}

    outChain = std::move(resultChain);
    return success;
}

std::string PlaceholderTypeSystem::getTypeName(std::size_t typeId) const {
    if (typeId == 0) return "N/A";
    std::shared_lock lk(mMutex);
    typeId       = findRootNoLock(typeId);
    auto aliasIt = mIdToAlias.find(typeId);
    if (aliasIt != mIdToAlias.end()) return aliasIt->second;
    auto keyIt = mIdToTypeKey.find(typeId);
    if (keyIt != mIdToTypeKey.end()) return keyIt->second;
    return "UnknownTypeId(" + std::to_string(typeId) + ")";
}

} // namespace PA
