// PlaceholderAPI.h
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#if defined(_WIN32)
#ifdef PA_BUILD
#define PA_API __declspec(dllexport)
#else
#define PA_API __declspec(dllimport)
#endif
#else
#define PA_API __attribute__((visibility("default")))
#endif

// 前置声明游戏类型，避免在接口头中引入第三方头
class Player;
class Mob;

namespace PA {

// 64-bit FNV-1a 编译期哈希，用于生成稳定的上下文类型 ID
constexpr uint64_t fnv1a64_constexpr(const char* s, size_t n) {
    uint64_t hash = 1469598103934665603ull;
    for (size_t i = 0; i < n; ++i) {
        hash ^= static_cast<unsigned char>(s[i]);
        hash *= 1099511628211ull;
    }
    return hash;
}
template <size_t N>
constexpr uint64_t TypeId(const char (&str)[N]) {
    return fnv1a64_constexpr(str, N - 1);
}

// 占位符上下文基类：所有上下文均需提供稳定的 typeId()
struct PA_API IContext {
    virtual ~IContext()                      = default;
    virtual uint64_t typeId() const noexcept = 0;
};

// 约定：服务器级（无上下文）占位符的上下文 ID = 0
inline constexpr uint64_t kServerContextId = 0;

// 预定义的一些上下文（如需更多上下文，请扩展此处并保持 ID 字符串常量不变）

// 玩家上下文
struct PlayerContext final : public IContext {
    static constexpr uint64_t kTypeId = TypeId("ctx:Player");
    Player*                   player{};
    uint64_t                  typeId() const noexcept override { return kTypeId; }
};

// 生物上下文
struct MobContext final : public IContext {
    static constexpr uint64_t kTypeId = TypeId("ctx:Mob");
    Mob*                      mob{};
    uint64_t                  typeId() const noexcept override { return kTypeId; }
};

// 占位符抽象基类：通过继承来定义不同占位符
struct PA_API IPlaceholder {
    virtual ~IPlaceholder() = default;

    // 例如 "{player_name}"
    virtual std::string_view token() const noexcept = 0;

    // 绑定的上下文类型 ID；服务器级占位符为 kServerContextId(=0)
    virtual uint64_t contextTypeId() const noexcept = 0;

    // 计算替换文本；服务器级占位符 ctx 可为 nullptr
    // 输出写入 out，由调用方持有，不跨模块传递分配权
    virtual void evaluate(const IContext* ctx, std::string& out) const = 0;
};

// 跨模块服务接口（稳定 ABI）
struct PA_API IPlaceholderService {
    virtual ~IPlaceholderService() = default;

    // 注册占位符：通过 shared_ptr 共享所有权，由 owner 标识归属模块
    // prefix 为占位符前缀，用于解决命名冲突。
    // 最终 token 形式为 "{prefix_token_name}"，其中 "token_name" 来自 IPlaceholder::token() (去除 '{}')。
    // 若 prefix 为空，则 token 保持不变。
    virtual void registerPlaceholder(std::string_view prefix, std::shared_ptr<const IPlaceholder> p, void* owner) = 0;

    // 卸载 owner 名下的全部占位符（模块卸载时调用）
    virtual void unregisterByOwner(void* owner) = 0;

    // 带上下文的替换：先替换特定上下文，再替换服务器占位符
    virtual std::string replace(std::string_view text, const IContext* ctx) const = 0;

    // 仅替换服务器占位符
    virtual std::string replaceServer(std::string_view text) const = 0;
};

// 跨模块获取占位符服务单例
extern "C" PA_API IPlaceholderService* PA_GetPlaceholderService();

} // namespace PA
