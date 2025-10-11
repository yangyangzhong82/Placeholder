// src/PA/ScriptExports.cpp
#include "PA/ScriptExports.h"

#include "PA/PlaceholderAPI.h"
#include "PA/logger.h"
#include "RemoteCallAPI.h"

#include "mc/deps/core/math/Vec3.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/BlockPos.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <fmt/core.h>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace PA {
namespace ScriptExports {

static constexpr const char* kNamespace = "PA";
static std::once_flag        gInstallOnce;
static std::atomic<bool>     gInstalled{false};

// 日志工具：避免日志中打印过长字符串
static std::string truncateForLog(std::string const& s, size_t maxLen = 256) {
    if (s.size() <= maxLen) return s;
    return s.substr(0, maxLen) + "...(" + std::to_string(s.size()) + " bytes)";
}

static std::string safeNullPtr(void const* p) {
    return p ? fmt::format("0x{:X}", reinterpret_cast<uintptr_t>(p)) : "null";
}

// ========== 新增：JS 占位符适配 ==========

namespace {

// 一个“所有权桶”，用来把同一 JS 命名空间注册的占位符归为一组，便于批量卸载
struct OwnerBucket {
    std::string key; // 一般使用 JS 回调命名空间作为 key
};

static std::mutex                                                    gOwnerMutex;
static std::unordered_map<std::string, std::unique_ptr<OwnerBucket>> gOwners;

static void* getOrCreateOwner(std::string const& key) {
    std::scoped_lock lk(gOwnerMutex);
    auto             it = gOwners.find(key);
    if (it != gOwners.end()) return it->second.get();
    auto node = std::make_unique<OwnerBucket>();
    node->key = key;
    void* ptr = node.get();
    gOwners.emplace(key, std::move(node));
    return ptr;
}

static bool unregisterByOwnerKey(std::string const& key) {
    auto* svc = PA_GetPlaceholderService();
    if (!svc) return false;

    std::unique_ptr<OwnerBucket> node;
    {
        std::scoped_lock lk(gOwnerMutex);
        auto             it = gOwners.find(key);
        if (it == gOwners.end()) return false;
        void* owner = it->second.get();
        // 卸载该 owner 下的全部占位符
        svc->unregisterByOwner(owner);
        node = std::move(it->second);
        gOwners.erase(it);
    }
    return true;
}

// 将上下文类型名转成 ID（方便 JS 传字符串）
static uint64_t parseContextKind(std::string kind) {
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

// IPlaceholder 适配器：将求值回调转发给 JS（通过 RemoteCall）
class JsPlaceholder final : public IPlaceholder {
public:
    JsPlaceholder(std::string tokenNameNoBraces, uint64_t ctxId, std::string cbNamespace, std::string cbName)
    : mTokenNoBraces(std::move(tokenNameNoBraces)),
      mTokenBraced("{" + mTokenNoBraces + "}"),
      mCtxId(ctxId),
      mCbNS(std::move(cbNamespace)),
      mCbName(std::move(cbName)) {}

    std::string_view token() const noexcept override { return mTokenBraced; }
    uint64_t         contextTypeId() const noexcept override { return mCtxId; }

    void evaluate(const IContext* ctx, std::string& out) const override { evaluateImpl(ctx, std::string_view{}, out); }

    void evaluateWithParam(const IContext* ctx, std::string_view param, std::string& out) const override {
        evaluateImpl(ctx, param, out);
    }

private:
    void evaluateImpl(const IContext* ctx, std::string_view param, std::string& out) const {
        try {
            // 将 tokenName（不带花括号）与 param 传给 JS
            if (mCtxId == kServerContextId) {
                // JS 回调签名：std::string(std::string token, std::string param)
                auto fn  = RemoteCall::importAs<std::string(std::string, std::string)>(mCbNS, mCbName);
                auto res = fn(mTokenNoBraces, std::string(param));
                out.append(res);
            } else if (mCtxId == PlayerContext::kTypeId) {
                // JS 回调签名：std::string(std::string token, std::string param, Player* player)
                auto    fn   = RemoteCall::importAs<std::string(std::string, std::string, Player*)>(mCbNS, mCbName);
                auto    pctx = static_cast<PlayerContext const*>(ctx);
                Player* p    = pctx ? pctx->player : nullptr;
                auto    res  = fn(mTokenNoBraces, std::string(param), p);
                out.append(res);
            } else if (mCtxId == MobContext::kTypeId) {
                // JS 回调签名：std::string(std::string token, std::string param, Actor* actor)
                // 注意：RemoteCall 支持 Actor*，Mob* 会被打包为 Actor* 传递，JS 侧可当作 Actor 使用
                auto   fn   = RemoteCall::importAs<std::string(std::string, std::string, Actor*)>(mCbNS, mCbName);
                auto   mctx = static_cast<MobContext const*>(ctx);
                Actor* a    = mctx ? static_cast<Actor*>(mctx->mob) : nullptr;
                auto   res  = fn(mTokenNoBraces, std::string(param), a);
                out.append(res);
            } else if (mCtxId == ActorContext::kTypeId) {
                // JS 回调签名：std::string(std::string token, std::string param, Actor* actor)
                auto   fn   = RemoteCall::importAs<std::string(std::string, std::string, Actor*)>(mCbNS, mCbName);
                auto   actx = static_cast<ActorContext const*>(ctx);
                Actor* a    = actx ? actx->actor : nullptr;
                auto   res  = fn(mTokenNoBraces, std::string(param), a);
                out.append(res);
            } else {
                // 未知上下文，回退为服务器级
                auto fn  = RemoteCall::importAs<std::string(std::string, std::string)>(mCbNS, mCbName);
                auto res = fn(mTokenNoBraces, std::string(param));
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
    std::string mTokenNoBraces;
    std::string mTokenBraced;
    uint64_t    mCtxId;
    std::string mCbNS;
    std::string mCbName;
};

static bool registerJsPlaceholder(
    std::string prefix,
    std::string tokenNameNoBraces,
    uint64_t    ctxId,
    std::string cbNamespace,
    std::string cbName
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

    auto  p     = std::make_shared<JsPlaceholder>(tokenNameNoBraces, ctxId, cbNamespace, cbName);
    void* owner = getOrCreateOwner(cbNamespace);

    svc->registerPlaceholder(prefix, p, owner);

    logger.info(
        "[PA] JS placeholder registered: prefix='{}', token='{}', ctxId={}, cb='{}::{}'",
        prefix,
        tokenNameNoBraces,
        ctxId,
        cbNamespace,
        cbName
    );
    return true;
}

} // anonymous namespace

// ========== 现有导出 ==========

void install() {
    std::call_once(gInstallOnce, [] {
        logger.info("[PA::ScriptExports] Installing script exports under namespace '{}'", kNamespace);

        auto* svc = PA_GetPlaceholderService();
        if (!svc) {
            logger.error("[PA::ScriptExports] PlaceholderService is null, cannot export APIs");
            return;
        }

        bool ok = true;

        // 1) ping: 测试连通性
        ok = ok && RemoteCall::exportAs(kNamespace, "ping", []() -> bool {
                 logger.debug("[PA::ping] called");
                 return true;
             });

        // 2) replace: 服务器级占位符替换
        ok = ok && RemoteCall::exportAs(kNamespace, "replace", [svc](std::string const& text) -> std::string {
                 logger.debug("[PA::replace] in='{}'", truncateForLog(text));
                 std::string out = svc->replaceServer(text);
                 logger.debug("[PA::replace] out='{}'", truncateForLog(out));
                 return out;
             });

        // 3) replaceForPlayer: 玩家上下文替换
        ok = ok
          && RemoteCall::exportAs(
                 kNamespace,
                 "replaceForPlayer",
                 [svc](std::string const& text, Player* player) -> std::string {
                     logger
                         .debug("[PA::replaceForPlayer] player={}, in='{}'", safeNullPtr(player), truncateForLog(text));
                     if (!player) {
                         logger.warn("[PA::replaceForPlayer] player is null, fallback to server replace");
                         auto out = svc->replaceServer(text);
                         logger.debug("[PA::replaceForPlayer] out='{}'", truncateForLog(out));
                         return out;
                     }
                     PlayerContext ctx;
                     ctx.player = player;
                     ctx.mob    = player;
                     ctx.actor  = player;
                     auto out   = svc->replace(text, &ctx);
                     logger.debug("[PA::replaceForPlayer] out='{}'", truncateForLog(out));
                     return out;
                 }
          );

        // 4) replaceForActor: Actor 上下文替换（非玩家实体）
        ok = ok
          && RemoteCall::exportAs(
                 kNamespace,
                 "replaceForActor",
                 [svc](std::string const& text, Actor* actor) -> std::string {
                     logger.debug("[PA::replaceForActor] actor={}, in='{}'", safeNullPtr(actor), truncateForLog(text));
                     if (!actor) {
                         logger.warn("[PA::replaceForActor] actor is null, fallback to server replace");
                         auto out = svc->replaceServer(text);
                         logger.debug("[PA::replaceForActor] out='{}'", truncateForLog(out));
                         return out;
                     }
                     ActorContext ctx;
                     ctx.actor = actor;
                     auto out  = svc->replace(text, &ctx);
                     logger.debug("[PA::replaceForActor] out='{}'", truncateForLog(out));
                     return out;
                 }
          );

        // 5) replaceMany: 数组批量（服务器级）
        ok = ok
          && RemoteCall::exportAs(
                 kNamespace,
                 "replaceMany",
                 [svc](std::vector<std::string> texts) -> std::vector<std::string> {
                     logger.debug("[PA::replaceMany] count={}", texts.size());
                     std::vector<std::string> outs;
                     outs.reserve(texts.size());
                     for (auto const& t : texts) outs.emplace_back(svc->replaceServer(t));
                     logger.debug("[PA::replaceMany] done");
                     return outs;
                 }
          );

        // 6) replaceManyForPlayer: 数组批量（玩家上下文）
        ok = ok
          && RemoteCall::exportAs(
                 kNamespace,
                 "replaceManyForPlayer",
                 [svc](std::vector<std::string> texts, Player* player) -> std::vector<std::string> {
                     logger.debug("[PA::replaceManyForPlayer] count={}, player={}", texts.size(), safeNullPtr(player));
                     std::vector<std::string> outs;
                     outs.reserve(texts.size());
                     if (!player) {
                         logger.warn("[PA::replaceManyForPlayer] player is null, fallback to server replace");
                         for (auto const& t : texts) outs.emplace_back(svc->replaceServer(t));
                         return outs;
                     }
                     PlayerContext ctx;
                     ctx.player = player;
                     ctx.mob    = player;
                     ctx.actor  = player;
                     for (auto const& t : texts) {
                         outs.emplace_back(svc->replace(t, &ctx));
                     }
                     logger.debug("[PA::replaceManyForPlayer] done");
                     return outs;
                 }
          );

        // 7) replaceObject: Map 批量（服务器级）
        ok =
            ok
            && RemoteCall::exportAs(
                kNamespace,
                "replaceObject",
                [svc](std::unordered_map<std::string, std::string> kv) -> std::unordered_map<std::string, std::string> {
                    logger.debug("[PA::replaceObject] size={}", kv.size());
                    std::unordered_map<std::string, std::string> out;
                    out.reserve(kv.size());
                    for (auto const& [k, v] : kv) {
                        out.emplace(k, svc->replaceServer(v));
                    }
                    logger.debug("[PA::replaceObject] done");
                    return out;
                }
            );

        // 8) replaceObjectForPlayer: Map 批量（玩家上下文）
        ok = ok
          && RemoteCall::exportAs(
                 kNamespace,
                 "replaceObjectForPlayer",
                 [svc](std::unordered_map<std::string, std::string> kv, Player* player)
                     -> std::unordered_map<std::string, std::string> {
                     logger.debug("[PA::replaceObjectForPlayer] size={}, player={}", kv.size(), safeNullPtr(player));
                     std::unordered_map<std::string, std::string> out;
                     out.reserve(kv.size());
                     if (!player) {
                         logger.warn("[PA::replaceObjectForPlayer] player is null, fallback to server replace");
                         for (auto const& [k, v] : kv) out.emplace(k, svc->replaceServer(v));
                         return out;
                     }
                     PlayerContext ctx;
                     ctx.player = player;
                     ctx.mob    = player;
                     ctx.actor  = player;
                     for (auto const& [k, v] : kv) {
                         out.emplace(k, svc->replace(v, &ctx));
                     }
                     logger.debug("[PA::replaceObjectForPlayer] done");
                     return out;
                 }
          );

        // 9) debugWorldPos: 示例（传递世界坐标 RemoteCall::WorldPosType）
        ok = ok && RemoteCall::exportAs(kNamespace, "debugWorldPos", [](RemoteCall::WorldPosType pos) -> std::string {
                 auto [vec, dim] = pos.get<std::pair<Vec3, int>>();
                 std::string s =
                     fmt::format("WorldPos: x={:.3f}, y={:.3f}, z={:.3f}, dim={}", vec.x, vec.y, vec.z, dim);
                 logger.debug("[PA::debugWorldPos] {}", s);
                 return s;
             });

        // 10) debugBlockPos: 示例（传递方块坐标 RemoteCall::BlockPosType）
        ok = ok && RemoteCall::exportAs(kNamespace, "debugBlockPos", [](RemoteCall::BlockPosType pos) -> std::string {
                 auto [bp, dim] = pos.get<std::pair<BlockPos, int>>();
                 std::string s  = fmt::format("BlockPos: x={}, y={}, z={}, dim={}", bp.x, bp.y, bp.z, dim);
                 logger.debug("[PA::debugBlockPos] {}", s);
                 return s;
             });

        // ========== 新增导出：JS 注册占位符 API ==========

        // A) 直接按上下文 ID 注册
        ok = ok
          && RemoteCall::exportAs(
                 kNamespace,
                 "registerPlaceholderByContextId",
                 [](std::string prefix,
                    std::string tokenName,
                    std::string ctxTypeIdStr, // Use string for JS compatibility
                    std::string cbNS,
                    std::string cbName) -> bool {
                     uint64_t ctxTypeId = 0;
                     try {
                         ctxTypeId = std::stoull(ctxTypeIdStr);
                     } catch (...) {
                         logger.error("[PA] Invalid ctxTypeId string: {}", ctxTypeIdStr);
                         return false;
                     }
                     return registerJsPlaceholder(
                         std::move(prefix),
                         std::move(tokenName),
                         ctxTypeId,
                         std::move(cbNS),
                         std::move(cbName)
                     );
                 }
          );

        // B) 按上下文名字注册（"server" | "actor" | "mob" | "player"）
        ok = ok
          && RemoteCall::exportAs(
                 kNamespace,
                 "registerPlaceholderByKind",
                 [](std::string prefix, std::string tokenName, std::string ctxKind, std::string cbNS, std::string cbName
                 ) -> bool {
                     uint64_t id = parseContextKind(ctxKind);
                     return registerJsPlaceholder(
                         std::move(prefix),
                         std::move(tokenName),
                         id,
                         std::move(cbNS),
                         std::move(cbName)
                     );
                 }
          );

        // C) 便捷函数：固定上下文
        ok = ok
          && RemoteCall::exportAs(
                 kNamespace,
                 "registerServerPlaceholder",
                 [](std::string prefix, std::string tokenName, std::string cbNS, std::string cbName) -> bool {
                     return registerJsPlaceholder(
                         std::move(prefix),
                         std::move(tokenName),
                         kServerContextId,
                         std::move(cbNS),
                         std::move(cbName)
                     );
                 }
          );
        ok = ok
          && RemoteCall::exportAs(
                 kNamespace,
                 "registerActorPlaceholder",
                 [](std::string prefix, std::string tokenName, std::string cbNS, std::string cbName) -> bool {
                     return registerJsPlaceholder(
                         std::move(prefix),
                         std::move(tokenName),
                         ActorContext::kTypeId,
                         std::move(cbNS),
                         std::move(cbName)
                     );
                 }
          );
        ok = ok
          && RemoteCall::exportAs(
                 kNamespace,
                 "registerMobPlaceholder",
                 [](std::string prefix, std::string tokenName, std::string cbNS, std::string cbName) -> bool {
                     return registerJsPlaceholder(
                         std::move(prefix),
                         std::move(tokenName),
                         MobContext::kTypeId,
                         std::move(cbNS),
                         std::move(cbName)
                     );
                 }
          );
        ok = ok
          && RemoteCall::exportAs(
                 kNamespace,
                 "registerPlayerPlaceholder",
                 [](std::string prefix, std::string tokenName, std::string cbNS, std::string cbName) -> bool {
                     return registerJsPlaceholder(
                         std::move(prefix),
                         std::move(tokenName),
                         PlayerContext::kTypeId,
                         std::move(cbNS),
                         std::move(cbName)
                     );
                 }
          );

        // D) 卸载：按回调命名空间（即注册时传入的 cbNamespace）批量卸载
        ok = ok && RemoteCall::exportAs(kNamespace, "unregisterByCallbackNamespace", [](std::string cbNS) -> bool {
                 bool removed = unregisterByOwnerKey(cbNS);
                 logger.info("[PA] Unregister by callback namespace '{}' -> {}", cbNS, removed);
                 return removed;
             });

        // E) 透出上下文类型 ID，便于 JS 选择使用
        ok = ok
          && RemoteCall::exportAs(
                 kNamespace,
                 "contextTypeIds",
                 []() -> std::unordered_map<std::string, std::string> {
                     return {
                         {"server", std::to_string(kServerContextId)      },
                         {"actor",  std::to_string(ActorContext::kTypeId) },
                         {"mob",    std::to_string(MobContext::kTypeId)   },
                         {"player", std::to_string(PlayerContext::kTypeId)}
                     };
                 }
          );

        if (!ok) {
            logger.error("[PA::ScriptExports] Some exports failed, please check earlier logs.");
        } else {
            logger.info("[PA::ScriptExports] All exports registered successfully under '{}'", kNamespace);
            gInstalled.store(true, std::memory_order_release);
        }
    });
}

void uninstall() {
    if (!gInstalled.load(std::memory_order_acquire)) return;
    int removed = RemoteCall::removeNameSpace(kNamespace);
    logger.info("[PA::ScriptExports] Uninstalled namespace '{}', removed {} exported functions", kNamespace, removed);
    gInstalled.store(false, std::memory_order_release);
}

} // namespace ScriptExports
} // namespace PA
