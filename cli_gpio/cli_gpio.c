#include <stdio.h>
#define LOG_TAG "gpio"
#include "cli_log.h"
#include "cli_gpio.h"
#include "interface_registry.h"
#include <freertos/queue.h>
#include <freertos/task.h>
// typedef int8_t (*gpio_cmd_func_t)(gpioHandle_t, char *);

static bool kxgpio_hal_init = false;
static QueueHandle_t gpio_event_queue = NULL;
static TaskHandle_t gpio_trigger_handler_task = NULL;

cmdEntry_t gpio_cmds[] = {
    {"set_pin", cli_gpio_set_pin, "set_pin <IO_x>                        : Set active GPIO pin (e.g. set_pin IO_5)"},
    {"get_config", cli_gpio_get_config, "get_config                            : Print current pin, dir, pull, level"},
    {"set_dir", cli_gpio_set_dir, "set_dir <input|output>                : Set pin direction"},
    {"set_pull", cli_gpio_set_pull, "set_pull <no_pull|pull_up|pull_down>  : Set pin pull mode"},
    {"set", cli_gpio_set, "set                                   : Drive active pin HIGH"},
    {"clear", cli_gpio_clear, "clear                                 : Drive active pin LOW"},
    {"read", cli_gpio_read, "read                                  : Read and print active pin level"},
    {"toggle", cli_gpio_toggle, "toggle                                : Toggle active pin level"},
    {"set_trigger", cli_gpio_set_trigger, "set_trigger <pos_edge|neg_edge|any_edge|low|high> : Set interrupt trigger on active pin"},
    {"clear_trigger", cli_gpio_clear_trigger, "clear_trigger                         : Disable interrupt trigger on active pin"},
    {"", NULL, ""} // sentinel
};

IRAM_ATTR void cli_gpio_intr_cb(uint32_t pin, TickType_t time)
{
    gpioTriggerQueueMember_t evt = {
        .pin = pin,
        .time = time,
    };
    BaseType_t high_task_wakeup = pdFALSE;
    xQueueSendFromISR(gpio_event_queue, &evt, &high_task_wakeup);
    if (high_task_wakeup)
    {
        portYIELD_FROM_ISR();
    }
}

