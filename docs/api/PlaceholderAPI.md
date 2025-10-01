# PlaceholderAPI 开发者文档

PlaceholderAPI (PA) 是一个为 LeviLamina (LLSE) 设计的功能强大、高性能且高度可扩展的占位符系统。本文档旨在为插件开发者提供全面的 API 使用指南。

## 核心概念

在深入 API 之前，理解以下核心概念至关重要：

### 1. PlaceholderManager (占位符管理器)

`PlaceholderManager` 是整个 API 的核心与入口。它是一个单例类，您可以通过 `PA::PlaceholderManager::getInstance()` 获取其实例。所有操作，如注册、替换和配置，都通过这个管理器进行。

### 2. PlaceholderContext (占位符上下文)

上下文（Context）是执行占位符替换时所需的数据。它封装了指向具体对象（如 `Player*`）的指针及其内部类型 ID，使得系统能够根据对象的实际类型找到最匹配的占位符。

-   **普通上下文**: 包含一个对象指针。
-   **关系型上下文**: 包含两个对象指针，用于处理需要两个对象关系的占位符（例如 `player_relation_is_enemy`）。

您可以使用 `manager.makeContext(ptr)` 辅助函数轻松创建上下文。

### 3. 类型系统 (Type System)

系统内部维护着一个类型系统，用于管理 C++ 类型与内部 ID 之间的映射。
-   **类型安全**: 通过 C++ 模板，API 能在编译期确定类型，并自动处理类型的注册。
-   **继承支持**: 您可以注册类之间的继承关系。例如，如果一个占位符是为 `Player` 注册的，当传入一个 `ServerPlayer` 对象（假设其继承自 `Player`）时，该占位符依然可以被正确解析。

### 4. 模板编译 (Template Compilation)

为了极致的性能，PA 引入了模板编译机制。
-   `compileTemplate(text)`: 将一个包含占位符的字符串预先解析成一个优化的 `CompiledTemplate` 对象。
-   `replacePlaceholders(template, context)`: 使用编译后的模板进行替换，避免了每次都重新解析字符串的开销，在需要对同一格式进行大量、重复替换的场景（如计分板、UI更新）下性能提升显著。

### 5. 多级缓存 (Caching)

系统内置了多级缓存以减少不必要的计算：
-   **全局缓存**: 缓存占位符的最终结果，并可设置过期时间。
-   **编译缓存**: 缓存编译后的 `CompiledTemplate` 对象。
-   **请求内缓存**: 在单次替换请求中（例如替换一个包含多个相同占位符的字符串），同一个占位符只会被计算一次。

---

## API 使用指南

### 1. 获取管理器实例

所有操作都始于获取 `PlaceholderManager` 的单例。

```cpp
#include "PA/PlaceholderManager.h"

auto& manager = PA::PlaceholderManager::getInstance();
```

### 2. 注册占位符

PA 支持多种类型的占位符，以满足不同场景的需求。

#### 占位符通用参数

在注册大部分占位符时，您可以提供以下可选参数：
-   `cache_duration`: `std::chrono::duration` 类型，用于设置该占位符的全局缓存时间。例如 `std::chrono::seconds(10)`。
-   `strategy`: `CacheKeyStrategy` 枚举，定义缓存键的生成策略。
    -   `CacheKeyStrategy::Default`: 默认策略，缓存键与上下文对象相关。
    -   `CacheKeyStrategy::ServerOnly`: 缓存键与上下文无关，即使传入不同玩家，也会命中相同的服务器级缓存。

#### A. 服务器占位符 (Server Placeholders)

不依赖任何特定对象，其值在整个服务器范围内通用。

-   **无参数**: `registerServerPlaceholder`
-   **带参数**: `registerServerPlaceholderWithParams`

**示例：注册一个返回服务器在线人数的占位符 `myserver:online_players`**
```cpp
#include "ll/api/service/Bedrock.h"
#include "mc/server/ServerPlayer.h"

manager.registerServerPlaceholder(
    "myserver",
    "online_players",
    []() {
        auto players = ll::service::getLevel()->getPlayers();
        return std::to_string(players.size());
    },
    std::chrono::seconds(5) // 缓存5秒
);
```

#### B. 上下文占位符 (Context Placeholders)

值与传入的特定对象（如 `Player*`）相关，这是最常用的占位符类型。

-   **无参数**: `registerPlaceholder<T>`
-   **带参数**: `registerPlaceholderWithParams<T>`

