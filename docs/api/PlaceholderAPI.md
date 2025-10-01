# PlaceholderAPI 占位符系统

PlaceholderAPI (PA) 是一个功能强大且高度可扩展的占位符系统。它允许插件注册动态文本片段（占位符），这些文本片段可以在需要时被替换为实际内容。该系统支持多种类型的占位符，包括服务器范围的、与特定对象（上下文）相关的、关系型的，并且支持同步和异步处理，以及高效的缓存和模板编译机制。

## 核心概念

在深入了解如何使用 API 之前，理解以下几个核心概念至关重要：

### 1. PlaceholderManager (占位符管理器)

`PlaceholderManager` 是整个 API 的核心和入口。它是一个单例类，您可以通过 `PA::PlaceholderManager::getInstance()` 获取其实例。所有操作，如注册、替换和配置，都通过这个管理器进行。

### 2. PlaceholderContext (占位符上下文)

上下文（Context）是执行占位符替换时传递的数据。它包含了指向具体对象（如 `Player*`）的指针及其内部类型 ID。这使得系统能够根据对象的实际类型找到最匹配的占位符。

-   **普通上下文**: 包含一个对象指针。
-   **关系型上下文**: 包含两个对象指针，用于处理需要两个对象关系的占位符（例如 `player_relation_is_enemy`）。

### 3. 类型系统 (Type System)

系统内部维护着一个类型系统，用于管理 C++ 类型与内部 ID 之间的映射。
-   **类型安全**: 通过模板，API 能在编译期确定类型，并自动处理类型注册。
-   **继承支持**: 您可以注册类之间的继承关系。例如，如果注册了 `Player` 的占位符，当传入一个 `ServerPlayer` 对象（假设其继承自 `Player`）时，该占位符依然可以被正确解析。

### 4. 模板编译 (Template Compilation)

为了提高重复替换的性能，系统引入了模板编译机制。
-   `compileTemplate(text)`: 将一个包含占位符的字符串预先解析成一个优化的 `CompiledTemplate` 对象。
-   `replacePlaceholders(template, context)`: 使用编译后的模板进行替换，避免了每次替换都重新解析字符串，从而大幅提升性能。

### 5. 缓存 (Caching)

系统内置了多级缓存：
-   **全局缓存**: 缓存占位符的最终结果，并可设置过期时间。
-   **编译缓存**: 缓存编译后的模板。
-   **请求内缓存**: 在单次替换请求中，如果同一个占位符被多次调用，只会计算一次。

## 使用指南

### 获取管理器实例

```cpp
auto& manager = PA::PlaceholderManager::getInstance();
```

### 注册占位符

API 提供了丰富的注册函数，以满足不同场景的需求。

#### 1. 服务器占位符 (Server Placeholders)

这类占位符不依赖任何特定对象，其值在整个服务器范围内是通用的。

**示例：注册一个返回服务器在线人数的占位符 `myserver:online_players`**

```cpp
#include "ll/api/service/Bedrock.h"
#include "mc/server/ServerPlayer.h"

manager.registerServerPlaceholder("myserver", "online_players", []() {
    auto players = ll::service::getLevel()->getPlayers();
    return std::to_string(players.size());
});
```

#### 2. 上下文占位符 (Context Placeholders)

这类占位符的值与传入的特定对象（上下文）相关。这是最常用的一种占位符。

**示例：注册一个返回玩家名称的占位符 `myplugin:player_name`**

```cpp
#include "mc/world/actor/player/Player.h"

// 注册 Player* 类型的占位符
manager.registerPlaceholder<Player>("myplugin", "player_name", [](Player* player) {
    if (player) {
        return player->getRealName();
    }
    return std::string{}; // 返回空字符串
});
```

#### 3. 带参数的占位符

几乎所有类型的占位符都支持参数。参数在占位符名称后用 `|` 分隔。

**示例：注册一个格式化玩家坐标的占位符 `myplugin:player_pos`**
该占位符接受一个 `format` 参数，如 `{myplugin:player_pos|format=X: %.1f, Y: %.1f, Z: %.1f}`

```cpp
#include "mc/world/actor/player/Player.h"
#include "PA/Utils.h" // for ParsedParams
#include <fmt/core.h>

manager.registerPlaceholderWithParams<Player>(
    "myplugin",
    "player_pos",
    [](Player* player, const PA::Utils::ParsedParams& params) {
        if (!player) return std::string{};

        // 从参数中获取格式字符串，如果不存在则使用默认值
        std::string format = std::string(params.get("format").value_or("X: %d, Y: %d, Z: %d"));
        auto& pos = player->getPosition();

        try {
            return fmt::format(format, pos.x, pos.y, pos.z);
        } catch (const std::exception& e) {
            return "Invalid format string";
        }
    }
);
```