// this is a task that will wait for a queue and process the data
void cli_gpio_trigger_handler_task(void *args)
{
    (void)args;
    gpioTriggerQueueMember_t pin_data;
    LOG_INFO("cli_gpio_trigger_handler_task started");
    while (xQueueReceive(gpio_event_queue, (void *)&pin_data, portMAX_DELAY))
    {
        Kx_GpioEvent_t trig_type;
        Kx_ErrorCode status = KxGpio_get_intr(pin_data.pin, &trig_type);
        if (status != KX_HAL_OK)
        {
            LOG_ERR("Failed to read trigger type for pin=%" PRIu32, pin_data.pin);
            continue;
        }

        switch (trig_type)
        {
        case KX_GPIO_INTR_POSEDGE:
            LOG_INFO("Trigger pin=%" PRIu32 " time=%" PRIu32 " type=POS_EDGE",
                      pin_data.pin, pin_data.time);
            break;
        case KX_GPIO_INTR_NEGEDGE:
            LOG_INFO("Trigger pin=%" PRIu32 " time=%" PRIu32 " type=NEG_EDGE",
                      pin_data.pin, pin_data.time);
            break;
        case KX_GPIO_INTR_ANYEDGE:
            LOG_INFO("Trigger pin=%" PRIu32 " time=%" PRIu32 " type=ANY_EDGE",
                      pin_data.pin, pin_data.time);
            break;
        case KX_GPIO_INTR_LOW_LEVEL:
            LOG_INFO("Trigger pin=%" PRIu32 " time=%" PRIu32 " type=LOW_LEVEL",
                      pin_data.pin, pin_data.time);
            break;
        case KX_GPIO_INTR_HIGH_LEVEL:
            LOG_INFO("Trigger pin=%" PRIu32 " time=%" PRIu32 " type=HIGH_LEVEL",
                      pin_data.pin, pin_data.time);
            break;
        case KX_GPIO_INTR_DISABLE:
        default:
            LOG_WARN("Trigger pin=%" PRIu32 " time=%" PRIu32 " type=UNKNOWN/DISABLED",
                       pin_data.pin, pin_data.time);
            break;
        }
    }
}
int8_t cli_gpio_register()
{
    LOG_INFO("Registring GPIO interface for CLI");
    InterfaceRegistryEntry_t entry;
    entry.id = INTERFACE_GPIO;
    strcpy(entry.name, "gpio");
    entry.init = cli_gpio_init;
    entry.de_init = cli_gpio_deinit;
    entry.cmd_handler = cli_gpio_cmd_dispatch;
    entry.help_handler = cli_gpio_help;

    // create the queue

    gpio_event_queue = xQueueCreate(10, sizeof(gpioTriggerQueueMember_t));

    if (gpio_event_queue == NULL)
    {
        LOG_ERR("gpio event queue creation failed!!!");
    }
    Kx_ErrorCode status;
    status = KxGpio_register_intr_cb(cli_gpio_intr_cb);
    if (status != KX_HAL_OK)
    {
        LOG_ERR("GPIO event call back function register failed");
    }

    // start the trigger_handler task
    xTaskCreate(cli_gpio_trigger_handler_task, "cli_gpio_trigger_handler_task", 2048, NULL, 1, &gpio_trigger_handler_task);

    KxGpio_Init();

    int8_t reg_sts = interface_registry_register(&entry);
    if (reg_sts == 0)
    {
        LOG_INFO("CLI GPIO registration success");
        return 0;
    }
    else
    {
        LOG_ERR("CLI GPIO registration fail");
        return 1;
    }
}

int8_t cli_gpio_init(void **handle)
{
    // For this simple implementation, we just allocate a struct to hold the config
    gpioHandle_t gpio = (gpioHandle_t)malloc(sizeof(gpioconfig_t));
    if (!gpio)
        return -1; // allocation failed
    memset(gpio, 0, sizeof(gpioconfig_t));
    // set the pin to ff
    gpio->pin = 0xFF;
    *handle = (void *)gpio;
    return 0;
}

int8_t cli_gpio_deinit(void *handle)
{
    if (handle)
    {
        free(handle);
        return 0;
    }
    return -1; // invalid handle
}

int8_t cli_gpio_cmd_dispatch(void *handle, const char *cmd)
{
    if (handle == NULL)
    {
        return -1;
    }
    char key[32];
    char args[128];
    cmd_parse(cmd, key, 32, args, 128);
    int8_t cb_idx = cmd_tbl_search(key, gpio_cmds, sizeof(gpio_cmds) / sizeof(cmdEntry_t));
    if (cb_idx == -1)
    {
        LOG_ERR("\n\r Unknown GPIO command \"%s\" not found", key);
        return -1;
    }
    gpio_cmds[cb_idx].func(handle, args);
    return 0;
}

void cli_gpio_help(void)
{
    printf("\n\r--- GPIO Interface Commands ---\n\r");
    for (int i = 0; gpio_cmds[i].func != NULL; i++)
    {
        printf("  %s\n\r", gpio_cmds[i].help_str);
    }
    printf("--------------------------------\n\r");
}

static int8_t get_io_num(char *io)
{
    // io string is defined as IO_x
    // can we use sscanf to get the number??
    int num = -1;
    if (io != NULL)
    {

        sscanf(io, "IO_%d", &num);
        return num;
    }
    return -1;
}

int8_t cli_gpio_set_pin(void *handle, char *args)
{
    if (handle == NULL)
    {
        return -1;
    }
    // must validate io number
    gpioconfig_t *gpio = (gpioconfig_t *)handle;
    int8_t retVal = get_io_num(args);
    if (retVal != -1)
    {
        gpio->pin = retVal;
        LOG_INFO("GPIO pin set to %d", gpio->pin);
        return 0;
    }
    else
    {

        gpio->pin = 0xFF;
        LOG_ERR("Invalid IO number");
        return -1;
    }
}

