#pragma once

#include "PA/PlaceholderAPI.h"

namespace PA {

struct IPlaceholderService;

void registerPlayerPlaceholders(IPlaceholderService* service);

} // namespace PA
