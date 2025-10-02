// PlaceholderManager.cpp
#define PA_BUILD
#include "PA/PlaceholderAPI.h"

#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>


namespace PA {

class PlaceholderManager final : public IPlaceholderService {
public:
    void registerPlaceholder(std::string_view prefix, const IPlaceholder* p, void* owner) override {
        if (!p) return;

        std::string      key;
        std::string_view token_sv = p->token();

        if (prefix.empty()) {
            key = token_sv;
        } else {
            if (token_sv.length() > 2 && token_sv.front() == '{' && token_sv.back() == '}') {
                std::string_view inner_token = token_sv.substr(1, token_sv.length() - 2);
                key                          = "{" + std::string(prefix) + "_" + std::string(inner_token) + "}";
            } else {
                // Fallback for tokens not in "{...}" format, just use the original token.
                key = token_sv;
            }
        }

        std::lock_guard<std::mutex> lk(mMutex);
        const uint64_t              ctxId = p->contextTypeId();

        if (ctxId == kServerContextId) {
            mServer[key] = {p, owner};
            mOwnerIndex[owner].push_back({true, 0, key});
        } else {
            mTyped[ctxId][key] = {p, owner};
            mOwnerIndex[owner].push_back({false, ctxId, key});
        }
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
            } else {
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
        std::vector<std::pair<std::string, const IPlaceholder*>> typedList;
        std::vector<std::pair<std::string, const IPlaceholder*>> serverList;

        {
            std::lock_guard<std::mutex> lk(mMutex);
            if (ctx) {
                const uint64_t ctxId = ctx->typeId();
                auto           tit   = mTyped.find(ctxId);
                if (tit != mTyped.end()) {
                    typedList.reserve(tit->second.size());
                    for (auto& kv : tit->second) {
                        typedList.emplace_back(kv.first, kv.second.ptr);
                    }
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
                replaceAll(result, kv.first, kv.second, ctx);
            }
        }

        // 再替换服务器占位符
        for (auto& kv : serverList) {
            replaceAll(result, kv.first, kv.second, nullptr);
        }

        return result;
    }

    std::string replaceServer(std::string_view text) const override {
        std::vector<std::pair<std::string, const IPlaceholder*>> serverList;
        {
            std::lock_guard<std::mutex> lk(mMutex);
            serverList.reserve(mServer.size());
            for (auto& kv : mServer) {
                serverList.emplace_back(kv.first, kv.second.ptr);
            }
        }

        std::string result(text);
        for (auto& kv : serverList) {
            replaceAll(result, kv.first, kv.second, nullptr);
        }
        return result;
    }

private:
    struct Entry {
        const IPlaceholder* ptr{};
        void*               owner{};
    };
    struct Handle {
        bool        isServer{};
        uint64_t    ctxId{};
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
    mutable std::mutex                                                   mMutex;
    std::unordered_map<uint64_t, std::unordered_map<std::string, Entry>> mTyped;
    std::unordered_map<std::string, Entry>                               mServer;
    std::unordered_map<void*, std::vector<Handle>>                       mOwnerIndex;
};

static PlaceholderManager gManager;

extern "C" PA_API IPlaceholderService* PA_GetPlaceholderService() { return &gManager; }

} // namespace PA
