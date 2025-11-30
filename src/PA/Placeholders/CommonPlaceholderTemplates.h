#pragma once

#include "PA/PlaceholderAPI.h"
#include "PA/logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

/*
 * ========== 占位符注册宏使用指南 ==========
 * 
 * 本文件提供了简化的宏来注册占位符，使代码更简洁易读。
 * 
 * 推荐使用以下宏进行占位符注册：
 * 
 * 1. PA_SIMPLE - 简单上下文占位符（无参数）
 *    示例: PA_SIMPLE(svc, owner, PlayerContext, "{player_name}", {
 *        out = c.player ? c.player->getName() : "N/A";
 *    });
 * 
 * 2. PA_CACHED - 带缓存的上下文占位符
 *    示例: PA_CACHED(svc, owner, PlayerContext, "{player_uuid}", 300, {
 *        out = c.player ? c.player->getUuid().asString() : "";
 *    });
 * 
 * 3. PA_WITH_ARGS - 带参数的上下文占位符
 *    示例: PA_WITH_ARGS(svc, owner, PlayerContext, "{player_score}", {
 *        if (!args.empty()) {
 *            out = getScore(c.player, args[0]);
 *        }
 *    });
 * 
 * 4. PA_SERVER - 服务器级占位符（无上下文）
 *    示例: PA_SERVER(svc, owner, "{online_players}", {
 *        out = std::to_string(getOnlineCount());
 *    });
 * 
 * 5. PA_SERVER_CACHED - 带缓存的服务器级占位符
 *    示例: PA_SERVER_CACHED(svc, owner, "{server_version}", 300, {
 *        out = getServerVersion();
 *    });
 * 
 * 6. PA_SERVER_WITH_ARGS - 带参数的服务器级占位符
 *    示例: PA_SERVER_WITH_ARGS(svc, owner, "{total_entities}", {
 *        bool excludeDrops = !args.empty() && args[0] == "exclude_drops";
 *        out = std::to_string(countEntities(excludeDrops));
 *    });
 * 
 * 注意事项：
 * - owner 参数用于标识占位符归属，建议使用模块内唯一的静态变量地址
 * - cache_duration 单位为秒
 * - lambda_body 中可以直接访问上下文变量 c 和输出变量 out
 * - 带参数的占位符可以通过 args 向量访问参数
 */

namespace PA {

// 泛型占位符实现（上下文型）
template <typename Ctx, typename Fn>
class PA_API TypedLambdaPlaceholder final : public PA::IPlaceholder {
public:
    TypedLambdaPlaceholder(std::string token, Fn fn, unsigned int cacheDuration = 0)
    : token_(std::move(token)),
      fn_(std::move(fn)),
      cacheDuration_(cacheDuration) {}

    std::string_view token() const noexcept override { return token_; }
    uint64_t         contextTypeId() const noexcept override { return Ctx::kTypeId; }
    unsigned int     getCacheDuration() const noexcept override { return cacheDuration_; }

    void evaluate(const PA::IContext* ctx, std::string& out) const override {
        const auto* c = static_cast<const Ctx*>(ctx);
        if constexpr (std::is_invocable_v<Fn, const Ctx&, std::string&>) {
            fn_(*c, out);
        } else {
            // This placeholder expects arguments, but none were provided.
            // Call it with an empty vector of arguments.
            fn_(*c, {}, out);
        }
    }

    void evaluateWithArgs(const PA::IContext* ctx, const std::vector<std::string_view>& args, std::string& out)
        const override {
        const auto* c = static_cast<const Ctx*>(ctx);
        if constexpr (std::is_invocable_v<Fn, const Ctx&, const std::vector<std::string_view>&, std::string&>) {
            fn_(*c, args, out);
        } else {
            // This placeholder doesn't accept arguments, call the non-arg version.
            fn_(*c, out);
        }
    }

private:
    std::string  token_;
    Fn           fn_;
    unsigned int cacheDuration_;
};

// 服务器占位符实现（无上下文）
template <typename Fn>
class PA_API ServerLambdaPlaceholder final : public PA::IPlaceholder {
public:
    ServerLambdaPlaceholder(std::string token, Fn fn, unsigned int cacheDuration = 0)
    : token_(std::move(token)),
      fn_(std::move(fn)),
      cacheDuration_(cacheDuration) {}

    std::string_view token() const noexcept override { return token_; }
    uint64_t         contextTypeId() const noexcept override { return PA::kServerContextId; }
    unsigned int     getCacheDuration() const noexcept override { return cacheDuration_; }

    void evaluate(const PA::IContext*, std::string& out) const override {
        if constexpr (std::is_invocable_v<Fn, std::string&>) {
            fn_(out);
        } else {
            // This placeholder expects arguments, but none were provided.
            // Call it with an empty vector of arguments.
            fn_(out, {});
        }
    }

