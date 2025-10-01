#include "PlaceholderManager.h"


#include "ll/api/service/Bedrock.h"
#include "mc/server/ServerPlayer.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/Level.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>


namespace PA {

PlaceholderManager& PlaceholderManager::getInstance() {
    static PlaceholderManager instance;
    return instance;
}

PlaceholderManager::PlaceholderManager() {}

void PlaceholderManager::registerPlayerPlaceholder(
    const std::string&                  placeholder,
    std::function<std::string(Player*)> replacer
) {
    mPlayerPlaceholders[placeholder] = replacer;
}

void PlaceholderManager::registerServerPlaceholder(
    const std::string&                placeholder,
    std::function<std::string()> replacer
) {
    mServerPlaceholders[placeholder] = replacer;
}

std::string PlaceholderManager::replacePlaceholders(const std::string& text, Player* player) {
    std::string result = text;

    // 替换玩家占位符
    for (const auto& pair : mPlayerPlaceholders) {
        size_t pos = result.find(pair.first);
        while (pos != std::string::npos) {
            result.replace(pos, pair.first.length(), pair.second(player));
            pos = result.find(pair.first, pos + pair.second(player).length());
        }
    }

    // 替换服务器占位符
    for (const auto& pair : mServerPlaceholders) {
        size_t pos = result.find(pair.first);
        while (pos != std::string::npos) {
            result.replace(pos, pair.first.length(), pair.second());
            pos = result.find(pair.first, pos + pair.second().length());
        }
    }

    return result;
}

} // namespace PA
