#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <variant> // 用于统一存储

class Player;

namespace PA {

class PlaceholderManager {
public:
    // 为不同类型的占位符定义清晰的函数签名
    using ServerReplacer = std::function<std::string()>;
    using PlayerReplacer = std::function<std::string(Player*)>;

    // 使用 std::variant 统一两种类型的 Replacer
    using AnyReplacer = std::variant<ServerReplacer, PlayerReplacer>;

    // 获取全局唯一的实例（单例模式）
    static PlaceholderManager& getInstance();

    // 注册一个服务器占位符（不需要 Player 指针）
    void registerServerPlaceholder(const std::string& placeholder, ServerReplacer replacer);

    // 注册一个玩家占位符（需要 Player 指针）
    void registerPlayerPlaceholder(const std::string& placeholder, PlayerReplacer replacer);

    // [重载] 替换占位符，自动处理服务器和玩家类型
    // 当 player 为 nullptr 时，只会替换服务器占位符
    std::string replacePlaceholders(const std::string& text, Player* player = nullptr);

private:
    // 私有化构造函数和析构函数，以实现单例模式
    PlaceholderManager();
    ~PlaceholderManager() = default;

    // 禁用拷贝和赋值
    PlaceholderManager(const PlaceholderManager&)            = delete;
    PlaceholderManager& operator=(const PlaceholderManager&) = delete;

    // 使用一个 map 存储所有占位符
    std::unordered_map<std::string, AnyReplacer> mPlaceholders;
};

} // namespace PA