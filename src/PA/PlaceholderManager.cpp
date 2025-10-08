// PlaceholderManager.cpp
#define PA_BUILD
#include "PA/PlaceholderAPI.h"

#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <charconv> // For std::from_chars
#include <iomanip>  // For std::fixed, std::setprecision
#include <sstream>  // For std::stringstream

namespace PA {

class PlaceholderManager final : public IPlaceholderService {
public:
    void registerPlaceholder(std::string_view prefix, std::shared_ptr<const IPlaceholder> p, void* owner) override {
        if (!p) return;

        std::string      key;
        std::string_view token_sv = p->token();

        if (prefix.empty()) {
            key = token_sv;
        } else {
            if (token_sv.length() > 2 && token_sv.front() == '{' && token_sv.back() == '}') {
                std::string_view inner_token = token_sv.substr(1, token_sv.length() - 2);
                key                          = "{" + std::string(prefix) + ":" + std::string(inner_token) + "}";
            } else {
                // Fallback for tokens not in "{...}" format, just use the original token.
                key = token_sv;
            }
        }

        std::lock_guard<std::mutex> lk(mMutex);
        const uint64_t              ctxId = p->contextTypeId();

        if (ctxId == kServerContextId) {
            mServer[key] = {p, owner};
            mOwnerIndex[owner].push_back({true, false, 0, 0, 0, key}); // isServer, isRelational, mainCtxId, relCtxId, ctxId, token
        } else {
            mTyped[ctxId][key] = {p, owner};
            mOwnerIndex[owner].push_back({false, false, 0, 0, ctxId, key}); // isServer, isRelational, mainCtxId, relCtxId, ctxId, token
        }
    }

    void registerRelationalPlaceholder(std::string_view prefix, std::shared_ptr<const IPlaceholder> p, void* owner, uint64_t mainContextTypeId, uint64_t relationalContextTypeId) override {
        if (!p) return;

        std::string      key;
        std::string_view token_sv = p->token();

        if (prefix.empty()) {
            key = token_sv;
        } else {
            if (token_sv.length() > 2 && token_sv.front() == '{' && token_sv.back() == '}') {
                std::string_view inner_token = token_sv.substr(1, token_sv.length() - 2);
                key                          = "{" + std::string(prefix) + ":" + std::string(inner_token) + "}";
            } else {
                key = token_sv;
            }
        }

        std::lock_guard<std::mutex> lk(mMutex);
        mRelational[mainContextTypeId][relationalContextTypeId][key] = {p, owner};
        mOwnerIndex[owner].push_back({false, true, mainContextTypeId, relationalContextTypeId, 0, key}); // isServer, isRelational, mainCtxId, relCtxId, ctxId, token
    }

    void unregisterByOwner(void* owner) override {
        std::lock_guard<std::mutex> lk(mMutex);
        auto                        it = mOwnerIndex.find(owner);
        if (it == mOwnerIndex.end()) return;

        for (const Handle& h : it->second) {
            if (h.isServer) {
                auto sit = mServer.find(h.token);
                if (sit != mServer.end() && sit->second.owner == owner) {
                    mServer.erase(sit);
                }
            } else if (h.isRelational) {
                auto mainIt = mRelational.find(h.mainCtxId);
                if (mainIt != mRelational.end()) {
                    auto relIt = mainIt->second.find(h.relCtxId);
                    if (relIt != mainIt->second.end()) {
                        auto pit = relIt->second.find(h.token);
                        if (pit != relIt->second.end() && pit->second.owner == owner) {
                            relIt->second.erase(pit);
                            if (relIt->second.empty()) mainIt->second.erase(relIt);
                            if (mainIt->second.empty()) mRelational.erase(mainIt);
                        }
                    }
                }
            }
            else { // 普通占位符
                auto tit = mTyped.find(h.ctxId);
                if (tit != mTyped.end()) {
                    auto pit = tit->second.find(h.token);
                    if (pit != tit->second.end() && pit->second.owner == owner) {
                        tit->second.erase(pit);
                        if (tit->second.empty()) mTyped.erase(tit);
                    }
                }
            }
        }
        mOwnerIndex.erase(it);
    }

