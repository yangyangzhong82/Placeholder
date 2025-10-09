// src/PA/ParameterParser.cpp
#include "PA/ParameterParser.h"
#include <vector>
#include <charconv>
#include <sstream>
#include <iomanip>

namespace PA {

namespace ParameterParser {

PlaceholderParams parse(std::string_view paramPart) {
    PlaceholderParams params;
    if (paramPart.empty()) {
        return params;
    }

    std::string              currentParam(paramPart);
    std::vector<std::string> paramSegments;
    size_t                   start = 0;
    size_t                   end   = currentParam.find(',');
    while (end != std::string::npos) {
        paramSegments.push_back(currentParam.substr(start, end - start));
        start = end + 1;
        end   = currentParam.find(',', start);
    }
    paramSegments.push_back(currentParam.substr(start));

    std::vector<std::string> remainingParams;
    for (const auto& p : paramSegments) {
        if (p.rfind("precision=", 0) == 0) {
            std::string_view precision_sv = std::string_view(p).substr(10); // "precision=".length()
            int              parsedPrecision;
            auto [prec_ptr, prec_ec] = std::from_chars(precision_sv.data(), precision_sv.data() + precision_sv.size(), parsedPrecision);
            if (prec_ec == std::errc()) {
                params.precision = parsedPrecision;
            }
        } else if (p.find('=') != std::string::npos) {
            size_t separatorPos               = p.find('=');
            params.otherParams[p.substr(0, separatorPos)] = p.substr(separatorPos + 1);
        } else {
            remainingParams.push_back(p);
        }
    }

    if (!remainingParams.empty()) {
        std::stringstream ss;
        for (size_t i = 0; i < remainingParams.size(); ++i) {
            ss << remainingParams[i];
            if (i < remainingParams.size() - 1) {
                ss << ",";
            }
        }
        params.colorParamPart = ss.str();
    }

    return params;
}

void formatNumericValue(std::string& evaluatedValue, int precision) {
    if (precision == -1) {
        return;
    }

    double value;
    auto [ptr, ec] = std::from_chars(evaluatedValue.data(), evaluatedValue.data() + evaluatedValue.size(), value);
    if (ec == std::errc()) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(precision) << value;
        evaluatedValue = ss.str();
    }
}

void applyColorRules(std::string& evaluatedValue, const std::string& colorParamPart, std::string_view colorFormat) {
    if (colorParamPart.empty()) {
        return;
    }

    auto applyFormat = [&](const std::string& color) {
        std::string formatted = std::string(colorFormat);
        size_t      pos;
        while ((pos = formatted.find("{color}")) != std::string::npos) {
            formatted.replace(pos, 7, color);
        }
        while ((pos = formatted.find("{value}")) != std::string::npos) {
            formatted.replace(pos, 7, evaluatedValue);
        }
        evaluatedValue = formatted;
    };

    std::string              currentParam(colorParamPart);
    std::vector<std::string> params;
    size_t                   start = 0;
    size_t                   end   = currentParam.find(',');
    while (end != std::string::npos) {
        params.push_back(currentParam.substr(start, end - start));
        start = end + 1;
        end   = currentParam.find(',', start);
    }
    params.push_back(currentParam.substr(start));

    if (params.size() == 1) {
        // If there is only one parameter, treat it as a color code
        applyFormat(params[0]);
        return;
    }

    double value;
    auto [ptr, ec] = std::from_chars(evaluatedValue.data(), evaluatedValue.data() + evaluatedValue.size(), value);
    if (ec != std::errc()) {
        // Not a numeric value, cannot apply threshold-based coloring
        return;
    }

    if (params.size() >= 3 && params.size() % 2 == 1) {
        // Format: {value,color_low,value,color_mid,default_color}
        std::string appliedColor = params.back(); // Default color
        for (size_t i = 0; i < params.size() - 1; i += 2) {
            double           threshold;
            std::string_view threshold_sv = params[i];
            auto [t_ptr, t_ec]            = std::from_chars(threshold_sv.data(), threshold_sv.data() + threshold_sv.size(), threshold);
            if (t_ec == std::errc()) {
                if (value < threshold) {
                    appliedColor = params[i + 1];
                    break;
                }
            }
        }
        applyFormat(appliedColor);
    }
}

} // namespace ParameterParser
} // namespace PA
