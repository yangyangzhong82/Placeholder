# 内置占位符文档

本文档详细列出了 Placeholder API 提供的所有内置占位符及其用法。

**注册函数：** `void PA::registerBuiltinPlaceholders(IPlaceholderService* svc)`

### 玩家上下文 (`PlayerContext`)

| 占位符                     | 描述                               | 示例输出         |
| :------------------------- | :--------------------------------- | :--------------- |
| `{player_realname}`            | 玩家的真实名称                     | `Steve`          |
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
| `{player_hand:<inner_placeholder_spec>}` | 玩家手持的物品。可用于获取手持物品的属性，例如 `{player_hand:item_name}` | `minecraft:diamond_sword` |
| `{player_inventory:<inner_placeholder_spec>}` | 玩家背包容器。可用于获取背包容器的属性，例如 `{player_inventory:container_size}` | `36` |
| `{player_enderchest:<inner_placeholder_spec>}` | 玩家末影箱容器。可用于获取末影箱容器的属性，例如 `{player_enderchest:container_size}` | `27` |
| `{player_look_block_actor:<params>:<inner_placeholder_spec>}` | 玩家视线所指向的方块实体。可用于获取方块实体的属性。支持参数：`maxDistance` (浮点数), `includeLiquid` (布尔值), `solidOnly` (布尔值), `fullOnly` (布尔值)。例如 `{player_look_block_actor:maxDistance=10,includeLiquid=true:block_actor_type_name}` | `minecraft:chest` |
| `{player_world_coordinate:<inner_placeholder_spec>}` | 玩家的世界坐标。可用于获取玩家的坐标和维度信息，例如 `{player_world_coordinate:pos_x}` | `(100.5, 64.0, 200.5)` |
| `{player_offhand_item:<inner_placeholder_spec>}` | 玩家副手持有的物品。可用于获取副手物品的属性，例如 `{player_offhand_item:item_name}` | `minecraft:shield` |
| `{player_armor_container:<inner_placeholder_spec>}` | 玩家的盔甲容器。可用于获取盔甲容器的属性，例如 `{player_armor_container:container_size}` | `4` |
| `{player_level}` | 玩家的经验等级 | `30` |

### 容器上下文 (`ContainerContext`)

| 占位符                                   | 描述                                       | 示例输出                     |
| :--------------------------------------- | :----------------------------------------- | :--------------------------- |
| `{container_size}`                       | 容器的总槽位数量                           | `36`                         |
| `{container_empty_slots}`                | 容器的空槽数量                             | `10`                         |
| `{container_type_name}`                  | 容器的类型名称                             | `inventory`                  |
| `{container_has_custom_name}`            | 容器是否有自定义名称                       | `true`                       |
| `{container_custom_name}`                | 容器的自定义名称                           | `我的箱子`                   |
| `{container_is_empty}`                   | 容器是否为空                               | `false`                      |
| `{container_item_count}`                 | 容器内所有物品的总数量                     | `128`                        |
| `{container_remaining_capacity}`         | 容器剩余的空位数量                         | `10`                         |
| `{container_item_count_type:<type1>,<type2>}` | 容器内指定类型物品的总数量。支持多个物品类型 | `64`                         |
| `{container_slot:<index>:<inner_placeholder_spec>}` | 容器内指定槽位的物品。可用于获取物品属性，例如 `{container_slot:0:item_name}` | `minecraft:diamond`          |

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
| `{actor_look:<params>:<inner_placeholder_spec>}` | 实体正在看的实体。可用于获取所看实体的属性。支持参数：`maxDistance` (浮点数)。例如 `{actor_look:maxDistance=10:type_name}` | `minecraft:cow` |
| `{actor_runtimeid}`    | Actor 的运行时 ID                          | `123`                        |
| `{actor_effects}`      | Actor 的药水效果。无参数时列出所有效果名称；带一个参数时返回特定效果的详细信息；带两个参数时返回特定效果的指定属性 (level, duration, id, display_name)。 | `速度 (等级: 1, 持续时间: 30秒)` |
| `{actor_max_health}`   | Actor 的最大生命值                         | `20`                         |
| `{score:objective_name}`   | Actor 在指定记分板上的分数           | `123`            |
| `{entity_look_block:<params>:<inner_placeholder_spec>}` | 实体正在看的方块。可用于获取所看方块的属性。支持参数：`maxDistance` (浮点数), `includeLiquid` (布尔值), `solidOnly` (布尔值), `fullOnly` (布尔值)。例如 `{entity_look_block:maxDistance=10,includeLiquid=true:block_type_name}` | `minecraft:stone` |

### 世界坐标上下文 (`WorldCoordinateContext`)

| 占位符                     | 描述                               | 示例输出         |
| :------------------------- | :--------------------------------- | :--------------- |
| `{world_coordinate_pos}`   | 世界坐标 (Vec3.toString())         | `(100.5, 64.0, 200.5)` |
| `{world_coordinate_pos_x}` | 世界坐标的 X 坐标                  | `100.5`          |
| `{world_coordinate_pos_y}` | 世界坐标的 Y 坐标                  | `64.0`           |
| `{world_coordinate_pos_z}` | 世界坐标的 Z 坐标                  | `200.5`          |
| `{world_coordinate_dimension_id}` | 世界坐标的维度 ID             | `0` (主世界)     |

### 方块实体上下文 (`BlockActorContext`)