static void print_config(gpioconfig_t *config)
{
    printf("pin=%d  mode=%d  pull=%d  level=%d\n",
           config->pin, config->dir, config->pull, config->level);
}
// this fuction is to get the set values for interface
int8_t cli_gpio_get_config(void *handle, char *args)
{
    // handle null check
    if (handle == NULL)
    {
        return -1;
    }
    gpioconfig_t *gpio = (gpioconfig_t *)handle;
    // print all the configs
    print_config(gpio);
    return 0;
}

int8_t cli_gpio_set_dir(void *handle, char *args)
{
    if (handle == NULL)
    {
        return -1;
    }
    gpioconfig_t *gpio = (gpioconfig_t *)handle;

    if (strcmp(args, "output") == 0)
    {
        gpio->dir = 0;
        KxGpio_SetDirection(gpio->pin, KX_GPIO_DIR_OUTPUT);
    }
    else if (strcmp(args, "input") == 0)
    {
        gpio->dir = 1;
        KxGpio_SetDirection(gpio->pin, KX_GPIO_DIR_INPUT);
    }
    else
    {
        LOG_ERR("\n\rInvalid pin direction");
    }
    return 0;
}

int8_t cli_gpio_set_pull(void *handle, char *args)
{
    if (handle == NULL)
    {
        return -1;
    }
    gpioconfig_t *gpio = (gpioconfig_t *)handle;
    if (strcmp(args, "no_pull") == 0)
    {
        gpio->pull = 0;
        KxGpio_SetPull(gpio->pin, KX_GPIO_PULL_NONE);
    }
    else if (strcmp(args, "pull_up") == 0)
    {
        gpio->pull = 1;
        KxGpio_SetPull(gpio->pin, KX_GPIO_PULL_UP);
    }
    else if (strcmp(args, "pull_down") == 0)
    {
        gpio->pull = 2;
        KxGpio_SetPull(gpio->pin, KX_GPIO_PULL_DOWN);
    }
    else
    {
        LOG_ERR("\n\rInvalid PULL mode");
    }
    return 0;
}

int8_t cli_gpio_set(void *handle, char *args)
{
    if (handle == NULL)
    {
        return -1;
    }
    gpioconfig_t *gpio = (gpioconfig_t *)handle;
    if (gpio->pin == 0xFF)
    {
        LOG_WARN("IO is not set for this pin");
        return KX_HAL_ERR_FAIL;
    }

    Kx_ErrorCode retVal;
    retVal = KxGpio_Set(gpio->pin);
    if (retVal != KX_HAL_OK)
    {
        LOG_ERR("Unable to set the pin");
        return KX_HAL_ERR_FAIL;
    }
    gpio->level = 1;

    return KX_HAL_OK;
}

int8_t cli_gpio_clear(void *handle, char *args)
{
    if (handle == NULL)
    {
        return -1;
    }
    gpioconfig_t *gpio = (gpioconfig_t *)handle;
    if (gpio->pin == 0xFF)
    {
        LOG_WARN("IO is not set for this pin");
        return KX_HAL_ERR_FAIL;
    }

    Kx_ErrorCode retVal;
    retVal = KxGpio_Clear(gpio->pin);
    if (retVal != KX_HAL_OK)
    {
        LOG_ERR("Unable to set the pin");
        return KX_HAL_ERR_FAIL;
    }

    gpio->level = 0;

    return KX_HAL_OK;
}

