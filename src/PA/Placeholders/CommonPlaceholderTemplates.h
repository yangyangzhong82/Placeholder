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

namespace PA {

// 泛型占位符实现（上下文型）
template <typename Ctx, typename Fn>
class TypedLambdaPlaceholder final : public PA::IPlaceholder {
public:
    TypedLambdaPlaceholder(std::string token, Fn fn, unsigned int cacheDuration = 0)
    : token_(std::move(token)), fn_(std::move(fn)), cacheDuration_(cacheDuration) {}

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

    void evaluateWithArgs(
        const PA::IContext*                           ctx,
        const std::vector<std::string_view>&          args,
        std::string&                                  out
    ) const override {
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
class ServerLambdaPlaceholder final : public PA::IPlaceholder {
public:
    ServerLambdaPlaceholder(std::string token, Fn fn, unsigned int cacheDuration = 0)
    : token_(std::move(token)), fn_(std::move(fn)), cacheDuration_(cacheDuration) {}

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

    void evaluateWithArgs(
        const PA::IContext*                           ctx,
        const std::vector<std::string_view>&          args,
        std::string&                                  out
    ) const override {
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

} // namespace PA
