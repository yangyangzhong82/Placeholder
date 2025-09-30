#pragma once

#include <list>
#include <optional>
#include <unordered_map>
#include <mutex>
#include <functional> // For std::function

namespace PA {

template <typename Key, typename Value>
class LRUCache {
public:
    LRUCache(size_t capacity) : mCapacity(capacity) {
        if (mCapacity == 0) {
            mCapacity = 1; // 至少为1，避免除零等问题
        }
    }

    // 从缓存中获取一个值。如果未找到则返回 std::nullopt。
    std::optional<Value> get(const Key& key) {
        std::lock_guard<std::mutex> lock(mMutex);

        auto it = mMap.find(key);
        if (it == mMap.end()) {
            return std::nullopt;
        }

        // 将访问的项移动到列表的前面（最近使用）
        mList.splice(mList.begin(), mList, it->second);
        return it->second->second;
    }

    // 将一个键值对放入缓存。
    void put(const Key& key, const Value& value) {
        std::lock_guard<std::mutex> lock(mMutex);

        if (mCapacity == 0) return;

        auto it = mMap.find(key);
        if (it != mMap.end()) {
            // 键已存在，更新值并移动到前面
            it->second->second = value;
            mList.splice(mList.begin(), mList, it->second);
            return;
        }

        // 如果缓存已满，则淘汰最近最少使用的项
        if (mMap.size() >= mCapacity) {
            // 改进：直接使用迭代器，避免复制 key
            auto lruIt = mList.end();
            --lruIt;
            mMap.erase(lruIt->first);
            mList.pop_back();
        }

        // 在前面插入新项
        mList.emplace_front(key, value);
        mMap[key] = mList.begin();
    }

    // 清空缓存
    void clear() {
        std::lock_guard<std::mutex> lock(mMutex);
        mMap.clear();
        mList.clear();
    }

    // 获取缓存的当前大小
    size_t size() const {
        std::lock_guard<std::mutex> lock(mMutex);
        return mMap.size();
    }

    // 动态调整容量
    void setCapacity(size_t capacity) {
        std::lock_guard<std::mutex> lock(mMutex);
        mCapacity = capacity;
        if (mCapacity == 0) {
            mCapacity = 1;
        }
        // 淘汰多余的项（优化）
        while (mMap.size() > mCapacity) {
            auto lruIt = mList.end();
            --lruIt;
            mMap.erase(lruIt->first);
            mList.pop_back();
        }
    }

    // 根据谓词删除条目
    void remove_if(std::function<bool(const Key&)> pred) {
        std::lock_guard<std::mutex> lock(mMutex);
        for (auto it = mMap.begin(); it != mMap.end();) {
            if (pred(it->first)) {
                mList.erase(it->second);
                it = mMap.erase(it);
            } else {
                ++it;
            }
        }
    }

private:
    mutable std::mutex mMutex;
    size_t mCapacity;
    std::list<std::pair<Key, Value>> mList; // 存储键值对，前面是最近使用的
    std::unordered_map<Key, typename std::list<std::pair<Key, Value>>::iterator> mMap; // 键到列表迭代器的映射
};

} // namespace PA
