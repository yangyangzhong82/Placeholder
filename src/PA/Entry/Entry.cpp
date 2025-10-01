#include "PA/Entry/Entry.h"

#include "PA/BuiltinPlaceholders.h"
#include "PA/PlaceholderManager.h"
#include "ll/api/mod/RegisterHelper.h"
#include "PA/Config/ConfigManager.h" // 引入 ConfigManager

namespace PA {

Entry& Entry::getInstance() {
    static Entry instance;
    return instance;
}

bool Entry::load() {
    getSelf().getLogger().debug("Loading...");
    ConfigManager::getInstance().load((getSelf().getConfigDir() / "config.json").string());

    registerBuiltinPlaceholders(PlaceholderManager::getInstance());

    return true;
}

bool Entry::enable() {
    getSelf().getLogger().debug("Enabling...");
    // Code for enabling the mod goes here.
    return true;
}

bool Entry::disable() {
    getSelf().getLogger().debug("Disabling...");
    // Code for disabling the mod goes here.
    return true;
}

} // namespace PA

LL_REGISTER_MOD(PA::Entry, PA::Entry::getInstance());