    std::string replace(std::string_view text, const IContext* ctx) const override {
        // 拷贝指针快照，避免持锁执行用户代码
        std::vector<std::pair<std::string, std::shared_ptr<const IPlaceholder>>> typedList;
        std::vector<std::pair<std::string, std::shared_ptr<const IPlaceholder>>> serverList;

        {
            std::lock_guard<std::mutex> lk(mMutex);
            if (ctx) {
                // 使用 unordered_map 确保更具体的上下文占位符优先
                std::unordered_map<std::string, std::shared_ptr<const IPlaceholder>> tempTypedMap;
                std::vector<uint64_t> inheritedTypeIds = ctx->getInheritedTypeIds();
                // 从最不具体的上下文（父类）开始，到最具体的上下文（子类）
                // 这样，子类的同名占位符会覆盖父类的
                for (uint64_t id : inheritedTypeIds) {
                    auto tit = mTyped.find(id);
                    if (tit != mTyped.end()) {
                        for (auto& kv : tit->second) {
                            tempTypedMap.try_emplace(kv.first, kv.second.ptr);
                        }
                    }
                }

                // 处理关系型占位符
                uint64_t mainCtxId = ctx->typeId();
                auto mainIt = mRelational.find(mainCtxId);
                if (mainIt != mRelational.end()) {
                    for (uint64_t relId : inheritedTypeIds) {
                        auto relIt = mainIt->second.find(relId);
                        if (relIt != mainIt->second.end()) {
                            for (auto& kv : relIt->second) {
                                tempTypedMap.try_emplace(kv.first, kv.second.ptr);
                            }
                        }
                    }
                }

                typedList.reserve(tempTypedMap.size());
                for (auto& kv : tempTypedMap) {
                    typedList.emplace_back(kv.first, kv.second);
                }
            }
            serverList.reserve(mServer.size());
            for (auto& kv : mServer) {
                serverList.emplace_back(kv.first, kv.second.ptr);
            }
        }

        std::string result(text);

        // 先替换特定上下文
        if (ctx) {
            for (auto& kv : typedList) {
                replaceAll(result, kv.first, kv.second.get(), ctx);
            }
        }

        // 再替换服务器占位符
        for (auto& kv : serverList) {
            replaceAll(result, kv.first, kv.second.get(), nullptr);
        }

        return result;
    }

    std::string replaceServer(std::string_view text) const override {
        std::vector<std::pair<std::string, std::shared_ptr<const IPlaceholder>>> serverList;
        {
            std::lock_guard<std::mutex> lk(mMutex);
            serverList.reserve(mServer.size());
            for (auto& kv : mServer) {
                serverList.emplace_back(kv.first, kv.second.ptr);
            }
        }

        std::string result(text);
        for (auto& kv : serverList) {
            replaceAll(result, kv.first, kv.second.get(), nullptr);
        }
        return result;
    }

private:
    struct Entry {
        std::shared_ptr<const IPlaceholder> ptr{};
        void*                               owner{};
    };
    struct Handle {
        bool        isServer{};
        bool        isRelational{}; // 新增字段，标识是否为关系型占位符
        uint64_t    mainCtxId{};    // 主上下文 ID
        uint64_t    relCtxId{};     // 关系上下文 ID
        uint64_t    ctxId{};        // 普通占位符的上下文 ID
        std::string token;
    };

