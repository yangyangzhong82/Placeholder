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
}

} // namespace PA
