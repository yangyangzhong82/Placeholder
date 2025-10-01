# `PlaceholderManager` API 参考

`PlaceholderManager` 是 PlaceholderAPI 的核心管理类，负责注册、解析和替换占位符。它是一个单例类，整合了类型系统、注册表和缓存机制，提供了同步和异步的占位符处理能力。

## 获取实例

### getInstance

获取 `PlaceholderManager` 的单例实例。

```cpp
static PlaceholderManager& getInstance();
```

- **返回**: `PlaceholderManager` 的引用。

---

## 占位符注册

`PlaceholderManager` 提供了一套丰富的接口来注册不同类型的占位符。

### registerServerPlaceholder

注册一个服务器范围的占位符（无参数）。

```cpp
void registerServerPlaceholder(
    const std::string&           pluginName,
    const std::string&           placeholder,
    ServerReplacer               replacer,
    std::optional<CacheDuration> cache_duration = std::nullopt,
    CacheKeyStrategy             strategy       = CacheKeyStrategy::Default
);
```

- **参数**:
    - `pluginName`: 插件名。
    - `placeholder`: 占位符标识符。
    - `replacer`: 替换函数。
    - `cache_duration`: (可选) 缓存持续时间。
    - `strategy`: (可选) 缓存键策略。

### registerServerPlaceholderWithParams

注册一个带参数的服务器范围占位符。

```cpp
void registerServerPlaceholderWithParams(
    const std::string&           pluginName,
    const std::string&           placeholder,
    ServerReplacerWithParams&&   replacer,
    std::optional<CacheDuration> cache_duration = std::nullopt,
    CacheKeyStrategy             strategy       = CacheKeyStrategy::Default
);
```

### registerAsyncServerPlaceholder

注册一个异步的服务器范围占位符（无参数）。

```cpp
void registerAsyncServerPlaceholder(
    const std::string&           pluginName,
    const std::string&           placeholder,
    AsyncServerReplacer&&        replacer,
    std::optional<CacheDuration> cache_duration = std::nullopt,
    CacheKeyStrategy             strategy       = CacheKeyStrategy::Default
);
```

### registerAsyncServerPlaceholderWithParams

注册一个带参数的异步服务器范围占位符。

```cpp
void registerAsyncServerPlaceholderWithParams(
    const std::string&              pluginName,
    const std::string&              placeholder,
    AsyncServerReplacerWithParams&& replacer,
    std::optional<CacheDuration>    cache_duration = std::nullopt,
    CacheKeyStrategy                strategy       = CacheKeyStrategy::Default
);
```

### registerPlaceholder

注册一个特定类型的占位符（无参数）。

```cpp
template <typename T>
void registerPlaceholder(
    const std::string&               pluginName,
    const std::string&               placeholder,
    std::function<std::string(T*)>&& replacer,
    std::optional<CacheDuration>     cache_duration = std::nullopt,
    CacheKeyStrategy                 strategy       = CacheKeyStrategy::Default
);
```

- **模板参数**:
    - `T`: 上下文对象的类型。

### registerPlaceholderWithParams

注册一个特定类型的带参数占位符。

```cpp
template <typename T>
void registerPlaceholderWithParams(
    const std::string&                                           pluginName,
    const std::string&                                           placeholder,
    std::function<std::string(T*, const Utils::ParsedParams&)>&& replacer,
    std::optional<CacheDuration>                                 cache_duration = std::nullopt,
    CacheKeyStrategy                                             strategy       = CacheKeyStrategy::Default
);
```

### registerAsyncPlaceholder

注册一个特定类型的异步占位符（无参数）。

```cpp
template <typename T>
void registerAsyncPlaceholder(
    const std::string&                            pluginName,
    const std::string&                            placeholder,
    std::function<std::future<std::string>(T*)>&& replacer,
    std::optional<CacheDuration>                  cache_duration = std::nullopt,
    CacheKeyStrategy                              strategy       = CacheKeyStrategy::Default
);
```

### registerAsyncPlaceholderWithParams

注册一个特定类型的带参数异步占位符。

