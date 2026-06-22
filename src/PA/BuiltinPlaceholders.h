#pragma once

namespace PA {

struct IPlaceholderService;

void registerAllBuiltinPlaceholders(IPlaceholderService* service);
void unregisterAllBuiltinPlaceholders(IPlaceholderService* service);

} // namespace PA
