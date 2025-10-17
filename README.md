# PlaceholderAPI JavaScript 插件示例使用说明

本文档将指导您如何使用 `ExamplePlugin.js`，这是一个基于 PlaceholderAPI 的 JavaScript 插件示例。

## 功能简介

该插件会在每一位玩家加入游戏时，向其发送一条个性化的欢迎消息。这条消息利用了 PlaceholderAPI 的强大功能，将预设文本中的占位符（如 `{player_name}`）替换为玩家的实际信息。

**新功能**: 本插件现在还演示了如何在 JavaScript 中注册自定义占位符，并与 PlaceholderAPI 配合使用，例如 `{js:hello}`、`{js:server_time}` 和 `{js:actor_pos}`。

## 安装步骤

1.  **前置要求**:
    *   确保您的服务器已经正确安装并加载了 `PlaceholderAPI` C++ 插件。这是本 JS 插件运行的基础。
    *   确保您的服务器支持加载 JavaScript 插件。

2.  **放置插件**:
    *   将 `ExamplePlugin.js` 文件复制到您服务器的 `plugins/` 目录下。

3.  **重启服务器**:
    *   重启您的服务器以加载新的 JS 插件。加载成功后，您应该能在控制台看到 "PlaceholderAPI JS 示例插件已成功加载，正在监听 onJoin 事件并注册 JS 占位符。" 的提示信息。

## 工作原理

插件的核心代码逻辑如下：

```javascript
// 插件名称：PlaceholderAPI JS 示例（含注册自定义占位符）
// 插件版本：1.1.0
// 插件描述：演示如何在 JS 中注册自定义占位符，并与 PlaceholderAPI 配合使用

// 1) 导入 PlaceholderAPI 暴露的函数
const PA = {
    replaceForPlayer: ll.import("PA", "replaceForPlayer"),
    registerActorPlaceholder: ll.import("PA", "registerActorPlaceholder"),
    registerServerPlaceholder: ll.import("PA", "registerServerPlaceholder"),
    registerPlaceholderByKind: ll.import("PA", "registerPlaceholderByKind"),
    registerPlaceholderByContextId: ll.import("PA", "registerPlaceholderByContextId"),

    // 新增：缓存占位符注册
    registerCachedServerPlaceholder: ll.import("PA", "registerCachedServerPlaceholder"),
    registerCachedPlayerPlaceholder: ll.import("PA", "registerCachedPlayerPlaceholder"),
    registerCachedActorPlaceholder: ll.import("PA", "registerCachedActorPlaceholder"),
    registerCachedPlaceholderByKind: ll.import("PA", "registerCachedPlaceholderByKind"),
    registerCachedPlaceholderByContextId: ll.import("PA", "registerCachedPlaceholderByContextId"),

    unregisterByCallbackNamespace: ll.import("PA", "unregisterByCallbackNamespace"),
    contextTypeIds: ll.import("PA", "contextTypeIds"),
};

// 2) 在 JS 中导出一个回调函数，供 C++ 在占位符求值时调用
// 回调命名空间
const JS_CB_NS = "JSPH";

// 玩家上下文占位符回调签名：std::string(std::string token, std::string param, Player* player)
ll.export((token, param, player) => {
    // token 是注册时传入的 tokenName（不带花括号），例如 "hello"
    // param 是占位符参数（如果用户写了 {js:hello:xxx}，param 为 "xxx"，否则为空字符串）
    const extra = param ? `（${param}）` : "";
    const name = player ? player.name : "未知玩家";
    return `你好，${name}${extra}`;
}, JS_CB_NS, "helloPlayer");

// 服务器级占位符回调签名：std::string(std::string token, std::string param)
ll.export((token, param) => {
    const now = new Date();
    return `服务器时间：${now.toLocaleString()}`;
}, JS_CB_NS, "serverTime");

// Actor 上下文占位符回调签名：std::string(std::string token, std::string param, Actor* actor)
ll.export((token, param, actor) => {
    if (!actor) return "无实体";
    const pos = actor.pos;
    return `实体坐标(${pos.x.toFixed(1)}, ${pos.y.toFixed(1)}, ${pos.z.toFixed(1)})`;
}, JS_CB_NS, "actorPos");

// 注册一个缓存的服务器级占位符，缓存时间为 5 秒
ll.export((token, param) => {
    const now = new Date();
    return `缓存服务器时间：${now.toLocaleString()}`;
}, JS_CB_NS, "cachedServerTime");

// 3) 向 PlaceholderAPI 注册这些占位符
// 最终占位符形如：{js:hello}、{js:server_time}、{js:actor_pos}、{js:cached_server_time}
const ok1 = PA.registerPlayerPlaceholder("js", "hello", JS_CB_NS, "helloPlayer");
const ok2 = PA.registerServerPlaceholder("js", "server_time", JS_CB_NS, "serverTime");
const ok3 = PA.registerActorPlaceholder("js", "actor_pos", JS_CB_NS, "actorPos");
const ok4 = PA.registerCachedServerPlaceholder("js", "cached_server_time", JS_CB_NS, "cachedServerTime", 5);

if (!ok1 || !ok2 || !ok3 || !ok4) {
    logger.error("注册 JS 占位符失败，请检查前面的日志。");
} else {
    logger.info("已注册 JS 占位符：{js:hello} / {js:server_time} / {js:actor_pos} / {js:cached_server_time}");
}

mc.listen("onJoin", (player) => {
    const msg = "欢迎, {player_name}! 现在时间：{js:server_time}，自定义问候：{js:hello:再次欢迎} {js:actor_pos}，缓存时间：{js:cached_server_time}";
    const processedMessage = PA.replaceForPlayer(msg, player);
    player.tell(processedMessage);
    logger.info(`向玩家 ${player.name} 发送了欢迎消息: ${processedMessage}`);
});

// 5) 插件卸载时，清理由本 JS 命名空间注册的占位符
ll.registerPluginUnload && ll.registerPluginUnload(() => {
    const ok = PA.unregisterByCallbackNamespace(JS_CB_NS);
    logger.info(`已卸载 '${JS_CB_NS}' 名下的占位符：${ok}`);
});

logger.info("PlaceholderAPI JS 示例插件已加载，正在监听 onJoin 事件并注册 JS 占位符。");
```

