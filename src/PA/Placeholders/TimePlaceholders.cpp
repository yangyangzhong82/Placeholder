#include "PA/Placeholders/TimePlaceholders.h"
#include "PA/Placeholders/CommonPlaceholderTemplates.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

namespace PA {

void registerTimePlaceholders(IPlaceholderService* svc) {
    static int kBuiltinOwnerTag = 0;
    void*      owner            = &kBuiltinOwnerTag;

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

    // {time_diff:<unix_timestamp>} 计算从指定时间到现在已经过去了多少分钟
    svc->registerPlaceholder(
        "",
        std::make_shared<ServerLambdaPlaceholder<void (*)(std::string&, const std::vector<std::string_view>&)>>(
            "{time_diff}",
            +[](std::string& out, const std::vector<std::string_view>& args) {
                if (args.empty()) {
                    out = "Invalid arguments";
                    return;
                }
                try {
                    long long target_timestamp = std::stoll(std::string(args[0]));
                    auto      now              = std::chrono::system_clock::now();
                    auto      target_time      = std::chrono::system_clock::from_time_t(target_timestamp);
                    
                    std::string unit = "minutes"; // 默认单位是分钟
                    if (args.size() > 1) {
                        unit = std::string(args[1]);
                    }

                    if (unit == "hours") {
                        auto diff = std::chrono::duration_cast<std::chrono::hours>(now - target_time);
                        out = std::to_string(diff.count());
                    } else if (unit == "days") {
                        auto diff = std::chrono::duration_cast<std::chrono::days>(now - target_time);
                        out = std::to_string(diff.count());
                    } else if (unit == "seconds") { // 添加对秒的支持
                        auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - target_time);
                        out = std::to_string(diff.count());
                    }
                    else { // 默认为分钟
                        auto diff = std::chrono::duration_cast<std::chrono::minutes>(now - target_time);
                        out = std::to_string(diff.count());
                    }
                } catch (const std::exception& e) {
                    out = "Error: " + std::string(e.what());
                }
            }
        ),
        owner
    );
}

} // namespace PA