| 占位符                 | 描述                                       | 示例输出                     |
| :--------------------- | :----------------------------------------- | :--------------------------- |
| `{block_actor_pos}`    | 方块实体的位置 (BlockPos.toString())       | `(100, 64, 200)`             |
| `{block_actor_pos_x}`  | 方块实体的 X 坐标                          | `100`                        |
| `{block_actor_pos_y}`  | 方块实体的 Y 坐标                          | `64`                         |
| `{block_actor_pos_z}`  | 方块实体的 Z 坐标                          | `200`                        |
| `{block_actor_type_name}` | 方块实体的类型名称                       | `ChestBlockActor`            |
| `{block_actor_custom_name}` | 方块实体的自定义名称                     | `我的箱子`                   |
| `{block_actor_is_movable}` | 方块实体是否可移动                       | `true`                       |
| `{block_actor_repair_cost}` | 方块实体的修复成本                       | `0`                          |
| `{block_actor_has_container}` | 方块实体是否包含容器                     | `true`                       |

### 方块上下文 (`BlockContext`)

| 占位符                 | 描述                                       | 示例输出                     |
| :--------------------- | :----------------------------------------- | :--------------------------- |
| `{block_type_name}`    | 方块的类型名称                             | `minecraft:stone`            |
| `{block_data}`         | 方块的数据值                               | `0`                          |
| `{block_is_solid}`     | 方块是否是固体                             | `true`                       |
| `{block_is_air}`       | 方块是否是空气方块                         | `false`                      |
| `{block_description_id}` | 方块的描述 ID                              | `tile.stone`                 |

### 物品堆上下文 (`ItemStackBaseContext`)

| 占位符                 | 描述                                       | 示例输出                     |
| :--------------------- | :----------------------------------------- | :--------------------------- |
| `{item_name}`          | 物品的描述名称                             | `minecraft:stone`            |
| `{item_count}`         | 物品堆中的物品数量                         | `64`                         |
| `{item_aux_value}`     | 物品的辅助值 (数据值)                      | `0`                          |
| `{item_max_stack_size}`| 物品的最大堆叠数量                         | `64`                         |
| `{item_is_null}`       | 物品是否为空                               | `false`                      |
| `{item_is_enchanted}`  | 物品是否附魔                               | `true`                       |
| `{item_is_damaged}`    | 物品是否损坏                               | `false`                      |
| `{item_damage_value}`  | 物品的当前损坏值                           | `0`                          |
| `{item_max_damage}`    | 物品的最大损坏值                           | `100`                        |
| `{item_lore}`          | 物品的 Lore (多行文本，每行以换行符分隔) | `第一行 Lore\n第二行 Lore` |
| `{item_custom_name}`   | 物品的自定义名称                         | `我的钻石剑`                 |
| `{item_id}`            | 物品的数字 ID                            | `276`                        |
| `{item_raw_name_id}`   | 物品的原始名称 ID                        | `minecraft:diamond_sword`    |
| `{item_description_id}`| 物品的描述 ID                            | `item.diamondSword`          |
| `{item_is_block}`      | 物品是否是方块                           | `true`                       |
| `{item_is_armor}`      | 物品是否是盔甲                           | `false`                      |
| `{item_is_potion}`     | 物品是否是药水                           | `false`                      |
| `{item_block:<inner_placeholder_spec>}` | 物品的方块信息。可用于获取物品作为方块时的属性，例如 `{item_block:block_type_name}` | `minecraft:stone` |

### 服务器上下文 (`kServerContextId`)

| 占位符                    | 描述                 | 示例输出           |
| :------------------------ | :------------------- | :----------------- |
| `{online_players}`        | 当前在线玩家数量     | `10`               |
| `{max_players}`           | 服务器最大玩家数量   | `20`               |
| `{total_entities}`        | 服务器中的实体总数。可选参数 `exclude_drops` 可排除掉落物，`exclude_players` 可排除玩家 | `150`              |
| `{server_version}`        | 服务器版本           | `1.20.50.02`       |
| `{server_protocol_version}` | 服务器协议版本       | `618`              |
| `{loader_version}`        | 加载器版本           | `1.4.0`            |
| `{level_seed}`            | 世界种子             | `12345`            |
| `{level_name}`            | 世界名称             | `My World`         |
| `{language}`              | 服务器语言           | `zh_CN`            |
| `{server_name}`           | 服务器名称           | `My Minecraft Server` |
| `{server_port}`           | 服务器端口           | `19132`            |
| `{server_portv6}`         | 服务器 IPv6 端口     | `19133`            |
| `{server_mod_count}`      | 服务器加载的模组总数 | `5`                |

### 系统上下文 (`SystemContext`)

| 占位符                    | 描述                   | 示例输出     |
| :------------------------ | :--------------------- | :----------- |
| `{server_memory_usage}`   | 服务器进程的内存使用量 (MB) | `512.34`     |
| `{server_cpu_usage}`      | 服务器进程的 CPU 使用率 (%) | `15.78`      |
| `{system_total_memory}`   | 系统总内存 (MB)        | `16384.00`   |
| `{system_used_memory}`    | 系统已用内存 (MB)      | `8192.50`    |
| `{system_free_memory}`    | 系统空闲内存 (MB)      | `8200.00`    |
| `{system_memory_percent}` | 系统内存使用百分比 (%) | `50.00`      |
| `{server_memory_percent}` | 服务器内存使用百分比 (%) | `3.13`       |
| `{system_cpu_usage}`      | 系统总 CPU 使用率 (%)  | `30.25`      |
| `{system_uptime}`         | 系统运行时间           | `1d 05:30:15` |
| `{server_uptime}`         | 服务器运行时间           | `00:10:30`   |

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
| `{time_diff:<unix_timestamp>,<unit>}` | 计算从指定 Unix 时间戳到现在已经过去了多少时间。`<unit>` 可以是 `minutes` (默认)、`hours`、`days` 或 `seconds`。 | `120` (分钟)       |
