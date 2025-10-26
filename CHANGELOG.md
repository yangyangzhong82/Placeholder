# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.4.3] 2025-10-26
### Fixed
- 修复了 `PlaceholderRegistry::findPlaceholder` 在并发场景下可能返回悬垂指针的问题。通过引入一个持有快照生命周期的 `LookupResult` 结构体，确保了返回的 `CachedEntry*` 指针在被使用期间始终有效。

## [0.4.2] 2025-10-26



### Changed
- 适配LL1.6.1 并删除部分默认占位符以做适配。

## [0.4.1] 2025-10-25

### Fixed
- 修复了 `AdapterAliasPlaceholder::evaluateWithArgs` 中无参数上下文别名（如 `player_inventory`）的参数分割逻辑错误，导致嵌套占位符解析失败的问题。
- 修复了 `player_inventory` 别名解析器中返回局部指针的 use-after-return 错误。
- 修复了 `container_slot` 别名解析器中对 `ItemStack` API 的误用，并增加了空槽位检查以避免崩溃。

### Changed
- 修正了 `SystemPlaceholders.cpp` 中内存计算的浮点数除法，确保精确度。
- 使用 `magic_enum` 库来简化代码。
### Added
- **增强的上下文扩展性**: 引入“上下文工厂”机制 (`registerContextFactory`)，允许插件注册函数来动态构造临时的、自定义的目标上下文实例。这使得插件可以无缝地将其数据模型集成到占位符的上下文别名系统中。
- 新增 `BlockActor` 上下文及其相关占位符（`{block_actor_pos}`, `{block_actor_pos_x}`, `{block_actor_pos_y}`, `{block_actor_pos_z}`, `{block_actor_type_name}`, `{block_actor_custom_name}`, `{block_actor_is_movable}`, `{block_actor_repair_cost}`, `{block_actor_has_container}`）。
- 新增 `player_look_block_actor` 别名占位符，用于获取玩家视线所指向的方块实体信息。
- **物品堆上下文 (`ItemStackBaseContext`) 占位符增强:**
    - 新增 `{item_lore}`: 物品的 Lore。
    - 新增 `{item_custom_name}`: 物品的自定义名称。
    - 新增 `{item_id}`: 物品的数字 ID。
    - 新增 `{item_raw_name_id}`: 物品的原始名称 ID。
    - 新增 `{item_description_id}`: 物品的描述 ID。
    - 新增 `{item_is_block}`: 物品是否是方块。
    - 新增 `{item_is_armor}`: 物品是否是盔甲。
    - 新增 `{item_is_potion}`: 物品是否是药水。
    - 新增 `{item_block:<inner_placeholder_spec>}`: 物品的方块信息。
- **服务器上下文 (`kServerContextId`) 占位符增强:**
    - 新增 `{level_seed}`: 世界种子。
    - 新增 `{level_name}`: 世界名称。
    - 新增 `{language}`: 服务器语言。
    - 新增 `{server_name}`: 服务器名称。
    - 新增 `{server_port}`: 服务器端口。
    - 新增 `{server_portv6}`: 服务器 IPv6 端口。
- **系统上下文 (`SystemContext`) 占位符增强:**
    - 新增 `{system_free_memory}`: 系统空闲内存 (MB)。
    - 新增 `{system_memory_percent}`: 系统内存使用百分比 (%)。
    - 新增 `{server_memory_percent}`: 服务器内存使用百分比 (%)。
    - 新增 `{system_uptime}`: 系统运行时间。
    - 新增 `{server_uptime}`: 服务器运行时间。
- **时间上下文 (`TimeContext`) 占位符增强:**
    - 新增 `{time_diff:<unix_timestamp>,<unit>}`: 计算从指定 Unix 时间戳到现在已经过去了多少时间。

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
