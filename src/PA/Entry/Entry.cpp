#include "PA/Entry/Entry.h"

#include "PA/BuiltinPlaceholders.h"
#include "PA/Config/ConfigManager.h" // 引入 ConfigManager
#include "PA/PlaceholderAPI.h"

#include "PA/ScriptExports.h" // 新增：脚本导出
#include "ll/api/mod/RegisterHelper.h"



namespace PA {

Entry& Entry::getInstance() {
    static Entry instance;
    return instance;
}

bool Entry::load() {
    getSelf().getLogger().debug("Loading...");
    ConfigManager::getInstance().load((getSelf().getConfigDir() / "config.json").string());
    ScriptExports::install();

    return true;
}

bool Entry::enable() {
    getSelf().getLogger().debug("Enabling...");
    // Code for enabling the mod goes here.
    registerAllBuiltinPlaceholders(PA_GetPlaceholderService());

    return true;
}

bool Entry::disable() {
    getSelf().getLogger().debug("Disabling...");
    // Code for disabling the mod goes here.

    return true;
}

} // namespace PA

LL_REGISTER_MOD(PA::Entry, PA::Entry::getInstance());