**示例：注册一个返回玩家名称的占位符 `myplugin:player_name`**
```cpp
#include "mc/world/actor/player/Player.h"

manager.registerPlaceholder<Player>("myplugin", "player_name", [](Player* player) {
    if (player) {
        return player->getRealName();
    }
    return std::string{}; // 返回空字符串
});
```

**示例：带参数的占位符 `myplugin:player_pos`**
```cpp
#include "mc/world/actor/player/Player.h"
#include "PA/Utils.h" // for ParsedParams
#include <fmt/core.h>

manager.registerPlaceholderWithParams<Player>(
    "myplugin",
    "player_pos",
    [](Player* player, const PA::Utils::ParsedParams& params) {
        if (!player) return std::string{};
        std::string format = std::string(params.get("format").value_or("X: %d, Y: %d, Z: %d"));
        auto& pos = player->getPosition();
        try {
            return fmt::format(format, pos.x, pos.y, pos.z);
        } catch (...) {
            return "Invalid format";
        }
    }
);
```

#### C. 关系型占位符 (Relational Placeholders)

用于处理需要两个对象进行比较或交互的场景。

-   **无参数**: `registerRelationalPlaceholder<T, T_Rel>`
-   **带参数**: `registerRelationalPlaceholderWithParams<T, T_Rel>`

**示例：判断玩家P1是否在看玩家P2的占位符 `myplugin:is_looking_at`**
```cpp
manager.registerRelationalPlaceholder<Player, Player>(
    "myplugin",
    "is_looking_at",
    [](Player* p1, Player* p2) {
        // 此处应实现具体的逻辑来判断 p1 是否在注视 p2
        bool isLooking = false; // 假设的逻辑结果
        return isLooking ? "true" : "false";
    }
);
```

#### D. 列表占位符 (List Placeholders)

返回一个字符串列表，系统会自动使用指定的分隔符（通过 `separator` 参数，默认为 `, `）将它们连接成单个字符串。

-   **服务器范围**: `registerServerListPlaceholder`, `registerServerListPlaceholderWithParams`
-   **上下文相关**: `registerListPlaceholder<T>`, `registerListPlaceholderWithParams<T>`

**示例：返回服务器所有玩家名字的列表 `myserver:all_players_list`**
```cpp
manager.registerServerListPlaceholder("myserver", "all_players_list", []() {
    std::vector<std::string> names;
    for (auto& player : ll::service::getLevel()->getPlayers()) {
        names.push_back(player->getName());
    }
    return names;
});

// 使用时: {myserver:all_players_list|separator=; }
```

#### E. 对象列表占位符 (Object List Placeholders)

返回一个对象列表（`std::vector<PlaceholderContext>`）。这是一种非常强大的占位符，可以与 `template` 参数结合使用，对列表中的每个对象应用一个模板。

-   **服务器范围**: `registerServerObjectListPlaceholder`, `registerServerObjectListPlaceholderWithParams`
-   **上下文相关**: `registerObjectListPlaceholder<T>`, `registerObjectListPlaceholderWithParams<T>`

**示例：获取一个玩家附近所有实体的名称列表**
```cpp
manager.registerObjectListPlaceholder<Player>(
    "myplugin",
    "nearby_entities",
    [](Player* player) {
        std::vector<PA::PlaceholderContext> entities;
        // 伪代码：获取附近实体
        for (auto* entity : getNearbyEntities(player, 10.0)) {
            // 使用 makeContext 创建上下文并添加到列表
            entities.push_back(manager.makeContext(entity));
        }
        return entities;
    }
);

/*
使用时:
{myplugin:nearby_entities|template={entity:name}|join=, }
- `template` 参数会对列表中的每个实体替换 `{entity:name}`
- `join` 参数会将替换后的结果用 `, ` 连接起来
*/
```

#### F. 异步占位符

对于耗时的操作（如数据库查询、网络请求），应注册异步占位符，以避免阻塞服务器主线程。API 与同步版本类似，只是函数返回 `std::future<T>`。

-   `registerAsyncServerPlaceholder`, `registerAsyncServerPlaceholderWithParams`
-   `registerAsyncPlaceholder<T>`, `registerAsyncPlaceholderWithParams<T>`

**示例：一个模拟耗时操作的异步占位符**
```cpp
#include <chrono>
#include <thread>

manager.registerAsyncServerPlaceholder("myplugin", "long_task", []() {
    // 返回一个 std::future<std::string>
    return std::async(std::launch::async, [] {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return "Task Complete";
    });
});
```

### 3. 替换占位符

#### A. 同步替换

适用于不包含或不关心异步占位符结果的场景。

