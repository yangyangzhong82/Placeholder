// src/PA/PlaceholderProcessor.cpp
#include "PA/PlaceholderProcessor.h"
#include "PA/PlaceholderRegistry.h"
#include "PA/ParameterParser.h"
#include <vector>

namespace PA {

std::string PlaceholderProcessor::process(std::string_view text, const IContext* ctx, const PlaceholderRegistry& registry) {
    std::string result(text);

    auto typedList = registry.getTypedPlaceholders(ctx);
    for (auto& kv : typedList) {
        replaceAll(result, kv.first, kv.second.get(), ctx);
    }

    auto serverList = registry.getServerPlaceholders();
    for (auto& kv : serverList) {
        replaceAll(result, kv.first, kv.second.get(), nullptr);
    }

    return result;
}

std::string PlaceholderProcessor::processServer(std::string_view text, const PlaceholderRegistry& registry) {
    std::string result(text);

    auto serverList = registry.getServerPlaceholders();
    for (auto& kv : serverList) {
        replaceAll(result, kv.first, kv.second.get(), nullptr);
    }

    return result;
}

void PlaceholderProcessor::replaceAll(std::string& text, const std::string& token, const IPlaceholder* ph, const IContext* ctx) {
    if (!ph) return;

    std::string_view innerToken;
    if (token.length() > 2 && token.front() == '{' && token.back() == '}') {
        innerToken = std::string_view(token).substr(1, token.length() - 2);
    } else {
        innerToken = token;
    }

    std::string searchPrefix = "{" + std::string(innerToken);
    size_t      pos          = 0;

    while ((pos = text.find(searchPrefix, pos)) != std::string::npos) {
        size_t endPos = text.find('}', pos);
        if (endPos == std::string::npos) {
            pos += searchPrefix.length();
            continue;
        }

        std::string_view fullMatch           = std::string_view(text).substr(pos, endPos - pos + 1);
        std::string_view contentInsideBraces = std::string_view(text).substr(pos + 1, endPos - (pos + 1));

        std::string_view actualTokenPart;
        std::string_view paramPart;
        size_t           colonPos = contentInsideBraces.find(':');
        if (colonPos != std::string::npos) {
            actualTokenPart = contentInsideBraces.substr(0, colonPos);
            paramPart       = contentInsideBraces.substr(colonPos + 1);
        } else {
            actualTokenPart = contentInsideBraces;
        }

        if (actualTokenPart != innerToken) {
            pos = endPos + 1;
            continue;
        }

        std::string evaluatedValue;
        if (!paramPart.empty()) {
            ph->evaluateWithParam(ctx, paramPart, evaluatedValue);
        } else {
            ph->evaluate(ctx, evaluatedValue);
        }

        auto params = ParameterParser::parse(paramPart);
        ParameterParser::formatNumericValue(evaluatedValue, params.precision);
        ParameterParser::applyColorRules(evaluatedValue, params.colorParamPart);

        text.replace(pos, fullMatch.length(), evaluatedValue);
        pos += evaluatedValue.length();
    }
}

} // namespace PA
