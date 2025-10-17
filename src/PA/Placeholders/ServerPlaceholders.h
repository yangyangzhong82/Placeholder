#pragma once

#include "PA/PlaceholderAPI.h"

namespace PA {

struct IPlaceholderService;

void registerServerPlaceholders(IPlaceholderService* service);

} // namespace PA
