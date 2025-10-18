#pragma once

#include "PA/PlaceholderAPI.h"

namespace PA {

struct IPlaceholderService;

void registerBlockPlaceholders(IPlaceholderService* service);

} // namespace PA
