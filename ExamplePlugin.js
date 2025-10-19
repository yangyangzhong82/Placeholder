// 插件名称：PlaceholderAPI JS 示例（含注册自定义占位符）
// 插件版本：1.1.0
// 插件描述：演示如何在 JS 中注册自定义占位符，并与 PlaceholderAPI 配合使用

// 1) 导入 PlaceholderAPI 暴露的函数
const PA = {
  replaceForPlayer: ll.import('PA', 'replaceForPlayer'),
  replace: ll.import('PA', 'replace'),

  registerPlayerPlaceholder: ll.import('PA', 'registerPlayerPlaceholder'),
  // registerMobPlaceholder: ll.import("PA", "registerMobPlaceholder"),
  // lse没有Mob这个对象，不要用
  registerActorPlaceholder: ll.import('PA', 'registerActorPlaceholder'),
  registerServerPlaceholder: ll.import('PA', 'registerServerPlaceholder'),
  registerPlaceholderByKind: ll.import('PA', 'registerPlaceholderByKind'),
  registerPlaceholderByContextId:
      ll.import('PA', 'registerPlaceholderByContextId'),

  unregisterByCallbackNamespace:
      ll.import('PA', 'unregisterByCallbackNamespace'),
  contextTypeIds: ll.import('PA', 'contextTypeIds'),
};
// 更新玩家侧边栏信息
function update(player) {
  // 获取玩家名称
  const playerName = PA.replaceForPlayer('{player_name}', player);
  logger.info(`[JS Debug] 玩家名称: ${playerName}`);
  const b = PA.replaceForPlayer('{entity_look_block:block_type_name}', player);
  logger.info(`[JS Debug] 方块名称: ${b}`);
  const money = PA.replaceForPlayer('{llmoney}', player);
  logger.info(`[JS Debug] llmoney: ${money}`);
  const moneya = PA.replace('{total_entities}');
  logger.info(`[JS Debug] 实体数 ${moneya}`);
  const moneyab = PA.replace('{total_entities:exclude_drops}');
  logger.info(`[JS Debug] 无掉落物实体数 ${moneyab}`);
  const pos = PA.replaceForPlayer('{js:actor_pos}', player);
  logger.info(`[JS Debug] 实体坐标 ${pos}`);
}

// 定时更新所有在线玩家的侧边栏
setInterval(() => {
  const players = mc.getOnlinePlayers();
  players.forEach(player => {
    update(player);
  });
}, 1000);
// 2) 在 JS 中导出一个回调函数，供 C++ 在占位符求值时调用
// 回调命名空间
const JS_CB_NS = 'JSPH';

// 玩家上下文占位符回调签名：std::string(std::string token, std::string param,
// Player* player)
ll.export((token, param, player) => {
  // token 是注册时传入的 tokenName（不带花括号），例如 "hello"
  // param 是占位符参数（如果用户写了 {js:hello:xxx}，param 为
  // "xxx"，否则为空字符串）
  const extra = param ? `（${param}）` : '';
  const name = player ? player.name : '未知玩家';
  return `你好，${name}${extra}`;
}, JS_CB_NS, 'helloPlayer');

// 服务器级占位符回调签名：std::string(std::string token, std::string param)
ll.export((token, param) => {
  const now = new Date();
  return `服务器时间：${now.toLocaleString()}`;
}, JS_CB_NS, 'serverTime');

// Actor 上下文占位符回调签名：std::string(std::string token, std::string param,
// Actor* actor)
ll.export((token, param, actor) => {
  if (!actor) return '无实体';
  const pos = actor.pos;
  return `实体坐标(${pos.x.toFixed(1)}, ${pos.y.toFixed(1)}, ${
      pos.z.toFixed(1)})`;
}, JS_CB_NS, 'actorPos');

// 3) 向 PlaceholderAPI 注册这些占位符
// 最终占位符形如：{js:hello}、{js:server_time}、{js:actor_pos}
const ok1 =
    PA.registerPlayerPlaceholder('js', 'hello', JS_CB_NS, 'helloPlayer', 0);
const ok2 =
    PA.registerServerPlaceholder('js', 'server_time', JS_CB_NS, 'serverTime', 0);
const ok3 =
    PA.registerActorPlaceholder('js', 'actor_pos', JS_CB_NS, 'actorPos', 5); // 恢复缓存时间为 5 秒

// 注册一个缓存的服务器级占位符，缓存时间为 5 秒
ll.export((token, param) => {
  const now = new Date();
  return `缓存服务器时间：${now.toLocaleString()}`;
}, JS_CB_NS, 'cachedServerTime');
// 现在 registerServerPlaceholder 会根据传入的 cacheDuration 自动处理缓存
const ok4 = PA.registerServerPlaceholder(
    'js', 'cached_server_time', JS_CB_NS, 'cachedServerTime', 5);

logger.info(`[JS Debug] ok1: ${ok1}, ok2: ${ok2}, ok3: ${ok3}, ok4: ${ok4}`);

if (!ok1 || !ok2 || !ok3 || !ok4) {
  logger.error('注册 JS 占位符失败，请检查前面的日志。');
} else {
  logger.info(
      '已注册 JS 占位符：{js:hello} / {js:server_time} / {js:actor_pos} / {js:cached_server_time} (缓存)');
}

mc.listen('onJoin', (player) => {
  const msg =
      '欢迎, {player_name}! 现在时间：{js:server_time}，自定义问候：{js:hello:再次欢迎} {js:actor_pos}，缓存时间：{js:cached_server_time}';
  const processedMessage = PA.replaceForPlayer(msg, player);
  player.tell(processedMessage);
  logger.info(`向玩家 ${player.name} 发送了欢迎消息: ${processedMessage}`);
  update(player);
});


// 5) 插件卸载时，清理由本 JS 命名空间注册的占位符
ll.registerPluginUnload && ll.registerPluginUnload(() => {
  const ok = PA.unregisterByCallbackNamespace(JS_CB_NS);
  logger.info(`已卸载 '${JS_CB_NS}' 名下的占位符：${ok}`);
});

logger.info(
    'PlaceholderAPI JS 示例插件已加载，正在监听 onJoin 事件并注册 JS 占位符。');
