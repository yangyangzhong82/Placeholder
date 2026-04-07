// src/PA/PlaceholderProcessor.cpp
#include "PA/PlaceholderProcessor.h"
#include "PA/ParameterParser.h"
#include "PA/PlaceholderRegistry.h"
#include "PA/logger.h"
#include <array>
#include <sstream>
#include <vector>

namespace PA {

namespace {

constexpr std::array<std::string_view, 8> kFormattingPrefixes = {
    "precision=",
    "map=",
    "eq_eps=",
    "color_format=",
    "bool_map=",
    "char_map=",
    "regex_map=",
    "json_map=",
};

bool isFormattingParameter(std::string_view param) {
    for (auto prefix : kFormattingPrefixes) {
        if (param.rfind(prefix, 0) == 0) {
            return true;
        }
    }
    return false;
}

std::string buildCacheKey(const IContext* ctx, const std::string& cacheParamPart) {
    std::string contextKey = ctx ? ctx->getContextInstanceKey() : "";
    return contextKey + ":" + cacheParamPart;
}

} // namespace

size_t PlaceholderProcessor::findMatchingDelimiter(
    std::string_view text, size_t start_pos, char open_delim, char close_delim
) {
    int    nesting_level = 1;
    size_t scan_pos      = start_pos + 1;

    while (scan_pos < text.length()) {
        char current_char = text[scan_pos];
        if (current_char == '\\' && scan_pos + 1 < text.length()) {
            ++scan_pos;
        } else if (current_char == open_delim) {
            ++nesting_level;
        } else if (current_char == close_delim) {
            --nesting_level;
            if (nesting_level == 0) {
                return scan_pos;
            }
        }
        ++scan_pos;
    }

    return std::string_view::npos;
}

std::optional<PlaceholderMatch> PlaceholderProcessor::findNextPlaceholder(std::string_view text, size_t start_pos) {
    size_t placeholder_start = text.find_first_of("%{", start_pos);
    if (placeholder_start == std::string_view::npos) {
        return std::nullopt;
    }

    PlaceholderMatch match;
    match.start_pos = placeholder_start;

    char   open_delim = text[placeholder_start];
    char   close_delim = (open_delim == '{') ? '}' : '%';
    size_t end_pos = findMatchingDelimiter(text, placeholder_start, open_delim, close_delim);
    if (end_pos == std::string_view::npos) {
        return match;
    }

    match.end_pos   = end_pos;
    match.full_text = text.substr(placeholder_start, end_pos - placeholder_start + 1);
    match.content   = text.substr(placeholder_start + 1, end_pos - placeholder_start - 1);
    return match;
}

void PlaceholderProcessor::parsePlaceholderContent(
    PlaceholderMatch& match, const IContext* ctx, const PlaceholderRegistry& registry
) {
    match.token.clear();
    match.param_part.clear();
    match.placeholder.reset();
    match.cached_entry  = nullptr;
    match.snapshot_guard.reset();

    std::string content(match.content);
    size_t      pipe_pos_in_content = content.find('|');
    std::string token_search_part =
        (pipe_pos_in_content != std::string::npos) ? content.substr(0, pipe_pos_in_content) : content;

    for (size_t split_pos = token_search_part.length();;) {
        std::string potential_token = token_search_part.substr(0, split_pos);
        auto        find_result     = registry.findPlaceholder(potential_token, ctx);

        if (find_result.placeholder) {
            match.placeholder   = std::move(find_result.placeholder);
            match.cached_entry  = find_result.entry;
            match.snapshot_guard = std::move(find_result.snapshot_guard);
            match.token         = potential_token;

            if (split_pos < token_search_part.length()) {
                if (token_search_part[split_pos] == ':') {
                    match.param_part = token_search_part.substr(split_pos + 1);
                    if (pipe_pos_in_content != std::string::npos) {
                        match.param_part += "|" + content.substr(pipe_pos_in_content + 1);
                    }
                } else {
                    match.placeholder.reset();
                    match.cached_entry = nullptr;
                    match.snapshot_guard.reset();
                    match.token.clear();
                }
            } else if (pipe_pos_in_content != std::string::npos) {
                match.param_part = "|" + content.substr(pipe_pos_in_content + 1);
            }

            if (match.placeholder) {
                break;
            }
        }

        if (split_pos == 0) {
            break;
        }
        size_t prev_colon = token_search_part.rfind(':', split_pos - 1);
        if (prev_colon == std::string::npos) {
            break;
        }
        split_pos = prev_colon;
    }

    logger.debug(
        "1. Initial Parse: token='{}', param_part='{}', ctx_type_id={}",
        match.token,
        match.param_part,
        ctx ? ctx->typeId() : 0
    );
}

SeparatedParams PlaceholderProcessor::separateParameters(std::string_view param_part) {
    SeparatedParams separated;
    if (param_part.empty()) {
        return separated;
    }

    size_t pipe_pos = param_part.find('|');
    if (pipe_pos != std::string_view::npos) {
        separated.cache_param_part      = std::string(param_part.substr(0, pipe_pos));
        separated.formatting_param_part = std::string(param_part.substr(pipe_pos + 1));
        return separated;
    }

    std::vector<std::string> param_segments = ParameterParser::splitParamString(param_part, ',');
    std::stringstream        raw_param_stream;
    std::stringstream        formatting_stream;
    bool                     first_raw        = true;
    bool                     first_formatting = true;

    for (const auto& param : param_segments) {
        bool  is_formatting = isFormattingParameter(param);
        auto& target_stream = is_formatting ? formatting_stream : raw_param_stream;
        bool& first_item    = is_formatting ? first_formatting : first_raw;
        if (!first_item) {
            target_stream << ",";
        }
        target_stream << param;
        first_item = false;
    }

    separated.cache_param_part      = raw_param_stream.str();
    separated.formatting_param_part = formatting_stream.str();
    return separated;
}

bool PlaceholderProcessor::tryGetCachedValue(
    const CachedEntry* entry, const IContext* ctx, const std::string& cache_param_part, std::string& out
) {
    if (!entry) {
        return false;
    }

    std::string contextKey = ctx ? ctx->getContextInstanceKey() : "";
    std::string cacheKey   = buildCacheKey(ctx, cache_param_part);

    logger.debug(
        "Cache Check: contextKey='{}', cache_param_part='{}', fullCacheKey='{}', cacheDuration={}",
        contextKey,
        cache_param_part,
        cacheKey,
        entry->cacheDuration
    );

    std::lock_guard<std::mutex> lock(entry->cacheMutex);
    auto                        it = entry->cachedValues.find(cacheKey);
    if (it == entry->cachedValues.end()) {
        logger.debug("Cache Miss: No entry found for cacheKey='{}'", cacheKey);
        return false;
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.lastEvaluated).count();
    logger.debug(
        "Cache Entry Found: lastEvaluated={}, now={}, elapsedSeconds={}, cacheDuration={}",
        std::chrono::duration_cast<std::chrono::seconds>(it->second.lastEvaluated.time_since_epoch()).count(),
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count(),
        elapsedSeconds,
        entry->cacheDuration
    );

    if (elapsedSeconds >= entry->cacheDuration) {
        logger.debug("Cache Expired: elapsedSeconds={} >= cacheDuration={}", elapsedSeconds, entry->cacheDuration);
        return false;
    }

    out = it->second.value;
    logger.debug("3. Cache Hit: evaluatedValue='{}'", out);
    return true;
}

void PlaceholderProcessor::evaluateWithContext(
    const IPlaceholder* placeholder,
    const IContext*     ctx,
    std::string_view    raw_param_part,
    const std::string&  cache_param_part,
    std::string&        out
) {
    if (!placeholder) {
        return;
    }

    if (placeholder->isContextAliasPlaceholder()) {
        std::vector<std::string_view> args;
        if (!raw_param_part.empty()) {
            args.push_back(raw_param_part);
        }
        placeholder->evaluateWithArgs(ctx, args, out);
        return;
    }

    if (cache_param_part.empty()) {
        placeholder->evaluate(ctx, out);
        return;
    }

    std::vector<std::string>      placeholder_args = ParameterParser::splitParamString(cache_param_part, ',');
    std::vector<std::string_view> args;
    args.reserve(placeholder_args.size());
    for (const auto& arg : placeholder_args) {
        args.push_back(arg);
    }
    placeholder->evaluateWithArgs(ctx, args, out);
}

void PlaceholderProcessor::updateCache(
    const CachedEntry* entry, const IContext* ctx, const std::string& cache_param_part, const std::string& value
) {
    if (!entry) {
        return;
    }

    std::string cacheKey = buildCacheKey(ctx, cache_param_part);

    std::lock_guard<std::mutex> lock(entry->cacheMutex);
    entry->cachedValues[cacheKey] = {value, std::chrono::steady_clock::now()};
    logger.debug("3.5. Cache Updated: cacheKey='{}', evaluatedValue='{}'", cacheKey, value);
}

void PlaceholderProcessor::applyFormatting(std::string& value, const std::string& formatting_param_part) {
    if (formatting_param_part.empty()) {
        return;
    }

    auto params = ParameterParser::parse(formatting_param_part);
    ParameterParser::applyConditionalOutput(value, params.conditional);
    logger.debug("4. After applyConditionalOutput: evaluatedValue='{}'", value);
    ParameterParser::formatNumericValue(value, params.precision);
    logger.debug("5. After formatNumericValue: evaluatedValue='{}'", value);
    ParameterParser::applyBooleanMap(value, params.booleanMap);
    logger.debug("5.5. After applyBooleanMap: evaluatedValue='{}'", value);
    ParameterParser::applyCharReplaceMap(value, params.charReplaceMap);
    logger.debug("5.6. After applyCharReplaceMap: evaluatedValue='{}'", value);
    ParameterParser::applyRegexReplaceMap(value, params.regexReplaceMap);
    logger.debug("5.7. After applyRegexReplaceMap: evaluatedValue='{}'", value);
    ParameterParser::applyJsonMap(value, params.jsonMap);
    logger.debug("5.8. After applyJsonMap: evaluatedValue='{}'", value);

    std::string_view colorFormat = "{color}{value}";
    auto             colorFormatIt = params.otherParams.find("color_format");
    if (colorFormatIt != params.otherParams.end()) {
        colorFormat = colorFormatIt->second;
    }

    if (!params.conditional.enabled) {
        ParameterParser::applyColorRules(value, params.colorParamPart, colorFormat);
    }
    logger.debug("6. After applyColorRules: evaluatedValue='{}'", value);
}

std::string
PlaceholderProcessor::process(std::string_view text, const IContext* ctx, const PlaceholderRegistry& registry) {
    std::string result;
    result.reserve(text.length());
    size_t pos = 0;

    while (pos < text.length()) {
        auto match = findNextPlaceholder(text, pos);
        if (!match) {
            result.append(text.substr(pos));
            break;
        }

        result.append(text.substr(pos, match->start_pos - pos));

        if (!match->isValid()) {
            result.push_back(text[match->start_pos]);
            pos = match->start_pos + 1;
            continue;
        }

        parsePlaceholderContent(*match, ctx, registry);
        if (!match->placeholder) {
            result.append(match->full_text);
            pos = match->end_pos + 1;
            continue;
        }

        auto separated = separateParameters(match->param_part);
        logger.debug(
            "2. Separated Params: placeholder_param='{}', formatting_param='{}'",
            separated.cache_param_part,
            separated.formatting_param_part
        );

        std::string evaluatedValue;
        bool        useCachedValue = tryGetCachedValue(match->cached_entry, ctx, separated.cache_param_part, evaluatedValue);
        if (!useCachedValue) {
            logger.debug("Cache Miss or Expired: Re-evaluating placeholder.");
            evaluateWithContext(
                match->placeholder.get(),
                ctx,
                match->param_part,
                separated.cache_param_part,
                evaluatedValue
            );
            logger.debug("3. After Evaluate: evaluatedValue='{}'", evaluatedValue);
            updateCache(match->cached_entry, ctx, separated.cache_param_part, evaluatedValue);
        }

        applyFormatting(evaluatedValue, separated.formatting_param_part);
        logger.debug("7. Final Value: evaluatedValue='{}'", evaluatedValue);
        result.append(evaluatedValue);
        pos = match->end_pos + 1;
    }

    return result;
}

std::string PlaceholderProcessor::processServer(std::string_view text, const PlaceholderRegistry& registry) {
    return process(text, nullptr, registry);
}

} // namespace PA
