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

**新增方法：**

*   **`registerCachedRelationalPlaceholder(std::string_view prefix, std::shared_ptr<const IPlaceholder> p, void* owner, uint64_t mainContextTypeId, uint64_t relationalContextTypeId, unsigned int cacheDuration)`**：注册一个带缓存的关系型占位符。它与 `registerRelationalPlaceholder` 类似，但会根据 `cacheDuration` 对占位符的求值结果进行缓存。
*   **`registerContextAlias(...)`**: 注册一个上下文别名适配器，用于在不同上下文之间转换。
*   **`registerContextFactory(...)`**: 注册一个上下文工厂，用于在解析别名时动态构造自定义的上下文实例。
*   **`std::unique_ptr<IScopedPlaceholderRegistrar> createScopedRegistrar(void* owner)`**：创建一个 RAII 作用域注册器。通过此注册器注册的占位符会在注册器对象离开作用域时自动注销，极大地简化了资源管理。

### 4. 上下文别名 (Context Alias)

上下文别名允许你将一个占位符的求值环境从一个上下文（来源）动态切换到另一个上下文（目标）。这对于复用现有占位符非常有用。

例如，一个玩家可能正在看着一个生物。你希望在玩家的上下文中，获取被看着的生物的生命值。如果已经有一个在 `MobContext` 下工作的 `{mob_health}` 占位符，你可以注册一个名为 `look` 的别名，它能将 `PlayerContext` 解析为玩家视线所及的 `MobContext`。这样，你就可以直接使用 `{look:mob_health}` 来获取信息，而无需为“玩家看着的生物的生命值”编写一个全新的占位符。

这通过 `registerContextAlias` 方法实现，它需要一个**解析器函数 (Resolver Function)**。

*   **`ContextResolverFn`**: 这是一个函数指针，类型为 `void* (*)(const IContext*, const std::vector<std::string_view>& args)`。它的作用是接收来源上下文，并返回一个指向目标上下文所需**底层对象**的 `void*` 指针（例如，从 `Player*` 返回 `Mob*`）。

### 5. 上下文工厂 (Context Factory)

当上下文别名成功解析出目标底层对象后（例如，通过 `ContextResolverFn` 得到了一个 `Mob*`），占位符系统需要将这个底层对象包装成一个临时的目标上下文实例（例如 `MobContext`）。

对于内置的上下文类型，系统可以自动处理。但如果你的插件定义了**自定义的上下文类型**，你就需要提供一个**上下文工厂**。

*   **`ContextFactoryFn`**: 这是一个函数指针，类型为 `std::unique_ptr<IContext> (*)(void* rawObject)`。它的作用是接收一个指向底层对象的 `void*` 指针，并返回一个包含该对象的、新创建的上下文实例 (`std::unique_ptr<IContext>`)。

通过 `registerContextFactory` 方法注册工厂后，占位符系统就能在解析别名时，为你的自定义上下文类型动态创建实例，从而让别的插件也可以构造临时目标的上下文。

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

#### 推荐方式：使用简化宏

从最新版本开始，强烈推荐使用 `CommonPlaceholderTemplates.h` 中提供的简化宏来注册占位符。这些宏大幅简化了代码，提高了可读性和可维护性。

**可用的宏：**

1. **`PA_SIMPLE(svc, owner, ctx_type, token_str, lambda_body)`** - 简单上下文占位符（无参数）
2. **`PA_CACHED(svc, owner, ctx_type, token_str, cache_duration, lambda_body)`** - 带缓存的上下文占位符
3. **`PA_WITH_ARGS(svc, owner, ctx_type, token_str, lambda_body)`** - 带参数的上下文占位符
4. **`PA_WITH_ARGS_CACHED(svc, owner, ctx_type, token_str, cache_duration, lambda_body)`** - 带参数且带缓存的上下文占位符
5. **`PA_SERVER(svc, owner, token_str, lambda_body)`** - 服务器级占位符（无参数）
6. **`PA_SERVER_CACHED(svc, owner, token_str, cache_duration, lambda_body)`** - 带缓存的服务器级占位符
7. **`PA_SERVER_WITH_ARGS(svc, owner, token_str, lambda_body)`** - 带参数的服务器级占位符
8. **`PA_SERVER_WITH_ARGS_CACHED(svc, owner, token_str, cache_duration, lambda_body)`** - 带参数且带缓存的服务器级占位符

**使用示例：**

```cpp
#include "PA/PlaceholderAPI.h"
#include "PA/Placeholders/CommonPlaceholderTemplates.h"

void registerMyPlaceholders(PA::IPlaceholderService* svc) {
    static int kOwnerTag = 0;
    void* owner = &kOwnerTag;

    // 1. 简单的玩家占位符
    PA_SIMPLE(svc, owner, PA::PlayerContext, "{player_custom_name}", {
        out = c.player ? c.player->getRealName() : "Unknown";
    });

    // 2. 带缓存的服务器占位符（缓存60秒）
    PA_SERVER_CACHED(svc, owner, "{server_motd}", 60, {
        out = "欢迎来到我的服务器！";
    });

    // 3. 带参数的占位符
    PA_WITH_ARGS(svc, owner, PA::PlayerContext, "{player_custom_data}", {
        if (!args.empty()) {
            std::string key(args[0]);
            out = getPlayerData(c.player, key);
        } else {
            out = "请提供数据键";
        }
    });

    // 4. 带参数且带缓存的占位符（缓存30秒）
    PA_WITH_ARGS_CACHED(svc, owner, PA::PlayerContext, "{player_rank}", 30, {
        if (!args.empty()) {
            std::string rankType(args[0]);
            out = getPlayerRank(c.player, rankType);
        } else {
            out = getPlayerDefaultRank(c.player);
        }
    });

    // 5. 带参数的服务器级占位符
    PA_SERVER_WITH_ARGS(svc, owner, "{world_info}", {
        if (!args.empty()) {
            std::string worldName(args[0]);
            out = getWorldInfo(worldName);
        } else {
            out = "请提供世界名称";
        }
    });
}
```

