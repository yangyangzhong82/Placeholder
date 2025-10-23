#include "PA/PlaceholderAPI.h"
#include "mc/world/actor/player/Player.h"
#include <memory>
#include <string>

// ======================================================================================
//  示例：如何使用上下文工厂为插件添加自定义上下文
// ======================================================================================

namespace PA::Example {

// 1. 定义自定义数据结构
//    这是你的插件中希望通过占位符访问的数据。
struct CustomData {
    int         someValue;
    std::string someString;
};

// 2. 定义自定义上下文
//    这个上下文将封装你的自定义数据，并实现 IContext 接口。
struct CustomDataContext : public IContext {
    static constexpr uint64_t kTypeId = TypeId("ctx:CustomData");
    const CustomData*         data{}; // 指向实际数据的指针

    uint64_t typeId() const noexcept override { return kTypeId; }

    std::string getContextInstanceKey() const noexcept override {
        // 提供一个唯一键，用于缓存。这里我们使用数据的内存地址。
        return data ? std::to_string(reinterpret_cast<uintptr_t>(data)) : "";
    }
};

// 3. 创建自定义上下文的工厂函数
//    这个函数接收一个 void* 指针（指向你的原始数据），并返回一个 unique_ptr<IContext>。
//    占位符系统将使用这个工厂来动态创建你的上下文实例。
std::unique_ptr<IContext> createCustomDataContext(void* rawObject) {
    if (!rawObject) {
        return nullptr;
    }
    auto ctx   = std::make_unique<CustomDataContext>();
    ctx->data = static_cast<const CustomData*>(rawObject);
    return ctx;
}

// 4. 创建一个作用于自定义上下文的占位符
//    这个占位符将从 CustomDataContext 中读取数据并返回。
class CustomDataPlaceholder final : public IPlaceholder {
public:
    std::string_view token() const noexcept override { return "custom_data_value"; }
    uint64_t         contextTypeId() const noexcept override { return CustomDataContext::kTypeId; }

    void evaluate(const IContext* ctx, std::string& out) const override {
        const auto* customCtx = static_cast<const CustomDataContext*>(ctx);
        if (customCtx && customCtx->data) {
            // 返回自定义数据中的值
            out = std::to_string(customCtx->data->someValue);
        } else {
            out.clear();
        }
    }
};

// 5. 创建一个解析器 (Resolver) 函数
//    这个函数定义了如何从一个已有的上下文（例如 PlayerContext）转换到你的自定义数据。
//    这里，我们假设每个玩家都有一个关联的 CustomData 实例。
//    在实际应用中，你可能需要从一个 map 或其他数据结构中查找。
void* resolveCustomDataFromPlayer(const IContext* fromCtx, const std::vector<std::string_view>&) {
    const auto* playerCtx = static_cast<const PlayerContext*>(fromCtx);
    if (!playerCtx || !playerCtx->player) {
        return nullptr;
    }

    // ** 关键部分 **
    // 在真实插件中，你会在这里查找与玩家关联的数据。
    // 为了演示，我们这里只是用一个静态变量。
    static CustomData playerData{123, "hello"};
    return &playerData;
}


// 6. 注册所有组件
//    这个函数将上面定义的所有部分注册到占位符服务中。
void registerCustomExample(IPlaceholderService* svc) {
    if (!svc) return;

    // 使用一个静态变量作为 owner，确保在插件生命周期内地址唯一且稳定。
    static int kMyPluginOwner = 0;
    void*      owner          = &kMyPluginOwner;

    // a. 注册我们的自定义占位符
    svc->registerPlaceholder("", std::make_shared<CustomDataPlaceholder>(), owner);

    // b. 注册我们的自定义上下文工厂
    svc->registerContextFactory(CustomDataContext::kTypeId, createCustomDataContext, owner);

    // c. 注册一个上下文别名，将 PlayerContext 链接到 CustomDataContext
    //    - 别名: "my_custom_alias"
    //    - 来源: PlayerContext
    //    - 目标: CustomDataContext
    //    - 解析器: resolveCustomDataFromPlayer
    svc->registerContextAlias(
        "my_custom_alias",
        PlayerContext::kTypeId,
        CustomDataContext::kTypeId,
        resolveCustomDataFromPlayer,
        owner
    );

    // 现在，当一个玩家在聊天或UI中使用占位符 {my_custom_alias:custom_data_value} 时，
    // 系统会自动：
    // 1. 识别出 "my_custom_alias" 是一个从 PlayerContext 出发的别名。
    // 2. 调用 resolveCustomDataFromPlayer()，获得一个指向 CustomData 的指针。
    // 3. 发现目标上下文类型是 CustomDataContext::kTypeId，并查找对应的工厂。
    // 4. 调用 createCustomDataContext()，传入 CustomData 指针，创建一个临时的 CustomDataContext 实例。
    // 5. 在这个临时的上下文实例上，解析内部的 "custom_data_value" 占位符。
    // 6. CustomDataPlaceholder::evaluate() 被调用，返回 "123"。
    // 最终结果就是 "123"。
}

} // namespace PA::Example

// 你可以在你的插件加载时调用 PA::Example::registerCustomExample(svc);
