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
                     for (auto const& t : texts) outs.emplace_back(svc->replace(t, &ctx));
                     logger.debug("[PA::replaceManyForPlayer] done");
                     return outs;
                 }
          );

        // 7) replaceObject: Map 批量（服务器级）
        ok = ok
          && RemoteCall::exportAs(
                 kNamespace,
                 "replaceObject",
                 [svc](std::unordered_map<std::string, std::string> kv
                 ) -> std::unordered_map<std::string, std::string> {
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
                     for (auto const& [k, v] : kv) out.emplace(k, svc->replace(v, &ctx));
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