```cpp
template <typename T>
void registerAsyncPlaceholderWithParams(
    const std::string&                                                        pluginName,
    const std::string&                                                        placeholder,
    std::function<std::future<std::string>(T*, const Utils::ParsedParams&)>&& replacer,
    std::optional<CacheDuration>                                              cache_duration = std::nullopt,
    CacheKeyStrategy                                                          strategy = CacheKeyStrategy::Default
);
```

### registerRelationalPlaceholder

注册一个关系型占位符（需要两个上下文对象）。

```cpp
template <typename T, typename T_Rel>
void registerRelationalPlaceholder(
    const std::string&                       pluginName,
    const std::string&                       placeholder,
    std::function<std::string(T*, T_Rel*)>&& replacer,
    std::optional<CacheDuration>             cache_duration = std::nullopt,
    CacheKeyStrategy                         strategy       = CacheKeyStrategy::Default
);
```

- **模板参数**:
    - `T`: 主上下文对象的类型。
    - `T_Rel`: 关系上下文对象的类型。

### registerRelationalPlaceholderWithParams

注册一个带参数的关系型占位符。

```cpp
template <typename T, typename T_Rel>
void registerRelationalPlaceholderWithParams(
    const std::string&                                                   pluginName,
    const std::string&                                                   placeholder,
    std::function<std::string(T*, T_Rel*, const Utils::ParsedParams&)>&& replacer,
    std::optional<CacheDuration>                                         cache_duration = std::nullopt,
    CacheKeyStrategy                                                     strategy       = CacheKeyStrategy::Default
);
```

---

## 占位符替换

### replacePlaceholders

替换字符串中的占位符。提供多个重载版本以适应不同上下文。

```cpp
// 服务器占位符
std::string replacePlaceholders(const std::string& text);

// 使用 Player* 上下文
std::string replacePlaceholders(const std::string& text, Player* player);

// 使用通用 PlaceholderContext
std::string replacePlaceholders(const std::string& text, const PlaceholderContext& ctx);

// 使用模板化上下文对象
template <typename T>
std::string replacePlaceholders(const std::string& text, T* obj);
```

### compileTemplate

将字符串编译成模板以提高重复替换的效率。

```cpp
CompiledTemplate compileTemplate(const std::string& text);
```

- **返回**: 编译后的 `CompiledTemplate` 对象。

### replacePlaceholders (with CompiledTemplate)

使用编译后的模板和上下文进行替换，效率更高。

```cpp
std::string replacePlaceholders(const CompiledTemplate& tpl, const PlaceholderContext& ctx);
```

### replacePlaceholdersAsync

异步替换占位符。

```cpp
std::future<std::string> replacePlaceholdersAsync(const std::string& text, const PlaceholderContext& ctx);

std::future<std::string> replacePlaceholdersAsync(const CompiledTemplate& tpl, const PlaceholderContext& ctx);
```

- **返回**: 一个持有最终结果的 `std::future<std::string>`。

---

## 类型系统

### registerInheritance

注册类型继承关系，允许派生类使用基类的占位符。

```cpp
template <typename Derived, typename Base>
void registerInheritance();
```

- **模板参数**:
    - `Derived`: 派生类。
    - `Base`: 基类。

### registerTypeAlias

为一个类型注册别名。

```cpp
template <typename T>
void registerTypeAlias(const std::string& alias);
```

---

## 其他接口

### clearCache

清理占位符的全局缓存。

```cpp
// 清理所有缓存
void clearCache();

// 清理特定插件的缓存
void clearCache(const std::string& pluginName);
```

### getAllPlaceholders

获取所有已注册的占位符信息。

```cpp
AllPlaceholders getAllPlaceholders() const;
```

- **返回**: 包含所有占位符信息的 `AllPlaceholders` 结构体。

### hasPlaceholder

检查是否存在指定的占位符。

```cpp
bool hasPlaceholder(
    const std::string&                pluginName,
    const std::string&                placeholderName,
    const std::optional<std::string>& typeKey = std::nullopt
) const;
