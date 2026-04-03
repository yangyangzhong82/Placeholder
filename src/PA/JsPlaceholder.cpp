// src/PA/JsPlaceholder.cpp
#include "PA/JsPlaceholder.h"

#include "PA/PlaceholderAPI.h"
#include "PA/logger.h"
#include "RemoteCallAPI.h"

#include "mc/world/actor/Actor.h"
#include "mc/world/actor/player/Player.h"

#include <algorithm>
#include <cctype>
#include <fmt/core.h>

namespace PA {

// ========== OwnerBucket 管理 ==========

static std::mutex                                                    gOwnerMutex;
static std::unordered_map<std::string, std::unique_ptr<OwnerBucket>> gOwners;

void* getOrCreateOwner(std::string const& key) {
    std::scoped_lock lk(gOwnerMutex);
    auto             it = gOwners.find(key);
    if (it != gOwners.end()) return it->second.get();
    auto node = std::make_unique<OwnerBucket>();
    node->key = key;
    void* ptr = node.get();
    gOwners.emplace(key, std::move(node));
    return ptr;
}

bool unregisterByOwnerKey(std::string const& key) {
    auto* svc = PA_GetPlaceholderService();
    if (!svc) return false;

    std::unique_ptr<OwnerBucket> node;
    {
        std::scoped_lock lk(gOwnerMutex);
        auto             it = gOwners.find(key);
        if (it == gOwners.end()) return false;
        void* owner = it->second.get();
        svc->unregisterByOwner(owner);
        node = std::move(it->second);
        gOwners.erase(it);
    }
    return true;
}

// ========== 上下文类型解析 ==========

uint64_t parseContextKind(std::string kind) {
    auto toLower = [](std::string& s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    };
    toLower(kind);
    if (kind == "server" || kind == "srv" || kind == "none") return kServerContextId;
    if (kind == "actor") return ActorContext::kTypeId;
    if (kind == "mob") return MobContext::kTypeId;
    if (kind == "player" || kind == "pl" || kind == "p") return PlayerContext::kTypeId;
    return kServerContextId;
}

// ========== JsPlaceholder 类 ==========

class JsPlaceholder final : public IPlaceholder {
public:
    JsPlaceholder(
        std::string  tokenNameNoBraces,
        uint64_t     ctxId,
        std::string  cbNamespace,
        std::string  cbName,
        unsigned int cacheDuration = 0
    )
    : mTokenNoBraces(std::move(tokenNameNoBraces)),
      mTokenBraced("{" + mTokenNoBraces + "}"),
      mCtxId(ctxId),
      mCbNS(std::move(cbNamespace)),
      mCbName(std::move(cbName)),
      mCacheDuration(cacheDuration) {}

    std::string_view token() const noexcept override { return mTokenBraced; }
    uint64_t         contextTypeId() const noexcept override { return mCtxId; }
    unsigned int     getCacheDuration() const noexcept override { return mCacheDuration; }

    void evaluate(const IContext* ctx, std::string& out) const override {
        evaluateWithArgs(ctx, {}, out);
    }

    void evaluateWithArgs(
        const IContext*                      ctx,
        const std::vector<std::string_view>& args,
        std::string&                         out
    ) const override {
        try {
            std::vector<std::string> args_str;
            args_str.reserve(args.size());
            for (const auto& sv : args) {
                args_str.emplace_back(sv);
            }

            if (mCtxId == kServerContextId) {
                auto fn  = RemoteCall::importAs<std::string(std::string, std::vector<std::string>)>(mCbNS, mCbName);
                auto res = fn(mTokenNoBraces, args_str);
                out.append(res);
            } else if (mCtxId == PlayerContext::kTypeId) {
                auto fn =
                    RemoteCall::importAs<std::string(std::string, std::vector<std::string>, Player*)>(mCbNS, mCbName);
                auto    pctx = static_cast<PlayerContext const*>(ctx);
                Player* p    = pctx ? pctx->player : nullptr;
                auto    res  = fn(mTokenNoBraces, args_str, p);
                out.append(res);
            } else if (mCtxId == MobContext::kTypeId) {
                auto fn =
                    RemoteCall::importAs<std::string(std::string, std::vector<std::string>, Actor*)>(mCbNS, mCbName);
                auto   mctx = static_cast<MobContext const*>(ctx);
                Actor* a    = mctx ? static_cast<Actor*>(mctx->mob) : nullptr;
                auto   res  = fn(mTokenNoBraces, args_str, a);
                out.append(res);
            } else if (mCtxId == ActorContext::kTypeId) {
                auto fn =
                    RemoteCall::importAs<std::string(std::string, std::vector<std::string>, Actor*)>(mCbNS, mCbName);
                auto   actx = static_cast<ActorContext const*>(ctx);
                Actor* a    = actx ? actx->actor : nullptr;
                auto   res  = fn(mTokenNoBraces, args_str, a);
                out.append(res);
            } else {
                auto fn  = RemoteCall::importAs<std::string(std::string, std::vector<std::string>)>(mCbNS, mCbName);
                auto res = fn(mTokenNoBraces, args_str);
                out.append(res);
            }
        } catch (std::exception const& e) {
            logger.error(
                "[PA::JsPlaceholder] evaluate error for token '{}', cb='{}::{}': {}",
                mTokenBraced,
                mCbNS,
                mCbName,
                e.what()
            );
        }
    }

private:
    std::string  mTokenNoBraces;
    std::string  mTokenBraced;
    uint64_t     mCtxId;
    std::string  mCbNS;
    std::string  mCbName;
    unsigned int mCacheDuration;
};

// ========== 注册辅助函数 ==========

bool registerJsPlaceholder(
    std::string  prefix,
    std::string  tokenNameNoBraces,
    uint64_t     ctxId,
    std::string  cbNamespace,
    std::string  cbName,
    unsigned int cacheDuration
) {
    auto* svc = PA_GetPlaceholderService();
    if (!svc) {
        logger.error("[PA::registerJsPlaceholder] PlaceholderService is null");
        return false;
    }
    if (tokenNameNoBraces.empty()) {
        logger.warn("[PA::registerJsPlaceholder] tokenName is empty");
        return false;
    }

    auto  p     = std::make_shared<JsPlaceholder>(tokenNameNoBraces, ctxId, cbNamespace, cbName, cacheDuration);
    void* owner = getOrCreateOwner(cbNamespace);

    svc->registerPlaceholder(prefix, p, owner);

    logger.info(
        "[PA] JS placeholder registered: prefix='{}', token='{}', ctxId={}, cb='{}::{}', cacheDuration={}",
        prefix,
        tokenNameNoBraces,
        ctxId,
        cbNamespace,
        cbName,
        cacheDuration
    );
    return true;
}

bool registerJsCachedPlaceholder(
    std::string  prefix,
    std::string  tokenNameNoBraces,
    uint64_t     ctxId,
    std::string  cbNamespace,
    std::string  cbName,
    unsigned int cacheDuration
) {
    if (cacheDuration == 0) {
        logger.warn("[PA::registerJsCachedPlaceholder] cacheDuration is 0, registering as non-cached placeholder.");
    }
    return registerJsPlaceholder(
        std::move(prefix),
        std::move(tokenNameNoBraces),
        ctxId,
        std::move(cbNamespace),
        std::move(cbName),
        cacheDuration
    );
}

} // namespace PA
