#pragma once

#include "PA/PlaceholderAPI.h"

namespace PA {

struct IPlaceholderService;

void registerSystemPlaceholders(IPlaceholderService* service);

} // namespace PA
