// src/PA/ScriptExports.h
#pragma once

namespace PA {
namespace ScriptExports {

// 注册所有对脚本公开的 RemoteCall API
void install();

// 反注册（卸载时调用）
void uninstall();

} // namespace ScriptExports
} // namespace PA