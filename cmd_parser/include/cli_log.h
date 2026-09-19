#ifndef CLI_LOG_H
#define CLI_LOG_H

#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

/*
 * journalctl-style logging:  tag[pid]: level: message
 *
 * Example:
 *   gpio[main]: info: GPIO pin set to 5
 *   app[main]: err: Failed to initialize GPIO interface
 *
 * FreeRTOS has no PIDs, so the current task name stands in for the pid field,
 * which is the closest analog to journalctl's tag[pid] convention.
 *
 * Each .c file defines LOG_TAG before including this header (or it falls back
 * to the file name).
 */

#ifndef LOG_TAG
#define LOG_TAG __FILE__
#endif

#define CLI_LOG_PROC() (pcTaskGetName(NULL))

#define LOG_LEVEL(level, fmt, ...) \
    printf("\n\r%s[%s]: " level ": " fmt, LOG_TAG, CLI_LOG_PROC(), ##__VA_ARGS__)

#define LOG_ERR(fmt, ...)   LOG_LEVEL("err",   fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  LOG_LEVEL("warn",  fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  LOG_LEVEL("info",  fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) LOG_LEVEL("debug", fmt, ##__VA_ARGS__)

#endif /* CLI_LOG_H */
