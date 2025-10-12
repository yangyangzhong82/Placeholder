# 内置占位符文档

本文档详细列出了 Placeholder API 提供的所有内置占位符及其用法。

**注册函数：** `void PA::registerBuiltinPlaceholders(IPlaceholderService* svc)`

### 玩家上下文 (`PlayerContext`)

| 占位符           | 描述             | 示例输出 |
| :--------------- | :--------------- | :------- |
| `{player_name}`  | 玩家的真实名称   | `Steve`  |
| `{ping}`         | 玩家的平均网络延迟 | `50`     |

### 生物上下文 (`MobContext`)

| 占位符         | 描述           | 示例输出 |
| :------------- | :------------- | :------- |
| `{can_fly}`    | 生物是否能飞行 | `true`   |
| `{health}`     | 生物的生命值   | `20`     |

### Actor 上下文 (`ActorContext`)

| 占位符             | 描述               | 示例输出         |
| :----------------- | :----------------- | :--------------- |
| `{actor_is_on_ground}` | Actor 是否在地面上     | `true`           |
| `{actor_is_alive}` | Actor 是否存活         | `true`           |
| `{actor_is_invisible}` | Actor 是否隐形         | `false`          |
| `{actor_type_id}`  | Actor 的实体类型 ID    | `1`              |
| `{actor_type_name}` | Actor 的实体类型名称   | `minecraft:player` |
| `{actor_pos}`      | Actor 的位置 (Vec3.toString()) | `(100.5, 64.0, 200.5)` |
| `{actor_pos_x}`    | Actor 的 X 坐标        | `100.5`          |
| `{actor_pos_y}`    | Actor 的 Y 坐标        | `64.0`           |
| `{actor_pos_z}`    | Actor 的 Z 坐标        | `200.5`          |
| `{actor_unique_id}` | Actor 的唯一 ID        | `123456789`      |
| `{actor_is_baby}`  | Actor 是否是幼年生物   | `false`          |
| `{actor_is_riding}` | Actor 是否正在骑乘     | `false`          |
| `{actor_is_tame}`  | Actor 是否被驯服       | `false`          |

### 服务器上下文 (`kServerContextId`)

| 占位符           | 描述             | 示例输出           |
| :--------------- | :--------------- | :----------------- |
| `{online_players}` | 当前在线玩家数量 | `10`               |
| `{max_players}`  | 服务器最大玩家数量 | `20`               |
| `{time}`         | 当前服务器时间   | `2025-10-11 22:30:00` |
| `{year}`         | 当前年份         | `2025`             |
| `{month}`        | 当前月份         | `10`               |
| `{day}`          | 当前日期         | `11`               |
| `{hour}`         | 当前小时         | `22`               |
| `{minute}`       | `当前分钟`       | `30`               |
| `{second}`       | 当前秒           | `00`               |
