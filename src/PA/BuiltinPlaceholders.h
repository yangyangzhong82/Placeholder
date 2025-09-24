#pragma once

namespace PA {

/**
 * @brief 注册所有内置的（默认的）占位符
 *
 * 该函数将服务器和玩家相关的默认占位符注册到 PlaceholderManager 中。
 * 应该在插件加载时调用此函数。
 */
void registerBuiltinPlaceholders();

} // namespace PA
