


#include "PlaceholderAPI.h"
#include "ll/api/service/Bedrock.h"
#include "ll/api/Versions.h" // 引入 Versions.h 以获取服务器版本信息
#include "mc/deps/core/platform/BuildPlatform.h"
#include "mc/deps/ecs/gamerefs_entity/EntityContext.h"
#include "mc/deps/ecs/gamerefs_entity/EntityRegistry.h" // 提供 EntityRegistry 的完整定义
#include "mc/deps/ecs/gamerefs_entity/GameRefsEntity.h" // 确保 EntityContext 尽早被完全定义
#include "mc/deps/game_refs/GameRefs.h"
#include "mc/deps/game_refs/OwnerPtr.h"
#include "mc/network/ServerNetworkHandler.h"
#include "mc/server/ServerPlayer.h"
#include "mc/server/commands/CommandUtils.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/Mob.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/actor/provider/ActorAttribute.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/dimension/Dimension.h"


#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <fstream>
#include <sys/resource.h>
#include <sys/times.h>
#include <unistd.h>
#endif

namespace {

// 泛型占位符实现（上下文型）
template <typename Ctx, typename Fn>
class TypedLambdaPlaceholder final : public PA::IPlaceholder {
public:
    TypedLambdaPlaceholder(std::string token, Fn fn) : token_(std::move(token)), fn_(std::move(fn)) {}

    std::string_view token() const noexcept override { return token_; }
    uint64_t         contextTypeId() const noexcept override { return Ctx::kTypeId; }

    void evaluate(const PA::IContext* ctx, std::string& out) const override {
        const auto* c = static_cast<const Ctx*>(ctx);
        if constexpr (std::is_invocable_v<Fn, const Ctx&, std::string&>) {
            fn_(*c, out);
        } else {
            // This placeholder expects arguments, but none were provided.
            // Call it with an empty vector of arguments.
            fn_(*c, {}, out);
        }
    }

    void evaluateWithArgs(
        const PA::IContext*                           ctx,
        const std::vector<std::string_view>&          args,
        std::string&                                  out
    ) const override {
        const auto* c = static_cast<const Ctx*>(ctx);
        if constexpr (std::is_invocable_v<Fn, const Ctx&, const std::vector<std::string_view>&, std::string&>) {
            fn_(*c, args, out);
        } else {
            // This placeholder doesn't accept arguments, call the non-arg version.
            fn_(*c, out);
        }
    }

private:
    std::string token_;
    Fn          fn_;
};

// 服务器占位符实现（无上下文）
template <typename Fn>
class ServerLambdaPlaceholder final : public PA::IPlaceholder {
public:
    ServerLambdaPlaceholder(std::string token, Fn fn) : token_(std::move(token)), fn_(std::move(fn)) {}

    std::string_view token() const noexcept override { return token_; }
    uint64_t         contextTypeId() const noexcept override { return PA::kServerContextId; }

    void evaluate(const PA::IContext*, std::string& out) const override {
        if constexpr (std::is_invocable_v<Fn, std::string&>) {
            fn_(out);
        } else {
            // This placeholder expects arguments, but none were provided.
            // Call it with an empty vector of arguments.
            fn_({}, out);
        }
    }

    void evaluateWithArgs(
        const PA::IContext*                           ctx,
        const std::vector<std::string_view>&          args,
        std::string&                                  out
    ) const override {
        if constexpr (std::is_invocable_v<Fn, const std::vector<std::string_view>&, std::string&>) {
            fn_(args, out);
        } else {
            // This placeholder doesn't accept arguments, call the non-arg version.
            fn_(out);
        }
    }

private:
    std::string token_;
    Fn          fn_;
};

// time 工具
inline std::tm local_tm(std::time_t t) {
    std::tm buf{};
#ifdef _WIN32
    localtime_s(&buf, &t);
#else
    localtime_r(&t, &buf);
#endif
    return buf;
}

// System resource utils
#ifdef _WIN32
inline double getMemoryUsage() {
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        return static_cast<double>(pmc.PrivateUsage) / (1024 * 1024); // MB
    }
    return 0.0;
}

static ULARGE_INTEGER lastCPU, lastSysCPU, lastUserCPU;
static int            numProcessors = -1;
static HANDLE         self;

