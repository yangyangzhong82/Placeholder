#include "PA/Placeholders/SystemPlaceholders.h"
#include "PA/Placeholders/CommonPlaceholderTemplates.h"

#include <string>
#include <sstream>
#include <iomanip>
#include <chrono> 

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <fstream>
#include <sys/resource.h>
#include <sys/times.h>
#include <unistd.h>
#include <sys/sysinfo.h> 
#endif

namespace {

// System resource utils
#ifdef _WIN32
inline double getMemoryUsage() {
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        return static_cast<double>(pmc.PrivateUsage) / (1024.0 * 1024.0); // MB
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
        return static_cast<double>(memInfo.ullTotalPhys) / (1024.0 * 1024.0); // MB
    }
    return 0.0;
}

inline double getUsedMemory() {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        return static_cast<double>(memInfo.ullTotalPhys - memInfo.ullAvailPhys) / (1024.0 * 1024.0); // MB
    }
    return 0.0;
}

inline double getFreeMemory() {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        return static_cast<double>(memInfo.ullAvailPhys) / (1024.0 * 1024.0); // MB
    }
    return 0.0;
}

inline long long getSystemUptime() {
    return GetTickCount64() / 1000; // Seconds
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
    return static_cast<double>(parseMemInfo("MemTotal:")) / 1024.0; // MB
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
    return static_cast<double>(total - available) / 1024.0; // MB
}

inline double getFreeMemory() {
    long available = parseMemInfo("MemAvailable:");
    if (available == 0) { // Fallback for older kernels
        long free    = parseMemInfo("MemFree:");
        long buffers = parseMemInfo("Buffers:");
        long cached  = parseMemInfo("Cached:");
        available    = free + buffers + cached;
    }
    return static_cast<double>(available) / 1024.0; // MB
}

inline long long getSystemUptime() {
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        return info.uptime; // Seconds
    }
    return 0;
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

void registerSystemPlaceholders(IPlaceholderService* svc) {
    static int kBuiltinOwnerTag = 0;
    void*      owner            = &kBuiltinOwnerTag;

    // 记录服务器启动时间
    static auto serverStartTime = std::chrono::steady_clock::now();

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

    // {system_free_memory}
    svc->registerPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{system_free_memory}",
            +[](std::string& out) {
                std::ostringstream ss;
                ss << std::fixed << std::setprecision(2) << getFreeMemory();
                out = ss.str();
            }
        ),
        owner
    );

    // {system_memory_percent}
    svc->registerPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{system_memory_percent}",
            +[](std::string& out) {
                std::ostringstream ss;
                double total = getTotalMemory();
                double used = getUsedMemory();
                if (total > 0) {
                    ss << std::fixed << std::setprecision(2) << (used / total * 100.0);
                } else {
                    ss << "0.00";
                }
                out = ss.str();
            }
        ),
        owner
    );

    // {server_memory_percent}
    svc->registerPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{server_memory_percent}",
            +[](std::string& out) {
                std::ostringstream ss;
                double total = getTotalMemory();
                double server_usage = getMemoryUsage();
                if (total > 0) {
                    ss << std::fixed << std::setprecision(2) << (server_usage / total * 100.0);
                } else {
                    ss << "0.00";
                }
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

    // {system_uptime}
    svc->registerPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{system_uptime}",
            +[](std::string& out) {
                long long uptime_seconds = getSystemUptime();
                long long days = uptime_seconds / (24 * 3600);
                uptime_seconds %= (24 * 3600);
                long long hours = uptime_seconds / 3600;
                uptime_seconds %= 3600;
                long long minutes = uptime_seconds / 60;
                long long seconds = uptime_seconds % 60;

                std::ostringstream ss;
                if (days > 0) {
                    ss << days << "d ";
                }
                ss << std::setfill('0') << std::setw(2) << hours << ":"
                   << std::setfill('0') << std::setw(2) << minutes << ":"
                   << std::setfill('0') << std::setw(2) << seconds;
                out = ss.str();
            }
        ),
        owner
    );

    // {server_uptime}
    svc->registerPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&)>>(
            "{server_uptime}",
            +[](std::string& out) {
                auto now = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - serverStartTime);
                long long uptime_seconds = duration.count();

                long long days = uptime_seconds / (24 * 3600);
                uptime_seconds %= (24 * 3600);
                long long hours = uptime_seconds / 3600;
                uptime_seconds %= 3600;
                long long minutes = uptime_seconds / 60;
                long long seconds = uptime_seconds % 60;

                std::ostringstream ss;
                if (days > 0) {
                    ss << days << "d ";
                }
                ss << std::setfill('0') << std::setw(2) << hours << ":"
                   << std::setfill('0') << std::setw(2) << minutes << ":"
                   << std::setfill('0') << std::setw(2) << seconds;
                out = ss.str();
            }
        ),
        owner
    );
}

} // namespace PA
