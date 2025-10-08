// src/PA/PlaceholderProcessor.h
#pragma once

#include "PA/PlaceholderAPI.h"
#include <string>
#include <string_view>

namespace PA {

class PlaceholderProcessor {
public:
    static std::string process(std::string_view text, const IContext* ctx, const class PlaceholderRegistry& registry);
    static std::string processServer(std::string_view text, const class PlaceholderRegistry& registry);

private:
    static void replaceAll(std::string& text, const std::string& token, const IPlaceholder* ph, const IContext* ctx);
};

} // namespace PA
