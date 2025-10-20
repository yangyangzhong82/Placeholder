# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.4.1]

### Fixed
- 修复了 `AdapterAliasPlaceholder::evaluateWithArgs` 中无参数上下文别名（如 `player_inventory`）的参数分割逻辑错误，导致嵌套占位符解析失败的问题。
- 修复了 `player_inventory` 别名解析器中返回局部指针的 use-after-return 错误。
- 修复了 `container_slot` 别名解析器中对 `ItemStack` API 的误用，并增加了空槽位检查以避免崩溃。

## [0.4.0]

### Added
- 新增 `ItemStackBase` 上下文及其相关占位符（`{item_name}`, `{item_count}`, `{item_aux_value}`, `{item_max_stack_size}`, `{item_is_null}`, `{item_is_enchanted}`, `{item_is_damaged}`, `{item_damage_value}`, `{item_max_damage}`）。
- 新增 `player_hand` 别名占位符，用于获取玩家手持物品的信息。
- 新增 `PA.replace` 函数，用于替换不依赖特定上下文的占位符。
- 为 `entity_look_block` 占位符添加参数支持 (`maxDistance`, `includeLiquid`, `solidOnly`, `fullOnly`)。
- 将 `player_look` 占位符重命名为 `actor_look`，并为其添加 `maxDistance` 参数支持，使其适用于所有 `Actor`。
- 新增对带缓存的关系型占位符的支持。
- 引入 `IScopedPlaceholderRegistrar` 接口和 `createScopedRegistrar` 方法，提供 RAII 作用域注册器，简化占位符的注册和自动注销。

### Changed
- 统一 JavaScript 占位符注册 API：所有注册函数（`registerPlayerPlaceholder`, `registerActorPlaceholder`, `registerServerPlaceholder`, `registerPlaceholderByKind`, `registerPlaceholderByContextId`）现在都支持可选的 `cacheDuration` 参数，用于控制占位符的缓存行为。
- 更新 `ExamplePlugin.js` 示例代码，以演示新的 `replace` 函数和统一的注册 API。
- 改进了占位符解析逻辑，使其能够正确处理嵌套占位符和转义字符。

### Removed
- 移除独立的缓存占位符注册函数，包括 `registerCachedServerPlaceholder`, `registerCachedPlayerPlaceholder`, `registerCachedActorPlaceholder`, `registerCachedPlaceholderByKind`, `registerCachedPlaceholderByContextId`。

## [0.3.0]

### Added
- 改进参数解析器以支持复杂参数值。
- 添加占位符缓存机制以提升性能。
- 引入上下文适配器，增强占位符系统扩展性。
- 增加部分默认占位符。
- 增加方块占位符。

### Refactor
- 将默认占位符拆分。

### Fixed
- 修复 `PlaceholderProcessor::process` 方法中的参数分流逻辑问题。
- 修复派生上下文优先级反了的问题。
- 修复 `regex_map $n` 替换次序错误。
