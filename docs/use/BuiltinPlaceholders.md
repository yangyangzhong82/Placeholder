# 内置占位符

PlaceholderAPI 自带了一系列名为 `pa` 的占位符，涵盖了服务器信息、时间、数学计算等常用功能。这些占位符可以直接在任何支持 PlaceholderAPI 的地方使用。

## 占位符列表

### 服务器相关

这些占位符提供关于服务器的通用信息，无需特定上下文（如玩家）。

-   `{pa:online_players}`
    -   **说明**: 显示当前在线的玩家数量。
    -   **返回**: `整数`

-   `{pa:max_players}`
    -   **说明**: 显示服务器的最大玩家数量。
    -   **返回**: `整数`

-   `{pa:uptime_seconds}`
    -   **说明**: 显示服务器自启动以来经过的秒数。
    -   **返回**: `整数`

### 玩家与实体相关

这些占位符需要一个上下文对象（通常是 `Player` 或其他 `Mob` 实体）才能正确解析。

-   `{pa:player_name}`
    -   **上下文**: `Player`
    -   **说明**: 显示玩家的真实名称（游戏名）。
    -   **返回**: `字符串`

-   `{pa:ping}`
    -   **上下文**: `Player`
    -   **说明**: 显示玩家的网络延迟（Ping 值），单位为毫秒。
    -   **返回**: `整数`

-   `{pa:health}`
    -   **上下文**: `Mob` (包括 `Player`)
    -   **说明**: 显示实体（玩家、生物等）的当前生命值。
    -   **返回**: `浮点数`

-   `{pa:max_health}`
    -   **上下文**: `Mob` (包括 `Player`)
    -   **说明**: 显示实体的最大生命值。
    -   **返回**: `整数`

-   `{pa:can_fly}`
    -   **上下文**: `Mob` (包括 `Player`)
    -   **说明**: 判断实体当前是否可以飞行。
    -   **返回**: `布尔值` (`true` 或 `false`)

### 时间相关

