# 内置占位符文档

本文档详细列出了 Placeholder API 提供的所有内置占位符及其用法。

**注册函数：** `void PA::registerBuiltinPlaceholders(IPlaceholderService* svc)`

### 玩家上下文 (`PlayerContext`)

| 占位符                     | 描述                               | 示例输出         |
| :------------------------- | :--------------------------------- | :--------------- |
| `{player_name}`            | 玩家的真实名称                     | `Steve`          |
| `{player_average_ping}`    | 玩家的平均网络延迟                 | `50`             |
| `{player_ping}`            | 玩家的当前网络延迟                 | `45`             |
| `{player_avgpacketloss}`   | 玩家的当前丢包率                   | `0.5`            |
| `{player_averagepacketloss}` | 玩家的平均丢包率                   | `0.3`            |
| `{player_locale_code}`     | 玩家的语言区域代码                 | `zh_CN`          |
| `{player_os}`              | 玩家的操作系统                     | `Windows`        |
| `{player_uuid}`            | 玩家的 UUID                        | `...`            |
| `{player_xuid}`            | 玩家的 XUID                        | `...`            |
| `{player_hunger}`          | 玩家的饥饿值                       | `20`             |
| `{player_max_hunger}`      | 玩家的最大饥饿值                   | `20`             |
| `{player_saturation}`      | 玩家的饱和度                       | `20`             |
| `{player_max_saturation}`  | 玩家的最大饱和度                   | `20`             |
| `{player_gametype}`        | 玩家的游戏模式                     | `Survival`       |
| `{player_ip}`              | 玩家的 IP 地址和端口               | `127.0.0.1:19132` |
| `{llmoney}`                | 玩家的 LegacyMoney 余额            | `1000`           |
| `{player_riding:<inner_placeholder_spec>}` | 玩家骑乘的实体。可用于获取骑乘实体的属性，例如 `{player_riding:type_name}` | `minecraft:horse` |
| `{player_block:<inner_placeholder_spec>}` | 玩家脚下的方块。可用于获取方块的属性，例如 `{player_block:block_type_name}` | `minecraft:air` |

### 生物上下文 (`MobContext`)

| 占位符            | 描述             | 示例输出 |
| :---------------- | :--------------- | :------- |
| `{mob_can_fly}`   | 生物是否能飞行   | `true`   |
| `{mob_health}`    | 生物的生命值     | `20`     |
| `{mob_armor_value}` | 生物的护甲值     | `5`      |

### Actor 上下文 (`ActorContext`)

| 占位符                 | 描述                                       | 示例输出                     |
| :--------------------- | :----------------------------------------- | :--------------------------- |
| `{actor_is_on_ground}` | Actor 是否在地面上                         | `true`                       |
| `{actor_is_alive}`     | Actor 是否存活                             | `true`                       |
| `{actor_is_invisible}` | Actor 是否隐形                             | `false`                      |
| `{actor_type_id}`      | Actor 的实体类型 ID                        | `1`                          |
| `{actor_type_name}`    | Actor 的实体类型名称                       | `minecraft:player`           |
| `{actor_pos}`          | Actor 的位置 (Vec3.toString())             | `(100.5, 64.0, 200.5)`       |
| `{actor_pos_x}`        | Actor 的 X 坐标                            | `100.5`                      |
| `{actor_pos_y}`        | Actor 的 Y 坐标                            | `64.0`                       |
| `{actor_pos_z}`        | Actor 的 Z 坐标                            | `200.5`                      |
| `{actor_rotation}`     | Actor 的旋转 (Vec2.toString())             | `(0.0, 90.0)`                |
| `{actor_rotation_x}`   | Actor 的 X 旋转                            | `0.0`                        |
| `{actor_rotation_y}`   | Actor 的 Y 旋转                            | `90.0`                       |
| `{actor_unique_id}`    | Actor 的唯一 ID                            | `123456789`                  |
| `{actor_is_baby}`      | Actor 是否是幼年生物                       | `false`                      |
| `{actor_is_riding}`    | Actor 是否正在骑乘                         | `false`                      |
| `{actor_is_tame}`      | Actor 是否被驯服                           | `false`                      |
| `{player_look:<inner_placeholder_spec>}` | 玩家正在看的实体。可用于获取所看实体的属性，例如 `{player_look:type_name}` | `minecraft:cow` |
| `{actor_runtimeid}`    | Actor 的运行时 ID                          | `123`                        |
| `{actor_effects}`      | Actor 的药水效果。无参数时列出所有效果名称；带一个参数时返回特定效果的详细信息；带两个参数时返回特定效果的指定属性 (level, duration, id, display_name)。 | `速度 (等级: 1, 持续时间: 30秒)` |
| `{actor_max_health}`   | Actor 的最大生命值                         | `20`                         |
| `{score:objective_name}`   | Actor 在指定记分板上的分数           | `123`            |

### 方块上下文 (`BlockContext`)

| 占位符                 | 描述                                       | 示例输出                     |
| :--------------------- | :----------------------------------------- | :--------------------------- |
| `{block_type_name}`    | 方块的类型名称                             | `minecraft:stone`            |
| `{block_data}`         | 方块的数据值                               | `0`                          |
| `{block_is_solid}`     | 方块是否是固体                             | `true`                       |
| `{block_is_air}`       | 方块是否是空气方块                         | `false`                      |
| `{block_description_id}` | 方块的描述 ID                              | `tile.stone`                 |

### 服务器上下文 (`kServerContextId`)

| 占位符                    | 描述                 | 示例输出           |
| :------------------------ | :------------------- | :----------------- |
| `{online_players}`        | 当前在线玩家数量     | `10`               |
| `{max_players}`           | 服务器最大玩家数量   | `20`               |
| `{total_entities}`        | 服务器中的实体总数。可选参数 `exclude_drops` 可排除掉落物，`exclude_players` 可排除玩家 | `150`              |
| `{server_version}`        | 服务器版本           | `1.20.50.02`       |
| `{server_protocol_version}` | 服务器协议版本       | `618`              |
| `{loader_version}`        | 加载器版本           | `1.4.0`            |

### 系统上下文 (`SystemContext`)

| 占位符                  | 描述                   | 示例输出 |
| :---------------------- | :--------------------- | :------- |
| `{server_memory_usage}` | 服务器进程的内存使用量 (MB) | `512.34` |
| `{server_cpu_usage}`    | 服务器进程的 CPU 使用率 (%) | `15.78`  |
| `{system_total_memory}` | 系统总内存 (MB)        | `16384.00` |
| `{system_used_memory}`  | 系统已用内存 (MB)      | `8192.50`  |
| `{system_cpu_usage}`    | 系统总 CPU 使用率 (%)  | `30.25`  |

### 时间上下文 (`TimeContext`)

| 占位符    | 描述                 | 示例输出           |
| :-------- | :------------------- | :----------------- |
| `{time}`  | 当前服务器时间 (YYYY-MM-DD HH:MM:SS) | `2025-10-11 22:30:00` |
| `{year}`  | 当前年份             | `2025`             |
| `{month}` | 当前月份             | `10`               |
| `{day}`   | 当前日期             | `11`               |
| `{hour}`  | 当前小时             | `22`               |
| `{minute}` | 当前分钟             | `30`               |
| `{second}` | 当前秒               | `00`               |
