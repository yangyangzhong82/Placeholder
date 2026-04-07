// src/PA/PlaceholderProcessor.h
#pragma once

#include "PA/PlaceholderAPI.h"
#include <optional>
#include <string>
#include <string_view>

namespace PA {

// 前向声明
class PlaceholderRegistry;
struct CachedEntry;

// ========== 辅助结构体 ==========

/**
 * @brief 占位符匹配结果
 * 包含从文本中提取的占位符信息
 */
struct PlaceholderMatch {
    size_t           start_pos{}; // 起始位置
    size_t           end_pos{};   // 结束位置
    std::string_view full_text;   // 完整文本 {xxx}
    std::string_view content;     // 内容部分 xxx
    std::string      token;       // token部分
    std::string      param_part;  // 参数部分
    std::shared_ptr<const IPlaceholder> placeholder;
    const CachedEntry*                  cached_entry = nullptr;
    std::shared_ptr<const void>         snapshot_guard;

    bool isValid() const noexcept { return end_pos > start_pos; }
};

/**
 * @brief 参数分离结果
 * 将参数分为缓存参数和格式化参数
 */
struct SeparatedParams {
    std::string cache_param_part;      // 用于缓存键的参数
    std::string formatting_param_part; // 用于格式化的参数
};

/**
 * @brief 占位符处理器
 * 负责解析和替换文本中的占位符
 */
class PlaceholderProcessor {
public:
    /**
     * @brief 处理文本中的占位符替换
     * @param text 包含占位符的原始文本
     * @param ctx 上下文对象，用于获取占位符值
     * @param registry 占位符注册表
     * @return 替换后的文本
     */
    static std::string process(std::string_view text, const IContext* ctx, const PlaceholderRegistry& registry);

    /**
     * @brief 仅替换服务器级占位符
     * @param text 包含占位符的原始文本
     * @param registry 占位符注册表
     * @return 替换后的文本
     */
    static std::string processServer(std::string_view text, const PlaceholderRegistry& registry);

private:
    // ========== 查找相关 ==========

    /**
     * @brief 查找下一个占位符
     * @param text 文本内容
     * @param start_pos 开始查找的位置
     * @return 占位符匹配结果，未找到则返回 nullopt
     */
    static std::optional<PlaceholderMatch> findNextPlaceholder(std::string_view text, size_t start_pos);

    /**
     * @brief 查找匹配的定界符（处理嵌套）
     * @param text 文本内容
     * @param start_pos 起始位置
     * @param open_delim 开放定界符
     * @param close_delim 闭合定界符
     * @return 闭合定界符位置，未找到则返回 npos
     */
    static size_t findMatchingDelimiter(std::string_view text, size_t start_pos, char open_delim, char close_delim);

    // ========== 解析相关 ==========

    /**
     * @brief 解析占位符内容，分离token和参数
     * @param match 占位符匹配结果（输入输出参数）
     * @param ctx 上下文对象
     * @param registry 占位符注册表
     */
    static void
    parsePlaceholderContent(PlaceholderMatch& match, const IContext* ctx, const PlaceholderRegistry& registry);

    /**
     * @brief 分离缓存参数和格式化参数
     * @param param_part 参数字符串
     * @return 分离后的参数
     */
    static SeparatedParams separateParameters(std::string_view param_part);

    // ========== 求值相关 ==========

    /**
     * @brief 尝试从缓存获取值
     * @param entry 缓存条目
     * @param ctx 上下文对象
     * @param cache_param_part 缓存参数
     * @param out 输出结果
     * @return 是否成功从缓存获取
     */
    static bool tryGetCachedValue(
        const CachedEntry* entry,
        const IContext*    ctx,
        const std::string& cache_param_part,
        std::string&       out
    );

    /**
     * @brief 执行占位符求值
     * @param placeholder 占位符对象
     * @param ctx 上下文对象
     * @param cache_param_part 缓存参数
     * @param out 输出结果
     */
    static void evaluateWithContext(
        const IPlaceholder* placeholder,
        const IContext*     ctx,
        std::string_view    raw_param_part,
        const std::string&  cache_param_part,
        std::string&        out
    );

    /**
     * @brief 更新缓存
     * @param entry 缓存条目
     * @param ctx 上下文对象
     * @param cache_param_part 缓存参数
     * @param value 要缓存的值
     */
    static void updateCache(
        const CachedEntry* entry,
        const IContext*    ctx,
        const std::string& cache_param_part,
        const std::string& value
    );

    // ========== 格式化相关 ==========

    /**
     * @brief 应用所有格式化规则
     * @param value 要格式化的值（输入输出参数）
     * @param formatting_param_part 格式化参数
     */
    static void applyFormatting(std::string& value, const std::string& formatting_param_part);
};

} // namespace PA
