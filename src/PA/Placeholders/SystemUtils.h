// src/PA/Placeholders/SystemUtils.h
#pragma once

namespace PA {

double    getMemoryUsage();
double    getCpuUsage();
double    getTotalMemory();
double    getUsedMemory();
double    getFreeMemory();
long long getSystemUptime();
double    getSystemCpuUsage();

} // namespace PA
