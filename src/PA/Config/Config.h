#pragma once

struct Config {
    int  version   = 1;
    bool debugMode       = false; // 新增：是否启用调试模式，启用后会在占位符解析失败时输出警告
    int  globalCacheSize = 1024;  // 全局占位符缓存的大小
};
