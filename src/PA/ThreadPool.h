#pragma once

#include "logger.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace PA {

constexpr size_t    THREAD_POOL_QUEUE_DEPTH_WARNING_THRESHOLD      = 100;
constexpr long long THREAD_POOL_TASK_DURATION_WARNING_THRESHOLD_MS = 1000;

class ThreadPool {
public:
    struct HealthMetrics {
        size_t    queue_depth;
        size_t    active_tasks;
        long long total_tasks_executed;
        double    average_execution_time_ms;
    };

    ThreadPool(size_t threads) : stop(false) {
        for (size_t i = 0; i < threads; ++i)
            workers.emplace_back([this] {
                for (;;) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });
                        if (this->stop && this->tasks.empty()) return;
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }

                    auto start_time = std::chrono::high_resolution_clock::now();
                    task();
                    auto end_time = std::chrono::high_resolution_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

                    if (duration.count() > THREAD_POOL_TASK_DURATION_WARNING_THRESHOLD_MS) {
                        logger.warn("ThreadPool task took too long: {}ms", duration.count());
                    }

                    total_execution_time_ms += duration.count();
                    total_tasks_executed++;

                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        active_tasks--;
                        if (tasks.empty() && active_tasks == 0) {
                            idle_condition.notify_all();
                        }
                    }
                }
            });
    }

    template <class F, class... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        using return_type = std::invoke_result_t<F, Args...>;

        auto task =
            std::make_shared<std::packaged_task<return_type()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));

        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            if (stop) throw std::runtime_error("enqueue on stopped ThreadPool");
            tasks.emplace([task]() { (*task)(); });
            active_tasks++;
            if (tasks.size() > THREAD_POOL_QUEUE_DEPTH_WARNING_THRESHOLD) {
                logger.warn("ThreadPool queue depth is high: {}", tasks.size());
            }
        }
        condition.notify_one();
        return res;
    }

    void shutdown() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            if (stop) return;
            stop = true;
        }
        condition.notify_all();
        idle_condition.notify_all();
        for (std::thread& worker : workers)
            worker.join();
    }

    void waitIdle() {
        std::unique_lock<std::mutex> lock(queue_mutex);
        idle_condition.wait(lock, [this] { return this->tasks.empty() && this->active_tasks == 0; });
    }

    size_t getQueueDepth() {
        std::unique_lock<std::mutex> lock(queue_mutex);
        return tasks.size();
    }

    HealthMetrics getHealthMetrics() {
        long long executed = total_tasks_executed.load();
        return {
            getQueueDepth(),
            active_tasks.load(),
            executed,
            executed > 0 ? static_cast<double>(total_execution_time_ms.load()) / executed : 0.0,
        };
    }

    ~ThreadPool() {
        shutdown();
    }

private:
    std::vector<std::thread>          workers;
    std::queue<std::function<void()>> tasks;
    std::mutex                        queue_mutex;
    std::condition_variable           condition;
    std::condition_variable           idle_condition;
    bool                              stop;
    std::atomic<size_t>               active_tasks{0};
    std::atomic<long long>            total_execution_time_ms{0};
    std::atomic<long long>            total_tasks_executed{0};
};

} // namespace PA
