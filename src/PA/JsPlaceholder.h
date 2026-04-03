// src/PA/JsPlaceholder.h
#pragma once

#include "PA/PlaceholderAPI.h"
#include "PA/logger.h"

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace PA {

// 一个"所有权桶"，用来把同一 JS 命名空间注册的占位符归为一组，便于批量卸载
struct OwnerBucket {
    std::string key;
};

// 获取或创建所有权桶（按 JS 回调命名空间）
void* getOrCreateOwner(std::string const& key);

// 按所有权桶 key 批量卸载该命名空间下的全部占位符
bool unregisterByOwnerKey(std::string const& key);

// 将上下文类型名转成 ID（方便 JS 传字符串）
uint64_t parseContextKind(std::string kind);

// 注册一个 JS 占位符（通过 RemoteCall 回调）
bool registerJsPlaceholder(
    std::string  prefix,
    std::string  tokenNameNoBraces,
    uint64_t     ctxId,
    std::string  cbNamespace,
    std::string  cbName,
    unsigned int cacheDuration = 0
);

// 注册一个 JS 缓存占位符（兼容性函数）
bool registerJsCachedPlaceholder(
    std::string  prefix,
    std::string  tokenNameNoBraces,
    uint64_t     ctxId,
    std::string  cbNamespace,
    std::string  cbName,
    unsigned int cacheDuration
);

} // namespace PA
