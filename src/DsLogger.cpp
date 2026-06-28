///////////////////////////////////////////////////////////////////////////////
// src/DsLogger.cpp
//
//   全局共享日志器实现 —— 基于 spdlog。
//   详见 DsLogger.h。
///////////////////////////////////////////////////////////////////////////////
#include "DsLogger.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include <exception>

// 日志格式:[时间] [级别] [logger名] 消息
static const char *kLogPattern =
    "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v";

std::shared_ptr<spdlog::logger> DsLogger() {
    static std::shared_ptr<spdlog::logger> logger;
    if (logger) return logger;

    // 控制台 sink(彩色 stdout)始终可用
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    try {
        // 文件 sink:logs/dag_editor.log,单文件 5MB,保留 3 个
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            "logs/dag_editor.log", 5 * 1024 * 1024, 3);
        logger = std::make_shared<spdlog::logger>(
            "datasource", spdlog::sinks_init_list{console_sink, file_sink});
    } catch (const std::exception &e) {
        // 文件 sink 可能因目录不可写而抛异常:退化为只用控制台
        logger = std::make_shared<spdlog::logger>("datasource", console_sink);
        logger->set_pattern(kLogPattern);
        logger->set_level(spdlog::level::info);
        logger->warn("日志文件初始化失败({}),退化为仅控制台输出", e.what());
        return logger;
    }

    logger->set_level(spdlog::level::info);
    logger->set_pattern(kLogPattern);
    spdlog::register_logger(logger);
    return logger;
}