void initCpuUsage() {
    SYSTEM_INFO sysInfo;
    FILETIME    ftime, fsys, fuser;

    GetSystemInfo(&sysInfo);
    numProcessors = sysInfo.dwNumberOfProcessors;

    GetSystemTimeAsFileTime(&ftime);
    memcpy(&lastCPU, &ftime, sizeof(FILETIME));

    self = GetCurrentProcess();
    GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
    memcpy(&lastSysCPU, &fsys, sizeof(FILETIME));
    memcpy(&lastUserCPU, &fuser, sizeof(FILETIME));
}

inline double getCpuUsage() {
    if (numProcessors == -1) {
        initCpuUsage();
    }

    FILETIME       ftime, fsys, fuser;
    ULARGE_INTEGER now, sys, user;
    double         percent;

    GetSystemTimeAsFileTime(&ftime);
    memcpy(&now, &ftime, sizeof(FILETIME));

    GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
    memcpy(&sys, &fsys, sizeof(FILETIME));
    memcpy(&user, &fuser, sizeof(FILETIME));

    if (now.QuadPart - lastCPU.QuadPart == 0) {
        return 0.0;
    }

    percent = (double)((sys.QuadPart - lastSysCPU.QuadPart) + (user.QuadPart - lastUserCPU.QuadPart));
    percent /= (double)(now.QuadPart - lastCPU.QuadPart);
    percent /= numProcessors;

    lastCPU     = now;
    lastUserCPU = user;
    lastSysCPU  = sys;

    return percent * 100;
}

#else
inline double getMemoryUsage() {
    std::ifstream statm("/proc/self/statm");
    if (statm.is_open()) {
        long size = 0;
        statm >> size; // resident set size in pages
        return static_cast<double>(size) * sysconf(_SC_PAGESIZE) / (1024 * 1024); // MB
    }
    return 0.0;
}

static clock_t lastCPU, lastSysCPU, lastUserCPU;
static int     numProcessors = -1;

void initCpuUsage() {
    FILE*      file;
    struct tms timeSample;
    char       line[128];

    lastCPU     = times(&timeSample);
    lastSysCPU  = timeSample.tms_stime;
    lastUserCPU = timeSample.tms_utime;

    file          = fopen("/proc/cpuinfo", "r");
    numProcessors = 0;
    if (file) {
        while (fgets(line, 128, file) != NULL) {
            if (strncmp(line, "processor", 9) == 0) numProcessors++;
        }
        fclose(file);
    }
    if (numProcessors == 0) numProcessors = 1;
}

inline double getCpuUsage() {
    if (numProcessors == -1) {
        initCpuUsage();
    }

    struct tms timeSample;
    clock_t    now;
    double     percent;

    now = times(&timeSample);
    if (now <= lastCPU || timeSample.tms_stime < lastSysCPU || timeSample.tms_utime < lastUserCPU) {
        // Overflow detection.
        percent = 0.0;
    } else {
        percent = (timeSample.tms_stime - lastSysCPU) + (timeSample.tms_utime - lastUserCPU);
        percent /= (now - lastCPU);
        percent /= numProcessors;
    }

    lastCPU     = now;
    lastSysCPU  = timeSample.tms_stime;
    lastUserCPU = timeSample.tms_utime;

    return percent * 100;
}
#endif

#ifdef _WIN32
inline double getTotalMemory() {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        return static_cast<double>(memInfo.ullTotalPhys) / (1024 * 1024); // MB
    }
    return 0.0;
}

inline double getUsedMemory() {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        return static_cast<double>(memInfo.ullTotalPhys - memInfo.ullAvailPhys) / (1024 * 1024); // MB
    }
    return 0.0;
}

static ULARGE_INTEGER lastSystemIdle, lastSystemKernel, lastSystemUser;

void initSystemCpuUsage() {
    FILETIME ft_idle, ft_kernel, ft_user;
    if (GetSystemTimes(&ft_idle, &ft_kernel, &ft_user)) {
        memcpy(&lastSystemIdle, &ft_idle, sizeof(FILETIME));
        memcpy(&lastSystemKernel, &ft_kernel, sizeof(FILETIME));
        memcpy(&lastSystemUser, &ft_user, sizeof(FILETIME));
    }
}

