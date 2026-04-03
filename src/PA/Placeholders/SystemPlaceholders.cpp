#include "PA/Placeholders/SystemPlaceholders.h"
#include "PA/Placeholders/CommonPlaceholderTemplates.h"
#include "PA/Placeholders/SystemUtils.h"

#include <string>
#include <sstream>
#include <iomanip>
#include <chrono>

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
