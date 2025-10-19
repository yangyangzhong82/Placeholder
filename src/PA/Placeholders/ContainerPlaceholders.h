#pragma once

#include "PA/PlaceholderAPI.h"

namespace PA {

struct IPlaceholderService;

void registerContainerPlaceholders(IPlaceholderService* service);

} // namespace PA