inline double getSystemCpuUsage() {
    static bool initialized = false;
    if (!initialized) {
        initSystemCpuUsage();
        initialized = true;
    }

    FILETIME       ft_idle, ft_kernel, ft_user;
    ULARGE_INTEGER idle, kernel, user;
    double         percent = 0.0;

    if (GetSystemTimes(&ft_idle, &ft_kernel, &ft_user)) {
        memcpy(&idle, &ft_idle, sizeof(FILETIME));
        memcpy(&kernel, &ft_kernel, sizeof(FILETIME));
        memcpy(&user, &ft_user, sizeof(FILETIME));

        ULONGLONG idleDiff   = idle.QuadPart - lastSystemIdle.QuadPart;
        ULONGLONG kernelDiff = kernel.QuadPart - lastSystemKernel.QuadPart;
        ULONGLONG userDiff   = user.QuadPart - lastSystemUser.QuadPart;

        ULONGLONG totalSystemTime = kernelDiff + userDiff;
        ULONGLONG totalTime       = totalSystemTime + idleDiff;

        if (totalTime > 0) {
            percent = (double)totalSystemTime / totalTime * 100.0;
        }

        lastSystemIdle   = idle;
        lastSystemKernel = kernel;
        lastSystemUser   = user;
    }

    return percent;
}

#else

long parseMemInfo(const std::string& token) {
    std::ifstream file("/proc/meminfo");
    std::string   line;
    long          value = 0;
    if (file.is_open()) {
        while (std::getline(file, line)) {
            if (line.rfind(token, 0) == 0) {
                std::istringstream iss(line);
                std::string        key;
                iss >> key >> value;
                break;
            }
        }
    }
    return value;
}

inline double getTotalMemory() {
    return static_cast<double>(parseMemInfo("MemTotal:")) / 1024; // MB
}

inline double getUsedMemory() {
    long total     = parseMemInfo("MemTotal:");
    long available = parseMemInfo("MemAvailable:");
    if (available == 0) { // Fallback for older kernels
        long free    = parseMemInfo("MemFree:");
        long buffers = parseMemInfo("Buffers:");
        long cached  = parseMemInfo("Cached:");
        available    = free + buffers + cached;
    }
    return static_cast<double>(total - available) / 1024; // MB
}

static unsigned long long previousTotalTicks = 0;
static unsigned long long previousIdleTicks  = 0;

inline double getSystemCpuUsage() {
    double      percent = 0.0;
    std::ifstream file("/proc/stat");
    if (file.is_open()) {
        std::string line;
        std::getline(file, line);
        std::istringstream iss(line);
        std::string        cpu;
        iss >> cpu;

        unsigned long long user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice;
        iss >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal >> guest >> guest_nice;

        unsigned long long totalTicks = user + nice + system + idle + iowait + irq + softirq + steal;
        unsigned long long idleTicks  = idle + iowait;

        if (previousTotalTicks > 0) {
            unsigned long long totalPeriod = totalTicks - previousTotalTicks;
            unsigned long long idlePeriod  = idleTicks - previousIdleTicks;
            if (totalPeriod > 0) {
                percent = (1.0 - (double)idlePeriod / totalPeriod) * 100.0;
            }
        }

        previousTotalTicks = totalTicks;
        previousIdleTicks  = idleTicks;
    }
    return percent;
}
#endif


} // anonymous namespace

namespace PA {

// 注册内置占位符
// 注意：owner 指针用于跨模块卸载时反注册。建议使用模块内唯一地址作为 owner。
void registerBuiltinPlaceholders(IPlaceholderService* svc) {
    static int kBuiltinOwnerTag = 0;
    void*      owner            = &kBuiltinOwnerTag;

    // {player_money}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<
            PlayerContext,
            void (*)(const PlayerContext&, const std::vector<std::string_view>&, std::string&)>>(
            "{player_money}",
            +[](const PlayerContext& c, const std::vector<std::string_view>& args, std::string& out) {
                if (c.player && !args.empty()) {
                    std::string money_type(args[0]);
                    // Here you would typically call your economy API
                    // For demonstration, we'll just return a dummy value
                    if (money_type == "gold") {
                        out = "100";
                    } else if (money_type == "silver") {
                        out = "500";
                    } else {
                        out = "0";
                    }
                } else {
                    out = "0";
                }
            }
        ),
        owner
    );