    void evaluateWithArgs(const PA::IContext* ctx, const std::vector<std::string_view>& args, std::string& out)
        const override {
        if constexpr (std::is_invocable_v<Fn, std::string&, const std::vector<std::string_view>&>) {
            fn_(out, args);
        } else {
            // This placeholder doesn't accept arguments, call the non-arg version.
            fn_(out);
        }
    }

private:
    std::string  token_;
    Fn           fn_;
    unsigned int cacheDuration_;
};

// time 工具
inline std::tm local_tm(std::time_t t) {
    std::tm buf{};
#ifdef _WIN32
    localtime_s(&buf, &t);
#else
    localtime_r(&t, &buf);
#endif
    return buf;
}

// ========== 简化的占位符注册宏 ==========

// 简单上下文占位符（无参数）
#define PA_SIMPLE(svc, owner, ctx_type, token_str, lambda_body)                                                        \
    (svc)->registerPlaceholder(                                                                                        \
        "",                                                                                                            \
        std::make_shared<TypedLambdaPlaceholder<ctx_type, void (*)(const ctx_type&, std::string&)>>(                   \
            token_str,                                                                                                 \
            +[](const ctx_type& c, std::string& out) lambda_body                                                       \
        ),                                                                                                             \
        owner                                                                                                          \
    )

// 带缓存的上下文占位符
#define PA_CACHED(svc, owner, ctx_type, token_str, cache_duration, lambda_body)                                        \
    (svc)->registerPlaceholder(                                                                                        \
        "",                                                                                                            \
        std::make_shared<TypedLambdaPlaceholder<ctx_type, void (*)(const ctx_type&, std::string&)>>(                   \
            token_str,                                                                                                 \
            +[](const ctx_type& c, std::string& out) lambda_body,                                                      \
            cache_duration                                                                                             \
        ),                                                                                                             \
        owner                                                                                                          \
    )

// 带参数的上下文占位符
#define PA_WITH_ARGS(svc, owner, ctx_type, token_str, lambda_body)                                                     \
    (svc)->registerPlaceholder(                                                                                        \
        "",                                                                                                            \
        std::make_shared<TypedLambdaPlaceholder<                                                                       \
            ctx_type,                                                                                                  \
            void (*)(const ctx_type&, const std::vector<std::string_view>&, std::string&)>>(                           \
            token_str,                                                                                                 \
            +[](const ctx_type& c, const std::vector<std::string_view>& args, std::string& out) lambda_body            \
        ),                                                                                                             \
        owner                                                                                                          \
    )

// 带参数且带缓存的上下文占位符
#define PA_WITH_ARGS_CACHED(svc, owner, ctx_type, token_str, cache_duration, lambda_body)                              \
    (svc)->registerPlaceholder(                                                                                        \
        "",                                                                                                            \
        std::make_shared<TypedLambdaPlaceholder<                                                                       \
            ctx_type,                                                                                                  \
            void (*)(const ctx_type&, const std::vector<std::string_view>&, std::string&)>>(                           \
            token_str,                                                                                                 \
            +[](const ctx_type& c, const std::vector<std::string_view>& args, std::string& out) lambda_body,           \
            cache_duration                                                                                             \
        ),                                                                                                             \
        owner                                                                                                          \
    )

// 服务器级占位符（无参数）
#define PA_SERVER(svc, owner, token_str, lambda_body)                                                                  \
    (svc)->registerPlaceholder(                                                                                        \
        "",                                                                                                            \
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(                                             \
            token_str,                                                                                                 \
            +[](std::string & out) lambda_body                                                                         \
        ),                                                                                                             \
        owner                                                                                                          \
    )

// 带缓存的服务器级占位符
#define PA_SERVER_CACHED(svc, owner, token_str, cache_duration, lambda_body)                                           \
    (svc)->registerPlaceholder(                                                                                        \
        "",                                                                                                            \
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(                                             \
            token_str,                                                                                                 \
            +[](std::string & out) lambda_body,                                                                        \
            cache_duration                                                                                             \
        ),                                                                                                             \
        owner                                                                                                          \
    )

// 带参数的服务器级占位符
#define PA_SERVER_WITH_ARGS(svc, owner, token_str, lambda_body)                                                        \
    (svc)->registerPlaceholder(                                                                                        \
        "",                                                                                                            \
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&, const std::vector<std::string_view>&)>>(       \
            token_str,                                                                                                 \
            +[](std::string & out, const std::vector<std::string_view>& args) lambda_body                              \
        ),                                                                                                             \
        owner                                                                                                          \
    )

// 带参数且带缓存的服务器级占位符
#define PA_SERVER_WITH_ARGS_CACHED(svc, owner, token_str, cache_duration, lambda_body)                                 \
    (svc)->registerPlaceholder(                                                                                        \
        "",                                                                                                            \
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&, const std::vector<std::string_view>&)>>(       \
            token_str,                                                                                                 \
            +[](std::string & out, const std::vector<std::string_view>& args) lambda_body,                             \
            cache_duration                                                                                             \
        ),                                                                                                             \
        owner                                                                                                          \
    )

// ========== 旧版本宏（保持向后兼容） ==========
#define PA_REGISTER_SIMPLE_PLACEHOLDER(svc, owner, ctx_type, token_str, lambda_body)                                   \
    PA_SIMPLE(svc, owner, ctx_type, token_str, lambda_body)

#define PA_REGISTER_SIMPLE_SERVER_PLACEHOLDER(svc, owner, token_str, lambda_body)                                      \
    PA_SERVER(svc, owner, token_str, lambda_body)

#define PA_REGISTER_ARGS_PLACEHOLDER(svc, owner, ctx_type, token_str, lambda_body)                                     \
    PA_WITH_ARGS(svc, owner, ctx_type, token_str, lambda_body)

} // namespace PA
