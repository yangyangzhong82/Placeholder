#pragma once

#include "PA/PlaceholderAPI.h"

namespace PA {

struct IPlaceholderService;

void registerActorPlaceholders(IPlaceholderService* service);

} // namespace PA
