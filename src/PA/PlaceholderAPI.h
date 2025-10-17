// PlaceholderAPI.h
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#if defined(_WIN32)
#ifdef PA_BUILD
#define PA_API __declspec(dllexport)
#else
#define PA_API __declspec(dllimport)
#endif
#else
#define PA_API __attribute__((visibility("default")))
#endif


class Player;
class Mob;
class Actor; // Add Actor forward declaration

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
    // 新增方法：获取所有继承的上下文类型 ID 列表，包括自身
    virtual std::vector<uint64_t> getInheritedTypeIds() const noexcept {
        return {typeId()}; // 默认只返回自己的 typeId
    }
};

// 约定：服务器级（无上下文）占位符的上下文 ID = 0
inline constexpr uint64_t kServerContextId = 0;

// 预定义的一些上下文（如需更多上下文，请扩展此处并保持 ID 字符串常量不变）

// Actor 上下文
struct PA_API ActorContext : public IContext { // 移除 final
    static constexpr uint64_t kTypeId = TypeId("ctx:Actor");
    Actor*                    actor{};
    uint64_t                  typeId() const noexcept override { return kTypeId; }
    std::vector<uint64_t> getInheritedTypeIds() const noexcept override {
        return {kTypeId}; // Actor 不继承其他上下文
    }
};

// 生物上下文
struct PA_API MobContext : public ActorContext { // 移除 final, Mob 继承自 Actor
    static constexpr uint64_t kTypeId = TypeId("ctx:Mob");
    Mob*                      mob{};
    uint64_t                  typeId() const noexcept override { return kTypeId; }
    std::vector<uint64_t> getInheritedTypeIds() const noexcept override {
        std::vector<uint64_t> inherited = ActorContext::getInheritedTypeIds();
        inherited.push_back(kTypeId);
        return inherited;
    }
};

// 玩家上下文
struct PA_API PlayerContext : public MobContext { // 移除 final, Player 继承自 Mob
    static constexpr uint64_t kTypeId = TypeId("ctx:Player");
    Player*                   player{};
    uint64_t                  typeId() const noexcept override { return kTypeId; }
    std::vector<uint64_t> getInheritedTypeIds() const noexcept override {
        std::vector<uint64_t> inherited = MobContext::getInheritedTypeIds();
        inherited.push_back(kTypeId);
        return inherited;
    }
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

    // 新增带参数的 evaluate 方法
    virtual void evaluateWithArgs(
        const IContext*                           ctx,
        const std::vector<std::string_view>&      args,
        std::string&                              out
    ) const {
        // 默认实现调用无参数的 evaluate
        evaluate(ctx, out);
    }

    // 新增方法：获取缓存持续时间（秒）。返回 0 表示不缓存。
    virtual unsigned int getCacheDuration() const noexcept { return 0; }
};

// 缓存占位符抽象基类：继承自 IPlaceholder，并强制实现 getCacheDuration
struct PA_API ICachedPlaceholder : public IPlaceholder {
    // 强制实现 getCacheDuration，返回大于 0 的值表示启用缓存
    virtual unsigned int getCacheDuration() const noexcept override = 0;
};

// 颜色代码定义
#define PA_COLOR_RED    "§c"
#define PA_COLOR_YELLOW "§e"
#define PA_COLOR_GREEN  "§a"
#define PA_COLOR_RESET  "§r" // 重置颜色

// 跨模块服务接口（稳定 ABI）
struct PA_API IPlaceholderService {
    virtual ~IPlaceholderService() = default;

    // 注册占位符：通过 shared_ptr 共享所有权，由 owner 标识归属模块
    // prefix 为占位符前缀，用于解决命名冲突。
    // 最终 token 形式为 "{prefix:token_name}"，其中 "token_name" 来自 IPlaceholder::token() (去除 '{}')。
    // 若 prefix 为空，则 token 保持不变。
    virtual void registerPlaceholder(std::string_view prefix, std::shared_ptr<const IPlaceholder> p, void* owner) = 0;

    // 注册缓存占位符：通过 shared_ptr 共享所有权，由 owner 标识归属模块
    // prefix 为占位符前缀，用于解决命名冲突。
    // 最终 token 形式为 "{prefix:token_name}"，其中 "token_name" 来自 IPlaceholder::token() (去除 '{}')。
    // 若 prefix 为空，则 token 保持不变。
    // cacheDuration 为缓存持续时间（秒）。
    virtual void registerCachedPlaceholder(std::string_view prefix, std::shared_ptr<const IPlaceholder> p, void* owner, unsigned int cacheDuration) = 0;

    // 注册关系型占位符：通过 shared_ptr 共享所有权，由 owner 标识归属模块
    // prefix 为占位符前缀，用于解决命名冲突。
    // 最终 token 形式为 "{prefix:token_name}"，其中 "token_name" 来自 IPlaceholder::token() (去除 '{}')。
    // 若 prefix 为空，则 token 保持不变。
    // mainContextTypeId 为主上下文类型 ID，relationalContextTypeId 为关系上下文类型 ID。
    virtual void registerRelationalPlaceholder(std::string_view prefix, std::shared_ptr<const IPlaceholder> p, void* owner, uint64_t mainContextTypeId, uint64_t relationalContextTypeId) = 0;

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
