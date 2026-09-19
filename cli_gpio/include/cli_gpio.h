#ifndef CLI_GPIO_H
#define CLI_GPIO_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "cmd_parser.h"
#include "cli_app.h"
#include "kx_hal_types.h"
#include "kx_gpio_hal.h"



typedef struct
{
    Kx_GpioPin pin;         // GPIO pin number
    Kx_GpioDirection_t dir; // GPIO mode (input, output, etc.) 0=>output 1=>input
    Kx_GpioPull_t pull;     // Pull-up/pull-down configuration 0=> no_pull 1=>pull_up 2=>pull_down
    Kx_GpioState_t level;   // Output level (0 or 1)
    Kx_GpioEvent_t trigger;
} gpioconfig_t;

typedef struct{
    uint32_t pin;
    TickType_t time;
}gpioTriggerQueueMember_t;

typedef gpioconfig_t *gpioHandle_t;
// define gpio specific commands function pairs

int8_t cli_gpio_register();

int8_t cli_gpio_init(void **handle);
int8_t cli_gpio_deinit(void *handle);
void cli_gpio_help(void);
int8_t cli_gpio_cmd_dispatch(void *handle, const char *cmd);

int8_t cli_gpio_set_pin(void *handle, char *args);
int8_t cli_gpio_get_config(void *handle, char *args);
int8_t cli_gpio_set_dir(void *handle, char *args);
int8_t cli_gpio_set_pull(void *handle, char *args);
int8_t cli_gpio_clear(void *handle, char *args);
int8_t cli_gpio_set(void *handle, char *args);
int8_t cli_gpio_toggle(void *handle, char *args);
int8_t cli_gpio_read(void *handle, char *args);
int8_t cli_gpio_set_trigger(void *handle, char *args);
int8_t cli_gpio_clear_trigger(void *handle, char *args);

#endif // CLI_GPIO_H