    // {player_name}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<PlayerContext, void (*)(const PlayerContext&, std::string&)>>(
            "{player_name}",
            +[](const PlayerContext& c, std::string& out) {
                out.clear();
                if (c.player) out = c.player->getRealName();
            }
        ),
        owner
    );

    // {player_average_ping}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<PlayerContext, void (*)(const PlayerContext&, std::string&)>>(
            "{player_average_ping}",
            +[](const PlayerContext& c, std::string& out) {
                out = "0";
                if (c.player) {
                    if (auto ns = c.player->getNetworkStatus()) {
                        out = std::to_string(ns->mAveragePing);
                    }
                }
            }
        ),
        owner
    );
    // {player_ping}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<PlayerContext, void (*)(const PlayerContext&, std::string&)>>(
            "{player_ping}",
            +[](const PlayerContext& c, std::string& out) {
                out = "0";
                if (c.player) {
                    if (auto ns = c.player->getNetworkStatus()) {
                        out = std::to_string(ns->mCurrentPing);
                    }
                }
            }
        ),
        owner
    );
    // {player_avgpacketloss}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<PlayerContext, void (*)(const PlayerContext&, std::string&)>>(
            "{player_avgpacketloss}",
            +[](const PlayerContext& c, std::string& out) {
                out = "0";
                if (c.player) {
                    if (auto ns = c.player->getNetworkStatus()) {
                        out = std::to_string(ns->mCurrentPacketLoss);
                    }
                }
            }
        ),
        owner
    );
    // {player_averagepacketloss}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<PlayerContext, void (*)(const PlayerContext&, std::string&)>>(
            "{player_averagepacketloss}",
            +[](const PlayerContext& c, std::string& out) {
                out = "0";
                if (c.player) {
                    if (auto ns = c.player->getNetworkStatus()) {
                        out = std::to_string(ns->mAveragePacketLoss);
                    }
                }
            }
        ),
        owner
    );

    // {player_os}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<PlayerContext, void (*)(const PlayerContext&, std::string&)>>(
            "{player_os}",
            +[](const PlayerContext& c, std::string& out) {
                out = "Unknown";
                if (c.player) {
                    switch (c.player->mBuildPlatform) {
                    case BuildPlatform::Google:
                        out = "Android";
                        break;
                    case BuildPlatform::IOS:
                        out = "iOS";
                        break;
                    case BuildPlatform::Osx:
                        out = "macOS";
                        break;
                    case BuildPlatform::Amazon:
                        out = "Amazon";
                        break;
                    case BuildPlatform::Uwp:
                        out = "UWP";
                        break;
                    case BuildPlatform::Win32:
                        out = "Windows";
                        break;
                    case BuildPlatform::Dedicated:
                        out = "Dedicated";
                        break;
                    case BuildPlatform::Sony:
                        out = "PlayStation";
                        break;
                    case BuildPlatform::Nx:
                        out = "Nintendo Switch";
                        break;
                    case BuildPlatform::Xbox:
                        out = "Xbox";
                        break;
                    case BuildPlatform::Linux:
                        out = "Linux";
                        break;
                    default:
                        break;
                    }
                }
            }
        ),
        owner
    );


    // {mob_can_fly}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<MobContext, void (*)(const MobContext&, std::string&)>>(
            "{mob_can_fly}",
            +[](const MobContext& c, std::string& out) {
                bool can = false;
                if (c.mob) can = c.mob->canFly();
                out = can ? "true" : "false";
            }
        ),
        owner
    );

    // {mob_health}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<MobContext, void (*)(const MobContext&, std::string&)>>(
            "{mob_health}",
            +[](const MobContext& c, std::string& out) {
                out = "0";
                if (c.mob) {
                    auto h = ActorAttribute::getHealth(c.mob->getEntityContext());
                    out    = std::to_string(h);
                }
            }
        ),
        owner
    );

    // {actor_is_on_ground}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_is_on_ground}",
            +[](const ActorContext& c, std::string& out) {
                bool onGround = false;
                if (c.actor) onGround = c.actor->isOnGround();
                out = onGround ? "true" : "false";
            }
        ),
        owner
    );

    // {actor_is_alive}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_is_alive}",
            +[](const ActorContext& c, std::string& out) {
                bool alive = false;
                if (c.actor) alive = c.actor->isAlive();
                out = alive ? "true" : "false";
            }
        ),
        owner
    );

    // {actor_is_invisible}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_is_invisible}",
            +[](const ActorContext& c, std::string& out) {
                bool invisible = false;
                if (c.actor) invisible = c.actor->isInvisible();
                out = invisible ? "true" : "false";
            }
        ),
        owner
    );


    // {actor_type_id}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_type_id}",
            +[](const ActorContext& c, std::string& out) {
                out = "0";
                if (c.actor) out = std::to_string(static_cast<int>(c.actor->getEntityTypeId()));
            }
        ),
        owner
    );
    // {actor_type_name}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_type_name}",
            +[](const ActorContext& c, std::string& out) {
                out = "0";
                if (c.actor) out = (c.actor->getTypeName());
            }
        ),
        owner
    );
    // {actor_pos}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_pos}",
            +[](const ActorContext& c, std::string& out) {
                out = "0";
                if (c.actor) out = (c.actor->getPosition().toString());
            }
        ),
        owner
    );
    // {actor_pos_x}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_pos_x}",
            +[](const ActorContext& c, std::string& out) {
                out = "0";
                if (c.actor) out = std::to_string(c.actor->getPosition().x);
            }
        ),
        owner
    );
    // {actor_pos_y}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_pos_y}",
            +[](const ActorContext& c, std::string& out) {
                out = "0";
                if (c.actor) out = std::to_string(c.actor->getPosition().y);
            }
        ),
        owner
    );
    // {actor_pos_z}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_pos_z}",
            +[](const ActorContext& c, std::string& out) {
                out = "0";
                if (c.actor) out = std::to_string(c.actor->getPosition().z);
            }
        ),
        owner
    );
    // {actor_rotation}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_rotation}",
            +[](const ActorContext& c, std::string& out) {
                out = "0";
                if (c.actor) out = c.actor->getRotation().toString();
            }
        ),
        owner
    );
    // {actor_rotation_x}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_rotation_x}",
            +[](const ActorContext& c, std::string& out) {
                out = "0";
                if (c.actor) out = std::to_string(c.actor->getRotation().x);
            }
        ),
        owner
    );
    // {actor_rotation_y}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_rotation_y}",
            +[](const ActorContext& c, std::string& out) {
                out = "0";
                if (c.actor) out = std::to_string(c.actor->getRotation().y);
            }
        ),
        owner
    );
    // {actor_unique_id}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_unique_id}",
            +[](const ActorContext& c, std::string& out) {
                out = "0";
                if (c.actor) out = std::to_string(c.actor->getOrCreateUniqueID().rawID);
            }
        ),
        owner
    );

    // {actor_is_baby}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_is_baby}",
            +[](const ActorContext& c, std::string& out) {
                bool isBaby = false;
                if (c.actor) isBaby = c.actor->isBaby();
                out = isBaby ? "true" : "false";
            }
        ),
        owner
    );

    // {actor_is_riding}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_is_riding}",
            +[](const ActorContext& c, std::string& out) {
                bool isRiding = false;
                if (c.actor) isRiding = c.actor->isRiding();
                out = isRiding ? "true" : "false";
            }
        ),
        owner
    );

    // {actor_is_tame}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_is_tame}",
            +[](const ActorContext& c, std::string& out) {
                bool isTame = false;
                if (c.actor) isTame = c.actor->isTame();
                out = isTame ? "true" : "false";
            }
        ),
        owner
    );
    // {actor_is_tame}
    svc->registerPlaceholder(
        "",
        std::make_shared<TypedLambdaPlaceholder<ActorContext, void (*)(const ActorContext&, std::string&)>>(
            "{actor_is_tame}",
            +[](const ActorContext& c, std::string& out) {
                bool isTame = false;
                if (c.actor) isTame = c.actor->isTame();
                out = isTame ? "true" : "false";
            }
        ),
        owner
    );
    // {online_players}
    svc->registerPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{online_players}",
            +[](std::string& out) {
                auto level = ll::service::getLevel();
                out        = level ? std::to_string(level->getActivePlayerCount()) : "0";
            }
        ),
        owner
    );

    // {max_players}
    svc->registerPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{max_players}",
            +[](std::string& out) {
                auto server = ll::service::getServerNetworkHandler();
                out         = server ? std::to_string(server->mMaxNumPlayers) : "0";
            }
        ),
        owner
    );

    // {total_entities}
    svc->registerPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{total_entities}",
            +[](std::string& out) {
                auto level = ll::service::getLevel();
                out        = level ? std::to_string(level->getEntities().size()) : "0";
            }
        ),
        owner
    );

    // 服务器版本占位符
    svc->registerPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{server_version}",
            +[](std::string& out) { out = ll::getGameVersion().to_string(); }
        ),
        owner
    );

    // 服务器协议版本占位符
    svc->registerPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{server_protocol_version}",
            +[](std::string& out) { out = std::to_string(ll::getNetworkProtocolVersion()); }
        ),
        owner
    );
    // 加载器版本
    svc->registerPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{server_protocol_version}",
            +[](std::string& out) { out = ll::getLoaderVersion().to_string(); }
        ),
        owner
    );
    // 时间类占位符
    svc->registerPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{time}",
            +[](std::string& out) {
                auto               now = std::chrono::system_clock::now();
                auto               tt  = std::chrono::system_clock::to_time_t(now);
                auto               tm  = local_tm(tt);
                std::ostringstream ss;
                ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
                out = ss.str();
            }
        ),
        owner
    );

    svc->registerPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{year}",
            +[](std::string& out) {
                auto tt = std::time(nullptr);
                auto tm = local_tm(tt);
                out     = std::to_string(tm.tm_year + 1900);
            }
        ),
        owner
    );

    svc->registerPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{month}",
            +[](std::string& out) {
                auto tt = std::time(nullptr);
                auto tm = local_tm(tt);
                out     = std::to_string(tm.tm_mon + 1);
            }
        ),
        owner
    );

    svc->registerPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{day}",
            +[](std::string& out) {
                auto tt = std::time(nullptr);
                auto tm = local_tm(tt);
                out     = std::to_string(tm.tm_mday);
            }
        ),
        owner
    );

    svc->registerPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{hour}",
            +[](std::string& out) {
                auto tt = std::time(nullptr);
                auto tm = local_tm(tt);
                out     = std::to_string(tm.tm_hour);
            }
        ),
        owner
    );

    svc->registerPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{minute}",
            +[](std::string& out) {
                auto tt = std::time(nullptr);
                auto tm = local_tm(tt);
                out     = std::to_string(tm.tm_min);
            }
        ),
        owner
    );

    svc->registerPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{second}",
            +[](std::string& out) {
                auto tt = std::time(nullptr);
                auto tm = local_tm(tt);
                out     = std::to_string(tm.tm_sec);
            }
        ),
        owner
    );

    // {server_memory_usage}
    svc->registerPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{server_memory_usage}",
            +[](std::string& out) {
                std::ostringstream ss;
                ss << std::fixed << std::setprecision(2) << getMemoryUsage();
                out = ss.str();
            }
        ),
        owner
    );

    // {server_cpu_usage}
    svc->registerPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{server_cpu_usage}",
            +[](std::string& out) {
                std::ostringstream ss;
                ss << std::fixed << std::setprecision(2) << getCpuUsage();
                out = ss.str();
            }
        ),
        owner
    );

    // {system_total_memory}
    svc->registerPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{system_total_memory}",
            +[](std::string& out) {
                std::ostringstream ss;
                ss << std::fixed << std::setprecision(2) << getTotalMemory();
                out = ss.str();
            }
        ),
        owner
    );

    // {system_used_memory}
    svc->registerPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{system_used_memory}",
            +[](std::string& out) {
                std::ostringstream ss;
                ss << std::fixed << std::setprecision(2) << getUsedMemory();
                out = ss.str();
            }
        ),
        owner
    );

    // {system_cpu_usage}
    svc->registerPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{system_cpu_usage}",
            +[](std::string& out) {
                std::ostringstream ss;
                ss << std::fixed << std::setprecision(2) << getSystemCpuUsage();
                out = ss.str();
            }
        ),
        owner
    );
}

} // namespace PA
