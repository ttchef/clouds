
#ifndef LOG_H
#define LOG_H

#define LOGGER_LEVELS                                                          \
    _X(API_DUMP, LOGGER_COLOR_MAGENTA)                                         \
    _X(INFO, LOGGER_COLOR_BLUE)                                                \
    _X(WARN, LOGGER_COLOR_YELLOW)                                              \
    _X(ERROR, LOGGER_COLOR_RED)

// the logger needs at least one error
#define LOGGER_TYPES _X(ERROR, RANDOM, "random error")

#include <logger/logger.h>

#endif // LOG_H
