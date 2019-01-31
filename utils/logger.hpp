#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <string>
#include <atomic>
#include "spdlog/spdlog.h"


class LoggerFactory {
private:
    static std::atomic<uint32_t> _initialize_state;
    static std::shared_ptr<spdlog::details::thread_pool> _thread_pool_holder;
    static std::shared_ptr<spdlog::logger> _default_logger;
    static void _initialize();
    static std::shared_ptr<spdlog::logger> _create_logger(
        const std::string &logger_name,
        spdlog::level::level_enum log_level);
public:
    // create the logger
    // @PARAM logger_name
    //     Name of the logger. The log file would be created as 
    //     "<logger_name>.log"
    // @PARAM log_level
    //     The level of the logger.
    // @RETURN
    //     The created logger.
    static std::shared_ptr<spdlog::logger> createLogger(
        const std::string &logger_name,
        spdlog::level::level_enum log_level = spdlog::level::info);
    // get the default logger
    static std::shared_ptr<spdlog::logger>& getDefaultLogger();
};

#ifndef NDEBUG
  // Debug version
  #define dbg_trace(logger, ...) logger->trace(__VA_ARGS__)
  #define dbg_default_trace(...) dbg_trace(LoggerFactory::getDefaultLogger(), __VA_ARGS__)
  #define dbg_debug(logger, ...) logger->debug(__VA_ARGS__)
  #define dbg_default_debug(...) dbg_debug(LoggerFactory::getDefaultLogger(), __VA_ARGS__)
  #define dbg_info(logger, ...) logger->info(__VA_ARGS__)
  #define dbg_default_info(...) dbg_info(LoggerFactory::getDefaultLogger(), __VA_ARGS__)
  #define dbg_warn(logger, ...) logger->warn(__VA_ARGS__)
  #define dbg_default_warn(...) dbg_warn(LoggerFactory::getDefaultLogger(), __VA_ARGS__)
  #define dbg_error(logger, ...) logger->error(__VA_ARGS__)
  #define dbg_default_error(...) dbg_error(LoggerFactory::getDefaultLogger(), __VA_ARGS__)
  #define dbg_crit(logger, ...) logger->crit(__VA_ARGS__)
  #define dbg_default_crit(...) dbg_crit(LoggerFactory::getDefaultLogger(), __VA_ARGS__)
  #define dbg_flush(logger) logger->flush()
  #define dbg_default_flush() LoggerFactory::getDefaultLogger()->flush()
#else
  #define dbg_trace(logger, ...)
  #define dbg_default_trace(...)
  #define dbg_debug(logger, ...)
  #define dbg_default_debug(...)
  #define dbg_info(logger, ...)
  #define dbg_default_info(...)
  #define dbg_warn(logger, ...)
  #define dbg_default_warn(...)
  #define dbg_error(logger, ...)
  #define dbg_default_error(...)
  #define dbg_crit(logger, ...)
  #define dbg_default_crit(...)
  #define dbg_flush(logger)
  #define dbg_default_flush()
  // Release version
#endif

#endif//LOGGER_HPP