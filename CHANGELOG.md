# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).
## [0.4.0]

### Added
- 为 `entity_look_block` 占位符添加参数支持 (`maxDistance`, `includeLiquid`, `solidOnly`, `fullOnly`)。
- 将 `player_look` 占位符重命名为 `actor_look`，并为其添加 `maxDistance` 参数支持，使其适用于所有 `Actor`。

### Changed
- 改进了占位符解析逻辑，使其能够正确处理嵌套占位符和转义字符。

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
