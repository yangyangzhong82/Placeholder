#include "PlaceholderTypeSystem.h"
#include <algorithm>
#include <queue>
#include <unordered_set>

namespace PA {

PlaceholderTypeSystem::PlaceholderTypeSystem()  = default;
PlaceholderTypeSystem::~PlaceholderTypeSystem() = default;

std::size_t PlaceholderTypeSystem::ensureTypeId(const std::string& typeKeyStr) {
    // 尝试使用共享锁进行读取，避免不必要的写锁定
    {
        std::shared_lock lk_shared(mMutex);
        auto             it = mTypeKeyToId.find(typeKeyStr);
        if (it != mTypeKeyToId.end()) {
            return it->second;
        }
    }

    // 如果类型不存在，则获取唯一锁来创建它
    std::unique_lock lk_unique(mMutex);
    // 在获取唯一锁后再次检查，防止多线程竞争导致重复创建
    auto it = mTypeKeyToId.find(typeKeyStr);
    if (it != mTypeKeyToId.end()) {
        return it->second;
    }
    auto id                  = mNextTypeId++;
    mTypeKeyToId[typeKeyStr] = id;
    mIdToTypeKey[id]         = typeKeyStr;
    return id;
}

void PlaceholderTypeSystem::registerInheritanceByKeys(
    const std::string& derivedKey,
    const std::string& baseKey,
    Caster             caster
) {
    auto d = ensureTypeId(derivedKey);
    auto b = ensureTypeId(baseKey);
    std::unique_lock lk(mMutex);
    mUpcastEdges[d][b] = caster;
    mUpcastCache.clear(); // 继承关系变更，清空缓存
}

void PlaceholderTypeSystem::registerInheritanceByKeysBatch(const std::vector<InheritancePair>& pairs) {
    // 1. 先在锁外准备好所有 Type ID，避免在持有锁时调用 ensureTypeId 导致死锁
    std::vector<std::tuple<size_t, size_t, Caster>> id_pairs;
    id_pairs.reserve(pairs.size());
    for (const auto& pair : pairs) {
        id_pairs.emplace_back(ensureTypeId(pair.derivedKey), ensureTypeId(pair.baseKey), pair.caster);
    }

    // 2. 然后再加锁一次性写入
    std::unique_lock lk(mMutex);
    for (const auto& [d, b, caster] : id_pairs) {
        mUpcastEdges[d][b] = caster;
    }
    mUpcastCache.clear(); // 继承关系变更，清空缓存
}

void PlaceholderTypeSystem::registerTypeAlias(const std::string& alias, const std::string& typeKeyStr) {
    auto             id = ensureTypeId(typeKeyStr);
    std::unique_lock lk(mMutex);
    mIdToAlias[id] = alias;
}

// 使用广度优先搜索（BFS）查询 from -> to 的“最短上行路径”
bool PlaceholderTypeSystem::findUpcastChain(
    std::size_t fromTypeId,
    std::size_t toTypeId,
    std::vector<Caster>& outChain
) const {
    outChain.clear();
    if (fromTypeId == 0 || toTypeId == 0) return false;
    if (fromTypeId == toTypeId) return true; // 空链表示已是同型

    uint64_t cacheKey = (static_cast<uint64_t>(fromTypeId) << 32) | toTypeId;

    // 1. 检查缓存
    {
        std::shared_lock lk(mMutex);
        auto             it = mUpcastCache.find(cacheKey);
        if (it != mUpcastCache.end()) {
            outChain = it->second.chain;
            return it->second.success;
        }
    }

    // 2. 缓存未命中，执行 BFS
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
                    // 回溯构造链
                    std::size_t x = nxt;
                    while (x != fromTypeId) {
                        auto [p, c] = prev[x];
                        resultChain.push_back(c);
                        x = p;
                    }
                    std::reverse(resultChain.begin(), resultChain.end());
                    success = true;
                    goto bfs_end; // 找到路径，跳出循环
                }
                q.push(nxt);
            }
        }
    }
bfs_end:

    // 3. 结果写入缓存
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
    auto             aliasIt = mIdToAlias.find(typeId);
    if (aliasIt != mIdToAlias.end()) {
        return aliasIt->second;
    }
    auto keyIt = mIdToTypeKey.find(typeId);
    if (keyIt != mIdToTypeKey.end()) {
        return keyIt->second;
    }
    return "UnknownTypeId(" + std::to_string(typeId) + ")";
}

} // namespace PA
