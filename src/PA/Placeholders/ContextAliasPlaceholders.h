#pragma once

#include "PA/PlaceholderAPI.h"

namespace PA {

struct IPlaceholderService;

void registerContextAliasPlaceholders(IPlaceholderService* service);

} // namespace PA
