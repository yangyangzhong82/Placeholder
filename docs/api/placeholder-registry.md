# `PlaceholderRegistry` API 参考

`PlaceholderRegistry` 负责存储和管理所有已注册的占位符。它被 `PlaceholderManager` 内部使用，但了解其结构有助于深入理解 API 的工作原理。

## 核心概念

### PlaceholderCategory

占位符的分类枚举。

```cpp
enum class PlaceholderCategory {
    Server,      // 服务器级别
    Context,     // 上下文相关
    Relational,  // 关系型
    List,        // 列表
    ObjectList,  // 对象列表
};
```

### CacheKeyStrategy

缓存键的生成策略。

```cpp
enum class CacheKeyStrategy {
    Default,    // 默认策略，包含上下文信息
    ServerOnly, // 仅服务器级别，不包含上下文
};
```

### PlaceholderInfo

用于查询的占位符信息结构体。

```cpp
struct PlaceholderInfo {
    std::string              name;           // 占位符名称
    PlaceholderCategory      category;       // 分类
    bool                     isAsync{false}; // 是否异步
    std::string              targetType;     // 目标类型名
    std::string              relationalType; // 关系类型名
    std::vector<std::string> overloads;      // 重载类型列表
};
```

## 主要接口

### findBestReplacer

在 `PlaceholderManager` 内部，此函数用于根据给定的上下文查找最匹配的占位符替换函数。它会考虑类型继承关系，找到最优的重载版本。

```cpp
ReplacerMatch findBestReplacer(
    std::string_view pluginName,
    std::string_view placeholderName,
    const PlaceholderContext& ctx
);
```

- **参数**:
    - `pluginName`: 插件名。
    - `placeholderName`: 占位符名。
    - `ctx`: 当前的占位符上下文。
- **返回**: 一个 `ReplacerMatch` 结构体，包含了匹配到的替换函数及其相关信息。

### getAllPlaceholders

获取所有已注册占位符的详细信息。

```cpp
AllPlaceholders getAllPlaceholders() const;
```

- **返回**: 一个 `AllPlaceholders` 对象，其中包含一个从插件名到 `PlaceholderInfo` 向量的映射。

### hasPlaceholder

检查是否存在指定的占位符。

```cpp
bool hasPlaceholder(
    const std::string& pluginName,
    const std::string& placeholderName,
    const std::optional<std::string>& typeKey = std::nullopt
) const;
```

- **参数**:
    - `pluginName`: 插件名。
    - `placeholderName`: 占位符名。
    - `typeKey`: (可选) 上下文对象的类型键。如果提供，将检查是否存在针对该特定类型的重载。

### 注册接口

`PlaceholderRegistry` 包含一系列 `register...` 函数，与 `PlaceholderManager` 中的公开接口相对应。这些函数负责将不同类型的占位符存储在内部的数据结构中。通常情况下，开发者应通过 `PlaceholderManager` 而不是直接调用这些函数。