1.  **导入函数**: 插件首先通过 `ll.import` 从 C++ 插件中导入核心的占位符替换函数以及注册自定义占位符的函数。
2.  **导出回调**: 插件使用 `ll.export` 导出 JavaScript 函数，这些函数将作为自定义占位符的回调。`JS_CB_NS` 定义了这些回调的命名空间。
3.  **注册占位符**: 接着，插件使用 `PA.registerPlayerPlaceholder`、`PA.registerServerPlaceholder` 和 `PA.registerActorPlaceholder` 等函数向 PlaceholderAPI 注册自定义占位符，将它们与之前导出的 JS 回调函数关联起来。
4.  **监听事件**: 插件使用 `mc.listen("onJoin", ...)` 来监听玩家进入游戏的事件。
5.  **处理消息**: 当玩家加入时，插件会定义一条包含内置占位符和自定义 JS 占位符的欢迎语 `msg`。
6.  **替换占位符**: 然后调用 `PA.replaceForPlayer` 函数，将欢迎语和当前玩家对象 `player` 传进去。C++ 插件会负责将 `{player_name}`、`{js:server_time}`、`{js:hello:再次欢迎}` 和 `{js:actor_pos}` 等占位符替换成真实数据。
7.  **发送消息**: 最后，通过 `player.tell()` 将处理完成的消息发送给玩家。
8.  **插件卸载**: 在插件卸载时，通过 `PA.unregisterByCallbackNamespace(JS_CB_NS)` 批量卸载由本 JS 命名空间注册的所有占位符，避免资源泄露。

## 自定义

您可以轻松地修改 `ExamplePlugin.js` 文件以满足您的需求。

### 修改欢迎消息

要修改欢迎消息，只需编辑 `mc.listen` 回调函数中 `msg` 变量的内容即可：

```javascript
// ...
mc.listen("onJoin", (player) => {
    // 修改为您想要的任何文本和占位符
    const msg = "你好, {player_name}！欢迎来到服务器！你的坐标是 {js:actor_pos}。";

    const processedMessage = PA.replaceForPlayer(msg, player);
    player.tell(processedMessage);
    // ...
});
// ...
```

### 注册新的自定义占位符

您可以按照以下步骤注册新的自定义占位符：

1.  **定义回调函数**: 在 `JS_CB_NS` 命名空间下，使用 `ll.export` 定义一个新的 JavaScript 回调函数。确保其签名与您希望支持的上下文类型（服务器、玩家、实体等）相匹配。

    例如，一个简单的服务器级占位符回调：
    ```javascript
    ll.export((token, param) => {
        return `这是一个新的服务器级占位符！参数：${param}`;
    }, JS_CB_NS, "myNewServerPlaceholder");
    ```

2.  **注册占位符**: 使用 `PA` 对象中相应的 `register*Placeholder` 函数注册您的占位符。

    例如，注册上述服务器级占位符：
    ```javascript
    const okNew = PA.registerServerPlaceholder("js", "my_new_placeholder", JS_CB_NS, "myNewServerPlaceholder");
    if (okNew) {
        logger.info("已注册新的 JS 占位符：{js:my_new_placeholder}");
    }
    ```

**请注意**: 您可以使用的内置占位符取决于 `PlaceholderAPI` C++ 插件中注册了哪些。请参考其文档以获取所有可用的内置占位符列表。对于自定义 JS 占位符，您需要确保 `ll.export` 的回调函数签名与 `PA.register*Placeholder` 函数所期望的上下文类型相匹配。
