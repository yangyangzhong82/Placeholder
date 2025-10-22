#pragma once

#include "PA/PlaceholderAPI.h"

namespace PA {

struct IPlaceholderService;

void registerWorldCoordinatePlaceholders(IPlaceholderService* service);

} // namespace PA
