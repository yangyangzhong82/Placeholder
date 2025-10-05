// PlaceholderManager.cpp
#define PA_BUILD
#include "PA/PlaceholderAPI.h"

#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>


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

    static void replaceAll(std::string& text, const std::string& token, const IPlaceholder* ph, const IContext* ctx) {
        if (!ph) return;
        size_t pos = text.find(token);
        while (pos != std::string::npos) {
            std::string out;
            ph->evaluate(ctx, out);
            text.replace(pos, token.size(), out);
            pos = text.find(token, pos + out.size());
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
