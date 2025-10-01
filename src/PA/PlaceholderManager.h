#pragma once

#include <functional>
#include <string>
#include <unordered_map>


class Player;

namespace PA {

class PlaceholderManager {
public:
    static PlaceholderManager& getInstance();

    // 注册一个玩家占位符及其替换函数
    void registerPlayerPlaceholder(const std::string& placeholder, std::function<std::string(Player*)> replacer);

    // 注册一个服务器占位符及其替换函数 (不需要玩家上下文)
    void registerServerPlaceholder(const std::string& placeholder, std::function<std::string()> replacer);

    // 替换给定字符串中的所有占位符
    std::string replacePlaceholders(const std::string& text, Player* player = nullptr);

private:
    PlaceholderManager();
    std::unordered_map<std::string, std::function<std::string(Player*)>> mPlayerPlaceholders;
    std::unordered_map<std::string, std::function<std::string()>> mServerPlaceholders;
};

} // namespace PA
