# Placeholder API 文档

本文档旨在为开发者提供 Placeholder API 的详细说明，包括核心概念、内置占位符、参数解析以及如何通过 C++ 和 JavaScript (通过 RemoteCall) 调用和扩展此 API。

## 核心概念

Placeholder API 允许开发者在文本中定义可替换的占位符，这些占位符会根据不同的上下文和参数进行动态求值。

### 1. 上下文 (Context)

上下文是占位符求值时所需的数据环境。API 定义了以下几种上下文：

*   **`PA::IContext`**：所有上下文的基类。
    *   `typeId()`：返回一个唯一的 `uint64_t` 类型 ID。
    *   `getInheritedTypeIds()`：返回所有继承的上下文类型 ID 列表，包括自身。
*   **`PA::kServerContextId`**：服务器级上下文 ID (值为 `0`)。用于不依赖特定实体或玩家的占位符。
*   **`PA::ActorContext`**：用于游戏中的任何 `Actor` 实体。
    *   `kTypeId`：`TypeId("ctx:Actor")`
    *   包含 `Actor* actor` 成员。
*   **`PA::MobContext`**：继承自 `ActorContext`，用于 `Mob` 实体。
    *   `kTypeId`：`TypeId("ctx:Mob")`
    *   包含 `Mob* mob` 成员。
*   **`PA::PlayerContext`**：继承自 `MobContext`，用于 `Player` 实体。
    *   `kTypeId`：`TypeId("ctx:Player")`
    *   包含 `Player* player` 成员。

**上下文继承关系：** `PlayerContext` -> `MobContext` -> `ActorContext` -> `IContext`。这意味着一个 `PlayerContext` 也可以作为 `MobContext` 或 `ActorContext` 使用。

### 2. 占位符 (Placeholder)

占位符是实现 `PA::IPlaceholder` 接口的对象。

*   **`token()`**：返回占位符的字符串标识，例如 `"{player_name}"`。
*   **`contextTypeId()`**：返回此占位符绑定的上下文类型 ID。
*   **`evaluate(const IContext* ctx, std::string& out)`**：根据上下文计算并返回替换文本。
*   **`evaluateWithArgs(const IContext* ctx, const std::vector<std::string_view>& args, std::string& out)`**：带参数的求值方法，用于处理原生参数。
*   **`getCacheDuration()`**：返回占位符的缓存持续时间（秒）。返回 `0` 表示不缓存。

#### 缓存占位符 (Cached Placeholder)

对于一些不频繁变更的变量，例如服务器版本等信息，可以使用缓存来提升性能。任何实现 `PA::IPlaceholder` 接口的占位符，如果其 `getCacheDuration()` 方法返回一个大于 `0` 的值，都将被自动缓存。缓存的键将根据上下文实例和占位符参数动态生成，以确保缓存的准确性和线程安全。

### 3. 占位符服务 (Placeholder Service)

`PA::IPlaceholderService` 是用于管理和替换占位符的核心接口。通过 `PA::PA_GetPlaceholderService()` 函数可以获取其单例。

## 内置占位符

Placeholder API 提供了丰富的内置占位符。详细列表请参阅 [内置占位符文档](BUILTIN_PLACEHOLDERS.md)。
## 占位符参数解析

占位符的参数分为两类：**原生参数**和**格式化参数**。所有参数都在占位符名称后通过冒号 `:` 分隔，多个参数之间用逗号 `,` 分隔。

**格式：** `{token:arg1,arg2,...,format_param1=value,...}`

### 1. 原生参数

原生参数直接传递给占位符的 `evaluateWithArgs` 方法，由占位符自身逻辑进行处理。这允许创建更灵活、功能更强大的占位符。

**示例：**
假设我们有一个 `{player_money}` 占位符，用于查询玩家不同类型的货币。
*   `{player_money:gold}`：查询金币数量。
*   `{player_money:silver}`：查询银币数量。

在这个例子中，`gold` 和 `silver` 就是原生参数，会被传递给 `{player_money}` 的实现。

### 2. 格式化参数

格式化参数由 Placeholder API 的处理器在占位符求值后统一处理，用于对结果进行格式化、条件输出或着色。

#### a. 数值精度 (`precision`)

用于格式化数值的输出精度。

**用法：** `{health:precision=2}`
**示例：** `{health:precision=2}` 如果生命值为 `19.567`，则输出 `19.57`。

#### b. 条件输出 (`map`)

根据占位符的数值结果进行条件判断，并输出不同的文本。

**用法：** `{value:map=>10:高;<=5:低;中}`
**格式：** `map=[条件1];[条件2];...;[else]`
*   每个条件格式：`[运算符][阈值]:[输出]`
*   运算符支持：`>`, `<`, `=`, `>=`, `<=`, `!=`
*   `else` 部分在所有条件都不满足时输出。
*   `{value}` 可以在输出字符串中代表原始评估值。

**示例：** `{health:map=>15:健康;>5:受伤;濒危}`
*   如果 `{health}` 为 `20`，输出 `健康`。
*   如果 `{health}` 为 `10`，输出 `受伤`。
*   如果 `{health}` 为 `3`，输出 `濒危`。

#### c. 布尔值映射 (`bool_map`)

将占位符的布尔值（字符串 "true" 或 "false"）映射到自定义字符串。