-   `{pa:time}`
    -   **说明**: 显示格式化的当前时间。这是一个功能强大的占位符，支持多种参数。
    -   **返回**: `字符串`
    -   **参数**:
        -   `format`: 定义输出时间的格式。遵循 C++ `chrono` 库的[格式化规则](https://en.cppreference.com/w/cpp/chrono/format)。
            -   示例: `{pa:time|format=%Y年%m月%d日 %H:%M}`
        -   `tz`: 指定时区。
            -   `UTC`: 显示 UTC 时间。
            -   `UTC+8`, `UTC-5:30`: 显示指定偏移的 UTC 时间。
            -   `Asia/Shanghai`: 显示指定命名时区的时间（需要系统支持）。
            -   示例: `{pa:time|format=%H:%M;tz=UTC+8}`

-   `{pa:year}`, `{pa:month}`, `{pa:day}`, `{pa:hour}`, `{pa:minute}`, `{pa:second}`
    -   **说明**: 分别获取当前时间的年、月、日、时、分、秒。是 `{pa:time}` 的简化版本。
    -   **返回**: `整数`

-   `{pa:unix_timestamp}`
    -   **说明**: 获取当前时间的 Unix 时间戳（秒）。
    -   **返回**: `整数`

-   `{pa:unix_timestamp_ms}`
    -   **说明**: 获取当前时间的 Unix 时间戳（毫秒）。
    -   **返回**: `整数`

### 数学与逻辑

-   `{pa:random}`
    -   **说明**: 生成一个指定范围内的随机数。
    -   **返回**: `浮点数`
    -   **参数**:
        -   `min`: 随机数的最小值（默认为 `0.0`）。
        -   `max`: 随机数的最大值（默认为 `1.0`）。
    -   **示例**: `{pa:random|min=1;max=100;decimals=0}` (生成 1-100 的随机整数)

-   `{pa:calc}`
    -   **说明**: 计算一个数学表达式。表达式中可以嵌套其他占位符。
    -   **返回**: `浮点数`
    -   **上下文**: 服务器版和实体版。如果需要根据玩家属性计算，请在玩家上下文中使用。
    -   **参数**:
        -   `expr`: 要计算的数学表达式。
    -   **示例**:
        -   `{pa:calc|expr=100 / {pa:max_players}}` (计算每个玩家的百分比)
        -   `{pa:calc|expr=({pa:health} / {pa:max_health}) * 100;decimals=2}` (计算玩家生命值百分比)

---

## 通用格式化参数

所有占位符都支持一套强大的格式化参数，通过 `|` 分隔符附加在占位符名称后面，多个参数用 `;` 分隔。

### 1. 数字格式化

这些参数主要用于处理返回数字的占位符。

-   `decimals=<n>`: 设置小数点后的位数。
    -   示例: `{pa:health|decimals=1}` -> `18.5`

-   `round=<true|false>`: 是否对结果进行四舍五入（默认为 `true`）。

-   `commas=<true|false>`: 是否添加千位分隔符。
    -   示例: `{pa:unix_timestamp_ms|commas=true}` -> `1,678,886,400,123`

-   `si=<true|false>`: 是否使用 SI 单位（K, M, G...）来缩放数字。
    -   `base=<1000|1024>`: 设置 SI 缩放的基数，默认为 1000，对于字节等单位可设为 1024。
    -   `unit=<str>`: 在 SI 后缀后附加一个单位，如 "B" 表示字节。
    -   `space=<true|false>`: 是否在数字和单位之间添加空格（默认为 `true`）。
    -   示例: `{pa:online_players|si=true}` -> `1.2 K`

-   `math="<expr>"`: 对占位符的原始数值结果进行数学运算。表达式中的 `_` 代表原始值。
    -   示例: `{pa:health|math="_ * 2"}` (将当前生命值翻倍)

-   `thresholds="<spec>"`: 根据数值范围应用不同的样式或文本。
    -   **格式**: `条件1:值1, 条件2:值2, ... , default:默认值`
    -   **条件**: 可以是 `>10`, `<=5`, `10-20` (闭区间), `[10,20)` (半开区间) 等。
    -   **值**: 可以是文本或颜色代码 (`§c`, `red`, `bold` 等)。
    -   `replace=<true|false>`: 如果为 `true`，则用阈值指定的值替换原始数字，否则仅应用样式。
    -   示例: `{pa:ping|thresholds=">200:§c, >100:§e, default:§a"}` (根据延迟高低显示不同颜色)

### 2. 字符串格式化

-   `case=<lower|upper|title>`: 转换字符串的大小写。
    -   `lower`: 转为小写。
    -   `upper`: 转为大写。
    -   `title`: 转为标题格式（每个单词首字母大写）。
    -   示例: `{pa:player_name|case=upper}` -> `PLAYERNAME`

-   `prefix=<str>`: 在结果前添加前缀。
-   `suffix=<str>`: 在结果后附加后缀。
    -   示例: `{pa:online_players|prefix=[在线: ;suffix=人]}` -> `[在线: 20人]`

-   `maxlen=<n>`: 限制字符串的最大可见长度，超出部分会被截断。
    -   `ellipsis=<str>`: 设置截断后显示的省略号（默认为 `...`）。
    -   示例: `{pa:player_name|maxlen=5}`

-   `width=<n>`: 将字符串填充到指定宽度。
    -   `align=<left|right|center>`: 对齐方式（默认为 `left`）。
    -   `fill=<char>`: 用于填充的字符（默认为空格）。
    -   示例: `{pa:player_name|width=20;align=center;fill=-}`

-   `color=<spec>`: 为结果应用颜色或样式。
    -   **格式**: 可以是单个颜色名 (`red`)、样式 (`bold`)、代码 (`§c`)，或用 `+` 连接多个 (`red+bold`)。
    -   示例: `{pa:server_name|color=gold+bold}`

-   `stripcodes=<true|false>`: 移除结果中的所有颜色/样式代码。

### 3. 逻辑与映射

-   `cond="<condition>"`: 条件判断，仅当占位符结果满足数值条件时，才执行 `then` 或 `else`。
    -   `then=<str>`: 条件为真时的输出。
    -   `else=<str>`: 条件为假时的输出。
    -   示例: `{pa:online_players|cond=">10";then=服务器很热闹;else=服务器有点冷清}`

-   `equals="<str>"`: 判断占位符结果是否与指定字符串相等。
    -   `ci=<true|false>`: 是否忽略大小写比较（默认为 `false`）。
    -   `then=<str>` / `else=<str>`: 同上。
    -   示例: `{pa:can_fly|equals=true;then=飞行中;else=行走中}`

-   `map="<spec>"`: 将占位符的特定结果映射为其他值。
    -   **格式**: `键1:值1, 键2:值2, ... , default:默认值`
    -   `mapci`: 功能同 `map`，但键的匹配忽略大小写。
    -   `mapre`: 功能同 `map`，但键是正则表达式。
    -   示例: `{pa:can_fly|map="true:§a是, false:§c否"}`

-   `emptytext=<str>`: 当占位符的最终结果为空字符串时，显示此文本。

-   `repl="<from1>-><to1>,<from2>-><to2>"`: 对结果进行简单的字符串替换。