    // 修改后的 replaceAll 函数，支持参数和颜色
    static void replaceAll(std::string& text, const std::string& token, const IPlaceholder* ph, const IContext* ctx) {
        if (!ph) return;

        // 提取原始 token 的内部部分，例如 "{player_name}" -> "player_name"
        std::string_view innerToken;
        if (token.length() > 2 && token.front() == '{' && token.back() == '}') {
            innerToken = std::string_view(token).substr(1, token.length() - 2);
        } else {
            innerToken = token; // Fallback for non-{...} tokens
        }

        size_t pos = 0;
        // 查找 "{innerToken}" 或 "{innerToken:param}" 形式的占位符
        std::string searchPrefix = "{" + std::string(innerToken);

        while ((pos = text.find(searchPrefix, pos)) != std::string::npos) {
            size_t endPos = text.find('}', pos);
            if (endPos == std::string::npos) {
                // 占位符格式错误，跳过
                pos += searchPrefix.length();
                continue;
            }

            std::string_view fullMatch = std::string_view(text).substr(pos, endPos - pos + 1);
            std::string_view contentInsideBraces = std::string_view(text).substr(pos + 1, endPos - (pos + 1));

            std::string_view actualTokenPart;
            std::string_view paramPart;

            size_t colonPos = contentInsideBraces.find(':');
            if (colonPos != std::string::npos) {
                actualTokenPart = contentInsideBraces.substr(0, colonPos);
                paramPart       = contentInsideBraces.substr(colonPos + 1);
            } else {
                actualTokenPart = contentInsideBraces;
                paramPart       = "";
            }

            // 确保匹配到的 token 部分与我们注册的 innerToken 相同
            if (actualTokenPart == innerToken) {
                std::string evaluatedValue;
                if (!paramPart.empty()) {
                    ph->evaluateWithParam(ctx, paramPart, evaluatedValue);
                } else {
                    ph->evaluate(ctx, evaluatedValue);
                }

                // 尝试将 evaluatedValue 转换为数字
                double value;
                bool isNumeric = false;
                auto [ptr, ec] = std::from_chars(evaluatedValue.data(), evaluatedValue.data() + evaluatedValue.size(), value);
                if (ec == std::errc()) {
                    isNumeric = true;
                }

                int precision = -1; // 默认不设置小数位数
                std::string colorParamPart;

                if (!paramPart.empty()) {
                    std::string currentParam(paramPart);
                    std::vector<std::string> params;
                    size_t start = 0;
                    size_t end = currentParam.find(',');
                    while (end != std::string::npos) {
                        params.push_back(currentParam.substr(start, end - start));
                        start = end + 1;
                        end = currentParam.find(',', start);
                    }
                    params.push_back(currentParam.substr(start));

                    std::vector<std::string> remainingParams;
                    for (const auto& p : params) {
                        if (p.rfind("precision=", 0) == 0) { // 检查是否以 "precision=" 开头
                            std::string_view precision_sv = std::string_view(p).substr(10); // "precision=".length()
                            int parsedPrecision;
                            auto [prec_ptr, prec_ec] = std::from_chars(precision_sv.data(), precision_sv.data() + precision_sv.size(), parsedPrecision);
                            if (prec_ec == std::errc()) {
                                precision = parsedPrecision;
                            }
                        } else {
                            remainingParams.push_back(p);
                        }
                    }

                    // 重新组合剩余的参数作为颜色参数
                    if (!remainingParams.empty()) {
                        std::stringstream ss;
                        for (size_t i = 0; i < remainingParams.size(); ++i) {
                            ss << remainingParams[i];
                            if (i < remainingParams.size() - 1) {
                                ss << ",";
                            }
                        }
                        colorParamPart = ss.str();
                    }
                }

                // 如果是数字且设置了小数位数，则格式化
                if (isNumeric && precision != -1) {
                    std::stringstream ss;
                    ss << std::fixed << std::setprecision(precision) << value;
                    evaluatedValue = ss.str();
                }

                // 应用颜色规则
                if (!colorParamPart.empty()) {
                    std::string currentParam(colorParamPart);
                    std::vector<std::string> params;
                    size_t start = 0;
                    size_t end = currentParam.find(',');
                    while (end != std::string::npos) {
                        params.push_back(currentParam.substr(start, end - start));
                        start = end + 1;
                        end = currentParam.find(',', start);
                    }
                    params.push_back(currentParam.substr(start));

                    if (params.size() == 1) {
                        // 如果只有一个参数，直接作为颜色代码
                        evaluatedValue = params[0] + evaluatedValue + PA_COLOR_RESET;
                    } else if (params.size() >= 3 && params.size() % 2 == 1 && isNumeric) {
                        // 格式: {value,color_low,value,color_mid,default_color}
                        // 例如: {my_value:100,§c,300,§e,§a}
                        std::string appliedColor = params.back(); // 默认颜色
                        for (size_t i = 0; i < params.size() - 1; i += 2) {
                            double threshold;
                            std::string_view threshold_sv = params[i];
                            auto [t_ptr, t_ec] = std::from_chars(threshold_sv.data(), threshold_sv.data() + threshold_sv.size(), threshold);
                            if (t_ec == std::errc()) {
                                if (value < threshold) {
                                    appliedColor = params[i+1];
                                    break;
                                }
                            }
                        }
                        evaluatedValue = appliedColor + evaluatedValue + PA_COLOR_RESET;
                    }
                }
                // 如果转换失败或参数格式不匹配，则不应用颜色，保持原样

                text.replace(pos, fullMatch.length(), evaluatedValue);
                pos += evaluatedValue.length(); // 继续从替换后的位置开始查找
            } else {
                // 如果实际 token 部分不匹配，则跳过此占位符
                pos = endPos + 1;
            }
        }
    }

    // ctxId -> token -> Entry
    mutable std::mutex                                                               mMutex;
    std::unordered_map<uint64_t, std::unordered_map<std::string, Entry>>             mTyped;
    std::unordered_map<uint64_t, std::unordered_map<uint64_t, std::unordered_map<std::string, Entry>>> mRelational; // mainCtxId -> relCtxId -> token -> Entry
    std::unordered_map<std::string, Entry>                                           mServer;
    std::unordered_map<void*, std::vector<Handle>>                                   mOwnerIndex;
};

static PlaceholderManager gManager;

extern "C" PA_API IPlaceholderService* PA_GetPlaceholderService() { return &gManager; }

} // namespace PA
