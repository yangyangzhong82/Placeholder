#pragma once

#include <functional>
#include <string>
#include <unordered_map>


class Player;

namespace PA {

class PlaceholderManager {
public:
    static PlaceholderManager& getInstance();

    // 注册一个占位符及其替换函数
    void registerPlaceholder(const std::string& placeholder, std::function<std::string(Player*)> replacer);

    // 替换给定字符串中的所有占位符
    std::string replacePlaceholders(const std::string& text, Player* player);

private:
    PlaceholderManager();
    std::unordered_map<std::string, std::function<std::string(Player*)>> mPlaceholders;
};

} // namespace Sidebar
