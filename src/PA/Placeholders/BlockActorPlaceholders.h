#pragma once

#include "PA/PlaceholderAPI.h"

namespace PA {

struct IPlaceholderService;

void registerBlockActorPlaceholders(IPlaceholderService* service);

} // namespace PA
