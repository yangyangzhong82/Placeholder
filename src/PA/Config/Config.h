#pragma once

struct Config {
    int  version           = 1;
    bool debugMode         = false; // 新增：是否启用调试模式，启用后会在占位符解析失败时输出警告
    int  globalCacheSize   = 1024;  // 全局占位符缓存的大小
    int  asyncThreadPoolSize = -1;    // 异步占位符线程池的大小，-1 表示默认（硬件线程数）
    int  asyncThreadPoolQueueSize = 0; // 异步线程池的队列上限，0 表示无限制
    int  asyncPlaceholderTimeoutMs = 2000; // 异步占位符的超时时间（毫秒）
};
