#pragma once

#include "PA/PlaceholderAPI.h"

namespace PA {

struct IPlaceholderService;

void registerTimePlaceholders(IPlaceholderService* service);

} // namespace PA
