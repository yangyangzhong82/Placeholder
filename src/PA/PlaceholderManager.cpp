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

void PlaceholderManager::registerPlaceholder(
    const std::string&                  placeholder,
    std::function<std::string(Player*)> replacer
) {
    mPlaceholders[placeholder] = replacer;
}

std::string PlaceholderManager::replacePlaceholders(const std::string& text, Player* player) {
    std::string result = text;
    for (const auto& pair : mPlaceholders) {
        size_t pos = result.find(pair.first);
        while (pos != std::string::npos) {
            result.replace(pos, pair.first.length(), pair.second(player));
            pos = result.find(pair.first, pos + pair.second(player).length());
        }
    }
    return result;
}

} // namespace Sidebar