**宏参数说明：**
- `svc`: `IPlaceholderService*` 服务指针
- `owner`: `void*` 所有者标识，用于批量注销
- `ctx_type`: 上下文类型（如 `PA::PlayerContext`, `PA::ActorContext` 等）
- `token_str`: 占位符标识字符串（如 `"{player_name}"`）
- `cache_duration`: 缓存持续时间（秒）
- `lambda_body`: Lambda 函数体，可以直接访问：
  - `c`: 上下文对象（类型为 `const ctx_type&`）
  - `out`: 输出字符串引用（类型为 `std::string&`）
  - `args`: 参数向量（仅在 `_WITH_ARGS` 宏中可用，类型为 `const std::vector<std::string_view>&`）

**优势：**
- 代码量减少 50% 以上
- 更清晰的代码结构
- 减少模板代码错误
- 统一的代码风格
- 更易于维护

#### 传统方式：实现 IPlaceholder 接口

对于需要更复杂逻辑或特殊需求的场景，也可以直接实现 `PA::IPlaceholder` 接口来创建自定义占位符。

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

// 使用 IScopedPlaceholderRegistrar 简化注册和自动卸载
void useScopedRegistrar(PA::IPlaceholderService* svc, void* owner) {
    // 创建一个作用域注册器，当 registrar 离开作用域时，会自动注销其名下所有占位符
    auto registrar = svc->createScopedRegistrar(owner);

    // 通过 registrar 注册占位符，无需手动调用 unregisterByOwner
    class MyScopedPlaceholder final : public PA::IPlaceholder {
    public:
        std::string_view token() const noexcept override { return "{scoped_greet}"; }
        uint64_t         contextTypeId() const noexcept override { return PA::kServerContextId; }
        void evaluate(const PA::IContext* ctx, std::string& out) const override {
            out = "Hello from scoped registrar!";
        }
    };
    registrar->registerPlaceholder("", std::make_shared<MyScopedPlaceholder>(), owner);

    // 也可以注册带缓存的关系型占位符
    class MyScopedCachedRelationalPlaceholder final : public PA::IPlaceholder {
    public:
        std::string_view token() const noexcept override { return "{scoped_cached_relational}"; }
        uint64_t         contextTypeId() const noexcept override { return PA::TypeId("ctx:Player"); } // 示例：绑定到 Player
        unsigned int     getCacheDuration() const noexcept override { return 30; } // 缓存 30 秒

        void evaluate(const PA::IContext* ctx, std::string& out) const override {
            out = "Scoped cached relational placeholder value.";
        }
    };
    registrar->registerCachedRelationalPlaceholder(
        "",
        std::make_shared<MyScopedCachedRelationalPlaceholder>(),
        PA::TypeId("ctx:Player"), // 主上下文类型
        PA::TypeId("ctx:Mob"),    // 关系上下文类型
        30                         // 缓存持续时间
    );
}
```

### 4. 注册上下文别名和工厂（高级）

以下示例展示了如何注册一个自定义上下文、工厂和别名，以实现 `{my_alias:custom_value}` 的功能。

```cpp
#include "PA/PlaceholderAPI.h"

// 假设你有一个自定义的数据结构和上下文
struct MyData { int value; };
struct MyDataContext : public PA::IContext {
    static constexpr uint64_t kTypeId = PA::TypeId("ctx:MyData");
    const MyData* data{};
    uint64_t typeId() const noexcept override { return kTypeId; }
};

// 工厂函数：从 void* 创建 MyDataContext
std::unique_ptr<PA::IContext> createMyDataContext(void* raw) {
    auto ctx = std::make_unique<MyDataContext>();
    ctx->data = static_cast<const MyData*>(raw);
    return ctx;
}

// 解析器函数：从 PlayerContext 获取 MyData
void* resolveMyDataFromPlayer(const PA::IContext* fromCtx, const std::vector<std::string_view>&) {
    // 在真实场景中，你会从玩家身上查找关联的数据
    static MyData dummyData{42};
    return &dummyData;
}

// 占位符，作用于 MyDataContext
class MyDataPlaceholder final : public PA::IPlaceholder {
public:
    std::string_view token() const noexcept override { return "{custom_value}"; }
    uint64_t contextTypeId() const noexcept override { return MyDataContext::kTypeId; }
    void evaluate(const PA::IContext* ctx, std::string& out) const override {
        const auto* myCtx = static_cast<const MyDataContext*>(ctx);
        if (myCtx && myCtx->data) {
            out = std::to_string(myCtx->data->value);
        }
    }
};

// 注册流程
void registerMyAliasAndFactory(PA::IPlaceholderService* svc, void* owner) {
    auto registrar = svc->createScopedRegistrar(owner);

    // 1. 注册作用于自定义上下文的占位符
    registrar->registerPlaceholder("", std::make_shared<MyDataPlaceholder>());

    // 2. 注册自定义上下文的工厂
    registrar->registerContextFactory(MyDataContext::kTypeId, createMyDataContext);

    // 3. 注册别名，将 PlayerContext 链接到 MyDataContext
    registrar->registerContextAlias(
        "my_alias",
        PA::PlayerContext::kTypeId,
        MyDataContext::kTypeId,
        resolveMyDataFromPlayer
    );
}
```

**注意：** `owner` 指针用于标识占位符的归属模块，建议使用模块内唯一的地址作为 `owner`，以便在模块卸载时批量反注册。
