# `Utils` API 参考

`PA::Utils` 命名空间提供了一系列强大的工具函数，主要用于解析占位符参数和对输出结果进行高级格式化。

## 参数解析

### ParsedParams

这是一个核心类，用于解析占位符中的参数字符串（例如 `%player_name:prefix=Mr.;color=red%` 中的 `prefix=Mr.;color=red`）。它支持键值对、类型化访问和缓存，性能很高。

#### 构造函数

```cpp
ParsedParams(
    std::string_view paramStr,
    std::string_view kvsep = "=",
    std::string_view pairsep = ";"
);
```

- **参数**:
    - `paramStr`: 要解析的原始参数字符串。
    - `kvsep`: (可选) 键和值之间的分隔符，默认为 `=`。
    - `pairsep`: (可选) 键值对之间的分隔符，默认为 `;`。

#### 获取参数值

```cpp
// 获取原始字符串值
std::optional<std::string_view> get(const std::string& key) const;

// 获取并解析为布尔值 (支持 "true", "yes", "1", "on" 等)
std::optional<bool> getBool(const std::string& key) const;

// 获取并解析为整数
std::optional<int> getInt(const std::string& key) const;

// 获取并解析为浮点数
std::optional<double> getDouble(const std::string& key) const;

// 检查是否存在某个键
bool has(const std::string& key) const;
```

## 文本格式化

### applyFormatting

这是格式化功能的主要入口点。它接收一个原始字符串值和 `ParsedParams` 对象，然后根据参数应用一系列格式化规则。

```cpp
std::string applyFormatting(
    const std::string& rawValue,
    const ParsedParams& params
);
```

### 支持的格式化参数

`applyFormatting` 函数支持通过 `ParsedParams` 传入多种参数来控制输出格式，以下是一些常用的参数：

- **条件格式化**:
    - `cond`: 数值条件 (例如 `">10"`, `"[0-100]"`).
    - `equals`: 字符串精确匹配。
    - `ci`: (布尔) 与 `equals` 配合使用，进行不区分大小写的比较。
    - `then`: 条件满足时的输出文本。
    - `else`: 条件不满足时的输出文本。

- **数字格式化**:
    - `math`: 应用一个数学表达式 (例如 `"_*2+10"`，`_` 代表原始数值)。
    - `decimals`: 小数位数。
    - `round`: (布尔) 是否四舍五入。
    - `commas`: (布尔) 是否添加千分位分隔符。
    - `si`: (布尔) 是否使用 SI 单位缩写 (K, M, G)。
    - `unit`: 与 `si` 配合使用，指定单位 (例如 "B" 表示字节)。
    - `thresholds`: 基于数值阈值应用不同样式或文本 (例如 `">10:red,<=10:green"`).

- **字符串操作**:
    - `prefix`/`suffix`: 添加前缀/后缀。
    - `color`/`style`: 应用颜色和样式 (例如 `"red+bold"`).
    - `case`: 大小写转换 (`upper`, `lower`, `title`).
    - `repl`: 字符串替换 (例如 `"foo->bar,baz->qux"`).
    - `map`/`mapci`/`mapre`: 将输入值映射到另一个值。
    - `maxlen`: 截断字符串到指定可见长度。
    - `ellipsis`: 截断时使用的省略号。
    - `align`/`width`/`fill`: 对齐、填充和设置宽度。

- **其他**:
    - `emptytext`: 当原始值为空时显示的文本。
    - `json`: 将字符串转义为 JSON 兼容格式。
    - `stripcodes`: 移除所有颜色/样式代码。
    - `lua`: 使用 Lua 脚本进行高级处理。

### 示例

假设有一个占位符 `%server_tps%` 返回 `19.8765`。

- `params`: `decimals=1;commas=true`
  - **输出**: `"19.9"`
- `params`: `thresholds=>18:§a,>15:§e,<=15:§c`
  - **输出**: `"§a19.8765"`
- `params`: `math=round(_);prefix=[;suffix=]`
  - **输出**: `"[20]"`