**用法：** `{actor_is_alive:bool_map=true:活着;false:死亡}`
**格式：** `bool_map=[原始值1]:[映射值1];[原始值2]:[映射值2]`

**示例：** `{actor_is_alive:bool_map=true:存活;false:已故}`
*   如果 `{actor_is_alive}` 为 `true`，输出 `存活`。
*   如果 `{actor_is_alive}` 为 `false`，输出 `已故`。

#### d. 颜色规则

占位符支持应用颜色代码。

**用法：** `{player_name:§c}` (直接指定颜色代码) 或 `{health:10,§a,5,§e,§c}` (阈值颜色)
**格式：**
*   **单颜色：** `{token:颜色代码}`
*   **阈值颜色：** `{token:阈值1,颜色1,阈值2,颜色2,...,默认颜色}`
    *   当值小于 `阈值N` 时，应用 `颜色N`。
    *   如果所有阈值都不满足，则应用 `默认颜色`。
    *   颜色代码可以使用 `PA_COLOR_RED` 等宏定义的字符串，或直接使用 `§` 符号开头的 Minecraft 颜色代码。

**示例：** `{health:10,§c,20,§e,§a}`
*   如果 `{health}` 为 `5`，输出 `§c5`。
*   如果 `{health}` 为 `15`，输出 `§e15`。
*   如果 `{health}` 为 `25`，输出 `§a25`。

#### e. 颜色格式 (`color_format`)

自定义颜色规则的输出格式。默认格式为 `{color}{value}`。

**用法：** `{health:10,§c,§a,color_format={value} {color}}`
**示例：** `{health:10,§c,§a,color_format={value} {color}}`
*   如果 `{health}` 为 `5`，输出 `5 §c`。
*   如果 `{health}` 为 `15`，输出 `15 §a`。

## C++ API 调用

### 1. 获取服务实例

```cpp
#include "PA/PlaceholderAPI.h"

// 获取占位符服务单例
PA::IPlaceholderService* service = PA::PA_GetPlaceholderService();
if (!service) {
    // 处理错误
    return;
}
```

### 2. 替换占位符

*   **服务器级替换 (无上下文)：**
    ```cpp
    std::string text = "当前在线玩家: {online_players}";
    std::string result = service->replaceServer(text);
    // result: "当前在线玩家: 10"
    ```

*   **带上下文替换：**
    ```cpp
    #include "mc/world/actor/player/Player.h" // 假设 Player 类可用

    // 假设有一个 Player* player 对象
    Player* player = ...;
    PA::PlayerContext ctx;
    ctx.player = player;
    ctx.mob = player; // Player 继承自 Mob
    ctx.actor = player; // Player 继承自 Actor

    std::string text = "玩家 {player_name} 的 Ping: {ping}";
    std::string result = service->replace(text, &ctx);
    // result: "玩家 Steve 的 Ping: 50"
    ```

### 3. 注册自定义占位符

开发者可以实现 `PA::IPlaceholder` 接口来创建自定义占位符。

```cpp
#include "PA/PlaceholderAPI.h"
#include <iostream>

class MyCustomPlaceholderWithArgs final : public PA::IPlaceholder {
public:
    std::string_view token() const noexcept override { return "{greet}"; }
    uint64_t         contextTypeId() const noexcept override { return PA::kServerContextId; }

    // 无参数时的默认行为
    void evaluate(const PA::IContext* ctx, std::string& out) const override {
        out = "Hello, world!";
    }

    // 带参数的实现
    void evaluateWithArgs(
        const PA::IContext*                  ctx,
        const std::vector<std::string_view>& args,
        std::string&                         out
    ) const override {
        if (args.empty()) {
            evaluate(ctx, out); // 如果没有参数，调用默认实现
            return;
        }
        out = "Hello, ";
        out.append(args[0]); // 使用第一个参数作为名字
        out.append("!");
    }
};

// 在插件初始化时注册
void registerMyPlaceholder(PA::IPlaceholderService* svc, void* owner) {
    svc->registerPlaceholder("", std::make_shared<MyCustomPlaceholderWithArgs>(), owner);
}

// 注册一个缓存占位符 (例如，缓存 60 秒)
void registerMyCachedPlaceholder(PA::IPlaceholderService* svc, void* owner) {
    class MyCachedPlaceholder final : public PA::IPlaceholder {
    public:
        std::string_view token() const noexcept override { return "{cached_greet}"; }
        uint64_t         contextTypeId() const noexcept override { return PA::kServerContextId; }
        unsigned int     getCacheDuration() const noexcept override { return 60; } // 缓存 60 秒

        void evaluate(const PA::IContext* ctx, std::string& out) const override {
            out = "Hello from cache!";
        }
    };
    // 现在 registerPlaceholder 会根据 getCacheDuration() 自动处理缓存
    svc->registerPlaceholder("", std::make_shared<MyCachedPlaceholder>(), owner);
}

// 在插件卸载时反注册
void unregisterMyPlaceholder(PA::IPlaceholderService* svc, void* owner) {
    svc->unregisterByOwner(owner);
}
```
**注意：** `owner` 指针用于标识占位符的归属模块，建议使用模块内唯一的地址作为 `owner`，以便在模块卸载时批量反注册。