#### 4. 关系型占位符 (Relational Placeholders)

用于处理需要两个对象进行比较或交互的场景。

**示例：注册一个判断玩家P1是否在看玩家P2的占位符 `myplugin:is_looking_at`**

```cpp
manager.registerRelationalPlaceholder<Player, Player>(
    "myplugin",
    "is_looking_at",
    [](Player* p1, Player* p2) {
        // 此处应实现具体的逻辑来判断 p1 是否在注视 p2
        // ...
        bool isLooking = false; // 假设的逻辑结果
        return isLooking ? "true" : "false";
    }
);
```

#### 5. 列表占位符 (List Placeholders)

如果一个占位符需要返回一个项目列表，可以使用列表占位符。系统会自动使用指定的分隔符（默认为 `, `）将它们连接起来。

**示例：返回服务器所有玩家名字的列表 `myserver:all_players_list`**

```cpp
manager.registerServerListPlaceholder("myserver", "all_players_list", []() {
    std::vector<std::string> names;
    for (auto& player : ll::service::getLevel()->getPlayers()) {
        names.push_back(player->getName());
    }
    return names;
});

// 使用时可以通过 `separator` 参数指定分隔符
// {myserver:all_players_list|separator=; }
```

#### 6. 异步占位符

对于耗时的操作（如数据库查询、网络请求），应注册异步占位符，以避免阻塞服务器主线程。

**示例：一个模拟耗时操作的异步占位符**

```cpp
#include <chrono>
#include <thread>

manager.registerAsyncServerPlaceholder("myplugin", "long_task", []() {
    // 返回一个 std::future<std::string>
    return std::async(std::launch::async, [] {
        // 在另一个线程中执行耗时操作
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return "Task Complete";
    });
});
```

### 替换占位符

提供了同步和异步的替换方法。

#### 同步替换

```cpp
Player* somePlayer = ...;
std::string text = "玩家 {myplugin:player_name} 的坐标是 {myplugin:player_pos}";

// 1. 直接替换
std::string result = manager.replacePlaceholders(text, somePlayer);

// 2. 使用编译模板（推荐用于重复替换）
PA::CompiledTemplate tpl = manager.compileTemplate(text);
std::string result1 = manager.replacePlaceholders(tpl, somePlayer);
std::string result2 = manager.replacePlaceholders(tpl, anotherPlayer);
```

#### 异步替换

当文本中可能包含异步占位符时，使用异步替换。

```cpp
std::string textWithAsync = "异步任务结果: {myplugin:long_task}";

std::future<std::string> futureResult = manager.replacePlaceholdersAsync(textWithAsync, somePlayer);

// 你可以在未来的某个时间点获取结果
// futureResult.get(); // 这会阻塞直到结果可用
```

### 注册类型信息

#### 继承关系

如果你的对象存在继承关系，注册它们能让基类的占位符对派生类生效。

```cpp
class MyBasePlayer : public Player { ... };

// 注册继承关系
manager.registerInheritance<MyBasePlayer, Player>();
```

#### 类型别名

为复杂的模板类型指定一个易于理解的别名。

```cpp
manager.registerTypeAlias<Player>("Player");
```

## 高级主题

### 占位符格式

-   **基本格式**: `{plugin:placeholder}`
-   **带参数**: `{plugin:placeholder|key1=val1|key2=val2}`
-   **带默认值**: `{plugin:placeholder:-默认值}` (如果占位符不存在或返回空，则显示默认值)
-   **转义**:
    -   使用 `{{` 和 `}}` 来输出 `{` 和 `}`。
    -   使用 `%%` 来输出 `%`。

### 内置参数

-   `allowempty=true`: 允许占位符返回空字符串。默认情况下，空返回被视为“未替换”，会触发默认值。
-   `separator=, `: 用于列表占位符，定义各项之间的分隔符。
-   `join=, `: 用于对象列表占位符，定义各项之间的分隔符。
-   `template=...`: 用于对象列表占位符，为列表中的每个对象应用此模板。

### 缓存策略

注册占位符时，可以指定 `CacheKeyStrategy`:
-   `Default`: 默认策略，缓存键与上下文对象相关。
-   `ServerOnly`: 缓存键与上下文无关，即使传入不同玩家，也会命中相同的服务器级缓存。适用于那些虽然需要上下文（例如获取玩家所在的世界），但结果对于该世界的所有玩家都相同的情况。