```cpp
Player* somePlayer = ...;
std::string text = "玩家 {myplugin:player_name} 的坐标是 {myplugin:player_pos}";

// 方法一：直接替换
std::string result = manager.replacePlaceholders(text, somePlayer);

// 方法二：使用编译模板（推荐用于重复替换）
PA::CompiledTemplate tpl = manager.compileTemplate(text);
std::string result1 = manager.replacePlaceholders(tpl, manager.makeContext(somePlayer));
std::string result2 = manager.replacePlaceholders(tpl, manager.makeContext(anotherPlayer));
```
`replacePlaceholders` 接受多种上下文参数：`Player*`, `T*` (模板), `PlaceholderContext`, `std::any`。

#### B. 异步替换

当文本中可能包含异步占位符时，使用异步替换。它会返回一个 `std::future<std::string>`，你可以稍后获取结果或将其链接到其他任务。

```cpp
std::string textWithAsync = "异步任务结果: {myplugin:long_task}";

std::future<std::string> futureResult = manager.replacePlaceholdersAsync(textWithAsync, somePlayer);

// 在需要时获取结果（这将阻塞直到所有异步操作完成）
std::string finalResult = futureResult.get();
```

#### C. 批量替换

`replacePlaceholdersBatch` 允许你对多个模板使用同一个上下文进行一次性替换，这比循环调用 `replacePlaceholders` 更高效，因为它共享了请求内缓存。

```cpp
PA::CompiledTemplate tpl1 = manager.compileTemplate("Line 1: {myplugin:player_name}");
PA::CompiledTemplate tpl2 = manager.compileTemplate("Line 2: {myplugin:player_pos}");

std::vector<std::reference_wrapper<const PA::CompiledTemplate>> tpls = {tpl1, tpl2};
std::vector<std::string> results = manager.replacePlaceholdersBatch(tpls, manager.makeContext(player));
// results[0] = "Line 1: Steve"
// results[1] = "Line 2: X: 10, Y: 64, Z: 20"
```

### 4. 类型系统管理

#### 注册继承关系

如果你的对象存在继承关系，注册它们能让基类的占位符对派生类生效。

```cpp
class MyCustomPlayer : public Player { /* ... */ };

// 注册继承关系
manager.registerInheritance<MyCustomPlayer, Player>();
```

#### 注册类型别名

为复杂的模板类型指定一个易于理解的别名，方便在日志和查询中识别。

```cpp
manager.registerTypeAlias<Player>("Player");
```

### 5. 查询与管理

#### 查询占位符

-   `hasPlaceholder(plugin, name, typeKey)`: 检查是否存在指定的占位符。
-   `getAllPlaceholders()`: 获取所有已注册占位符的详细信息。

#### 缓存管理

-   `clearCache()`: 清理所有占位符的全局缓存。
-   `clearCache(pluginName)`: 清理特定插件的所有占位符缓存。

#### 全局配置

-   `setMaxRecursionDepth(depth)`: 设置占位符嵌套替换的最大深度，防止无限循环。
-   `setDoubleBraceEscape(enable)`: 启用或禁用 `{{ }}` 作为 `{` 的转义。

---

## 高级主题

### 占位符完整格式

-   **基本格式**: `{plugin:placeholder}`
-   **带参数**: `{plugin:placeholder|key1=val1|key2=val2}`
-   **带默认值**: `{plugin:placeholder:-默认值}` (如果占位符不存在或返回空，则显示默认值)
-   **嵌套**: `{plugin:outer|param={plugin:inner}}`
-   **转义**:
    -   使用 `{{` 和 `}}` 来输出 `{` 和 `}`。
    -   使用 `%%` 来输出 `%`。

### 内置参数

这些参数由系统处理，可用于所有占位符：

-   `allowempty=true`: 允许占位符返回空字符串。默认情况下，空返回被视为“未替换”，会触发默认值逻辑。
-   `separator=, `: 用于**列表占位符**，定义各项之间的分隔符。
-   `join=, `: 用于**对象列表占位符**，定义各项之间的分隔符。
-   `template=...`: 仅用于**对象列表占位符**，为列表中的每个对象应用此模板。

### 格式化参数

`Utils.h` 提供了一些内置的格式化参数，可用于处理数字和字符串结果：

-   `format=...`: 使用 `fmt::format` 格式化字符串（用于数字）。
-   `max_length=N`: 将结果截断到最大长度 `N`。
-   `random=N`: 从结果中随机选择 `N` 个字符。
-   `replace_A=B`: 将结果中的所有 `A` 替换为 `B`。
-   `math=...`: 执行数学表达式，用 `VALUE` 代表占位符的原始数字结果。例如 `math=(VALUE * 10) + 5`。
