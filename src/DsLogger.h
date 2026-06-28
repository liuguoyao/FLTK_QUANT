///////////////////////////////////////////////////////////////////////////////
// src/DsLogger.h
//
//   全局共享日志器 —— 基于 spdlog。
//
//   控制台(彩色 stdout)+ 滚动文件(logs/dag_editor.log,单文件 5MB、保留 3 个),
//   级别 = info。首次调用 DsLogger() 时懒初始化(文件 sink 失败则退化为仅控制台)。
//
//   设计意图:多个模块(datasource / tool / 其它)共用同一个 logger,
//   避免各处重复初始化。直接 #include "DsLogger.h" 后用:
//     DsLogger()->info(...);
//     DsLogger()->error(...);
//   或更简洁地用下面的 DS_LOG_* 宏。
///////////////////////////////////////////////////////////////////////////////
#ifndef DS_LOGGER_H
#define DS_LOGGER_H

#include <spdlog/spdlog.h>

#include <memory>

// 取得全局共享 logger(首次调用时初始化)。
std::shared_ptr<spdlog::logger> DsLogger();

// 便捷宏(等价于 DsLogger()->xxx)
#define DS_LOG_TRACE(...) DsLogger()->trace(__VA_ARGS__)
#define DS_LOG_DEBUG(...) DsLogger()->debug(__VA_ARGS__)
#define DS_LOG_INFO(...)  DsLogger()->info(__VA_ARGS__)
#define DS_LOG_WARN(...)  DsLogger()->warn(__VA_ARGS__)
#define DS_LOG_ERROR(...) DsLogger()->error(__VA_ARGS__)

#endif // DS_LOGGER_H