int8_t cli_gpio_read(void *handle, char *args)
{
    if (handle == NULL)
    {
        return -1;
    }
    gpioconfig_t *gpio = (gpioconfig_t *)handle;
    if (gpio->pin == 0xFF)
    {
        LOG_WARN("IO is not set for this pin");
        return KX_HAL_ERR_FAIL;
    }

    Kx_ErrorCode retVal;
    Kx_GpioState_t pin_state;
    retVal = KxGpio_Read(gpio->pin, &pin_state);
    if (retVal != KX_HAL_OK)
    {
        LOG_ERR("Unable to read the pin");
        return KX_HAL_ERR_FAIL;
    }

    if (pin_state == KX_GPIO_STATE_HIGH)
    {
        LOG_INFO("IO_%d is HIGH", gpio->pin);
    }
    else if (pin_state == KX_GPIO_STATE_LOW)
    {
        LOG_INFO("IO_%d is LOW", gpio->pin);
    }

    return KX_HAL_OK;
}

int8_t cli_gpio_toggle(void *handle, char *args)
{
    if (handle == NULL)
    {
        return -1;
    }
    gpioconfig_t *gpio = (gpioconfig_t *)handle;
    if (gpio->pin == 0xFF)
    {
        LOG_WARN("IO is not set for this pin");
        return KX_HAL_ERR_FAIL;
    }

    Kx_ErrorCode retVal;
    // retVal = KxGpio_Toggle(gpio->pin);
    if (gpio->level == 0)
    {
        retVal = KxGpio_Set(gpio->pin);
        if (retVal != KX_HAL_OK)
        {
            LOG_ERR("Unable to set the pin");
            return KX_HAL_ERR_FAIL;
        }
        gpio->level = 1;
    }
    else if (gpio->level == 1)
    {
        retVal = KxGpio_Clear(gpio->pin);
        if (retVal != KX_HAL_OK)
        {
            LOG_ERR("Unable to set the pin");
            return KX_HAL_ERR_FAIL;
        }

        gpio->level = 0;
    }
    else
    {
        retVal = KX_HAL_ERR_FAIL;
    }

    if (retVal != KX_HAL_OK)
    {
        LOG_ERR("Unable to toggle the pin");
        return KX_HAL_ERR_FAIL;
    }

    return KX_HAL_OK;
}

int8_t cli_gpio_set_trigger(void *handle, char *args)
{
    if (handle == NULL)
    {
        return -1;
    }
    gpioconfig_t *gpio = (gpioconfig_t *)handle;

    if (0xFF == gpio->pin)
    {
        LOG_ERR("Pin is not set yet");
        return KX_HAL_ERR_INVALID_ARG;
    }

    Kx_GpioEvent_t trig_type = KX_GPIO_INTR_DISABLE;
    if (strcmp(args, "pos_edge") == 0)
    {
        trig_type = KX_GPIO_INTR_POSEDGE;
    }
    else if (strcmp(args, "neg_edge") == 0)
    {
        trig_type = KX_GPIO_INTR_NEGEDGE;
    }
    else if (strcmp(args, "any_edge") == 0)
    {
        trig_type = KX_GPIO_INTR_ANYEDGE;
    }
    else if (strcmp(args, "low") == 0)
    {
        trig_type = KX_GPIO_INTR_LOW_LEVEL;
    }
    else if (strcmp(args, "high") == 0)
    {
        trig_type = KX_GPIO_INTR_HIGH_LEVEL;
    }
    else
    {
        LOG_ERR("Invalid trigger type");
        return KX_HAL_ERR_INVALID_ARG;
    }

    KxGpio_set_intr(gpio->pin, trig_type);
    gpio->trigger = trig_type;
    return KX_HAL_OK;
}

int8_t cli_gpio_clear_trigger(void *handle, char *args)
{
    if (handle == NULL)
    {
        return -1;
    }
    gpioconfig_t *gpio = (gpioconfig_t *)handle;

    if (0xFF == gpio->pin)
    {
        LOG_ERR("Pin is not set yet");
        return KX_HAL_ERR_INVALID_ARG;
    }

    KxGpio_clear_intr(gpio->pin);
    gpio->trigger = KX_GPIO_INTR_DISABLE;
    return KX_HAL_OK;
}