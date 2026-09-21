# PlaceholderAPI

适用于 LeviLamina (BDS) 的占位符系统。它让你在任意文本中写入 `{占位符}`，运行时自动替换为玩家、实体、世界或服务器的实时数据，并支持参数、条件输出、着色、缓存等格式化能力。

```text
你好, {player_realname}! 你的延迟：{player_ping:|map=>200:§c高;>100:§e中;§a低}
        ↓
你好, Steve! 你的延迟：§a低
```

## 功能特性

- **丰富的内置占位符**：玩家、实体、生物、方块、容器、物品、世界、时间、系统、服务器等。完整列表见 [内置占位符文档](BUILTIN_PLACEHOLDERS.md)。
- **强大的格式化管线**：数值精度、条件映射 (`map`)、布尔/字符/正则/JSON 映射、阈值着色等。
- **上下文继承**：`Player → Mob → Actor`，玩家上下文可直接复用实体/生物占位符。
- **上下文别名**：如 `{look:actor_health}` 获取玩家视线所指生物的血量，无需重写占位符。
- **缓存**：对不常变的值按秒级缓存，降低开销。
- **多语言接入**：C++ API、C ABI（C/Rust/Go/Zig 等 FFI）、以及通过 RemoteCall 的 JS API。

## 安装

1. 确保服务器已安装 [LeviLamina](https://github.com/LiteLDev/LeviLamina) 26.51.1 及插件依赖 `LegacyRemoteCall`。
2. 将本插件（`Placeholder`）放入服务器 `plugins/` 目录。
3. 重启服务器。控制台出现加载日志即表示成功。

> 本插件是其它插件/脚本的**基础库**：它本身负责解析与替换占位符，具体占位符由内置集合与各插件注册提供。

## 快速上手（服务器管理员 / 脚本作者）

占位符的基本写法是 `{token}`，可带参数与格式化指令：

```text
{token:业务参数...|格式化参数...}
```

常用示例：

```text
{player_realname}                                   玩家名
{player_pos_x:|precision=2}                          保留两位小数的坐标
{player_ping:|50,§a,100,§e,§c}ms                     按阈值着色的延迟
{actor_is_alive:|bool_map=true:§a存活;false:§c死亡}   布尔值本地化
{look:actor_health}                                  视线所指生物的血量（上下文别名）
```

完整的语法、参数分流规则（`:` `,` `;` `|`）、格式化参数与排错，见 **[使用指南 USAGE_GUIDE.md](USAGE_GUIDE.md)**。

## 文档导航

| 你是… | 推荐阅读 |
|---|---|
| 服务器管理员 / 配置占位符的人 | [USAGE_GUIDE.md](USAGE_GUIDE.md) · [BUILTIN_PLACEHOLDERS.md](BUILTIN_PLACEHOLDERS.md) |
| C++ / C ABI 插件开发者 | [API_DOC.md](API_DOC.md) |
| JS 脚本开发者（RemoteCall） | 下方「JS / RemoteCall 接入」 · [ExamplePlugin.js](ExamplePlugin.js) |
| 想了解版本变化 | [CHANGELOG.md](CHANGELOG.md) |

## JS / RemoteCall 接入

通过 `ll.import("PA", <函数名>)` 即可在 JS 中调用。可用导出函数：

**文本替换**

| 函数 | 说明 |
|---|---|
| `replace(text)` | 服务器级替换（无上下文） |
| `replaceForPlayer(text, player)` | 以玩家为上下文替换 |
| `replaceForActor(text, actor)` | 以实体为上下文替换 |
| `replaceMany(texts)` / `replaceManyForPlayer(texts, player)` | 批量替换 |
| `replaceObject(kv)` / `replaceObjectForPlayer(kv, player)` | 替换键值对象的值 |

**注册自定义占位符**（回调通过 `ll.export(fn, namespace, name)` 提供）

| 函数 | 上下文 |
|---|---|
| `registerServerPlaceholder(prefix, token, cbNS, cbName, cacheDuration?)` | 服务器级 |
| `registerPlayerPlaceholder(prefix, token, cbNS, cbName, cacheDuration?)` | 玩家 |
| `registerActorPlaceholder(prefix, token, cbNS, cbName, cacheDuration?)` | 实体 |
| `registerMobPlaceholder(prefix, token, cbNS, cbName, cacheDuration?)` | 生物 |
| `registerPlaceholderByKind(...)` / `registerPlaceholderByContextId(...)` | 进阶 |
| `unregisterByCallbackNamespace(cbNS)` | 按命名空间批量注销 |
| `contextTypeIds()` | 查询各上下文类型 ID |

> `cacheDuration` 传大于 0 的秒数即启用缓存。

最小示例：

```javascript
const PA = {
    replaceForPlayer: ll.import("PA", "replaceForPlayer"),
    registerPlayerPlaceholder: ll.import("PA", "registerPlayerPlaceholder"),
    unregisterByCallbackNamespace: ll.import("PA", "unregisterByCallbackNamespace"),
};

const NS = "MyScript";

// 回调签名：(token, args, player) => string
ll.export((token, args, player) => `你好，${player ? player.name : "?"}`, NS, "hello");

// 注册后即可使用 {js:hello}
PA.registerPlayerPlaceholder("js", "hello", NS, "hello", 0);

mc.listen("onJoin", (player) => {
    player.tell(PA.replaceForPlayer("欢迎, {js:hello}!", player));
});

// 卸载时清理
ll.registerPluginUnload && ll.registerPluginUnload(() => PA.unregisterByCallbackNamespace(NS));
```

可直接运行的完整示例（含实体坐标、缓存占位符、侧边栏更新等）见仓库根目录的 **[ExamplePlugin.js](ExamplePlugin.js)**。

## C++ / C ABI 接入

C++ 推荐使用 `CommonPlaceholderTemplates.h` 中的简化宏注册占位符；需要跨编译器/语言时使用 `PA/PlaceholderCAPI.h` 暴露的 C ABI。详见 [API_DOC.md](API_DOC.md)。

## 许可证

见 [LICENSE](LICENSE)。
