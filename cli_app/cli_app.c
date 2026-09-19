#include <stdio.h>
#define LOG_TAG "app"
#include "cli_log.h"
#include "cli_app.h"
#include "cli_gpio.h"
#include "cli_pwm.h"
#include "cli_i2c.h"

#include "esp_system.h"
#include "esp_err.h"
#include "esp_chip_info.h"
#include "esp_timer.h"
#include <freertos/task.h>

// global variable to store current mode

interface_instance_t interfaces[10]; // global array to hold interface handles

cmdEntry_t g_cmd_table[] = {
    {"print_banner", cmd_print_banner, "print_banner", "Show the startup banner"},
    {"time", cmd_time, "time", "Show uptime since boot"},
    {"sysinfo", cmd_sysinfo, "sysinfo", "Show chip and heap information"},
    {"panic", cmd_panic, "panic", "Break me on purpose and trigger fault handling"},
    {"all", cmd_all, "all", "Show uptime and system info"},
    {"help", cmd_help, "help [interface]", "Show help message"},
    {"create", cmd_create, "create <interface> <name>", "Create a named interface instance"},
    {"use", cmd_use, "use <name> <cmd> [args]", "Run a command on a created interface"},
    {"reboot", cmd_reboot, "reboot", "Restart the device"},
    {"cls", cmd_console_clear, "cls", "Clear the console"},
    {"clear", cmd_console_clear, "clear", "Clear the console"},
    {"", NULL, "", ""},
};


QueueHandle_t cmd_queue_q;

void app_init(void)
{
    cmd_queue_q = xQueueCreate(3, sizeof(cli_cmd_t));
    cli_gpio_register();
    cli_pwm_register();
    cli_i2c_register();
    return;
}

static int8_t get_interface_handle(char *name)
{
    for (int i = 0; i < 10; i++)
    {
        if (interfaces[i].in_use && strcmp(interfaces[i].name, name) == 0)
        {
            return i;
        }
    }
    return -1; // no handle found for this mode
}

static int8_t get_free_handle_index()
{
    for (int i = 0; i < 10; i++)
    {
        if (interfaces[i].in_use == 0)
        {
            return i;
        }
    }
    return -1; // no handle found for this mode
}

// create <interface> <name>
// call the init function for the specified interface and store the handle in a global variable for that mode

void cmd_create(void *handle, char *args)
{
    char interface[32];
    char name[32];
    cmd_parse(args, interface, 32, name, 32);
    InterfaceId_e inf;
    InterfaceRegistryEntry_t *entry = interface_registry_lookup_name(interface);
    if (entry == NULL)
    {
        LOG_ERR("interface '%s' not found in registry", interface);
        return;
    }

    // if name is empty, must not allow
    if (strlen(name) == 0)
    {
        LOG_ERR("Name must be atleast 1 char");
        return;
    }

    // check for name duplication
    int8_t retVal = get_interface_handle(name);
    if (retVal != -1)
    {
        LOG_ERR("interface name '%s' already exists", name);
        return;
    }

    int8_t new_inf_idx = get_free_handle_index();
    if (new_inf_idx == -1)
    {
        LOG_ERR("no free handle slots available");
        return;
    }

    entry->init(&interfaces[new_inf_idx].inf_handle);
    if (interfaces[new_inf_idx].inf_handle == NULL)
    {
        LOG_ERR("failed to create interface '%s', returned NULL handle", interface);
        return;
    }

    interfaces[new_inf_idx].in_use = 1;
    interfaces[new_inf_idx].inf = entry->id;
    strncpy(interfaces[new_inf_idx].name, name, 32);
    LOG_INFO("interface '%s' created with name '%s'", interface, name);
}

void cmd_use(void *handle, char *args)
{
    // the args have the name, find the handle, call the interface_cmd_dispatch function for the current mode with the handle and the rest of the args
    char name[32];
    char cmdArgs[128];
    cmd_parse(args, name, 32, cmdArgs, 128);
    int8_t indx = get_interface_handle(name);
    if (indx == -1)
    {
        LOG_ERR("\n\rno interface found with name '%s'", name);
        return;
    }

    InterfaceRegistryEntry_t *inf_entry = interface_registry_lookup_id(interfaces[indx].inf);

    if (inf_entry == NULL)
    {
        LOG_ERR("\n\rinterface '%s' not found in registry", name);
        return;
    }

    inf_entry->cmd_handler(interfaces[indx].inf_handle, cmdArgs);
}

void cmd_print_banner(void *handle, char *args)
{
    (void)args;
    printf("\r\n");
    printf("  ██╗  ██╗ █████╗ ██████╗ ██╗   ██╗██╗   ██╗██╗    ██╗  ██╗\r\n");
    printf("  ██║ ██╔╝██╔══██╗██╔══██╗██║   ██║██║   ██║██║    ╚██╗██╔╝\r\n");
    printf("  █████╔╝ ███████║██████╔╝██║   ██║██║   ██║██║     ╚███╔╝ \r\n");
    printf("  ██╔═██╗ ██╔══██║██╔══██╗██║   ██║╚██╗ ██╔╝██║     ██╔██╗ \r\n");
    printf("  ██║  ██╗██║  ██║██║  ██║╚██████╔╝ ╚████╔╝ ██║    ██╔╝ ██╗\r\n");
    printf("  ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝   ╚═══╝  ╚═╝    ╚═╝  ╚═╝\r\n");
    printf("\r\n");
    printf("  Open Source Engineer's Toolkit  |  ESP32-S3\r\n");
    printf("  v0.1.0  |  https://github.com/elizzer/Karuvi_X.git\r\n");
    printf("\r\n");
}

void cmd_time(void *handle, char *args)
{
    int64_t us = esp_timer_get_time();
    int64_t seconds = us / 1000000;
    int64_t micros = us % 1000000;
    printf("\n\rUptime: %lld.%06lld seconds\r\n", (long long)seconds, (long long)micros);
}

void cmd_sysinfo(void *handle, char *args)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    printf("\n\r");
    printf("Chip model: %s\r\n", chip_info.model == CHIP_ESP32S3 ? "ESP32-S3" : "Unknown");
    printf("Cores: %d\r\n", chip_info.cores);
    printf("Revision: %d\r\n", chip_info.revision);
    printf("Features bitmap: 0x%08" PRIx32 "\r\n", chip_info.features);
    printf("Free heap: %" PRIu32 " bytes\r\n", esp_get_free_heap_size());
    printf("Minimum free heap: %" PRIu32 " bytes\r\n", esp_get_minimum_free_heap_size());
}

void cmd_panic(void *handle, char *args)
{
    (void)args;
    printf("Breaking me on purpose...\r\n");
    abort();
}

void cmd_all(void *handle, char *args)
{
    (void)args;
    cmd_time("", NULL);
    cmd_sysinfo("", NULL);
}

void cmd_reboot(void *handle, char *args)
{
    (void)args;
    esp_restart();
}

void cmd_console_clear(void *handle, char *args)
{
    // \033[2J clears the entire screen, \033[H moves cursor to home (0,0)
    printf("\033[2J\033[H");
}

void cmd_help(void *handle, char *args);

void cmd_help(void *handle, char *args)
{

    printf("\r\n--- Karuvi X CLI Commands ---\r\n");
    if (strlen(args) != 0)
    {
        InterfaceRegistryEntry_t *inf = interface_registry_lookup_name(args);
        if (inf != NULL)
        {
            inf->help_handler();
        }
    }
    else
    {
        for (int i = 0; g_cmd_table[i].func != NULL; i++)
        {
            printf("  %-28s - %s\r\n",
                   g_cmd_table[i].key,
                   g_cmd_table[i].help_str);
        }
    }

    printf("-----------------------------\r\n");
}

void main_cmd_dispatch(const char *cmd)
{
    uint8_t status = cmd_dispatch(cmd, g_cmd_table, sizeof(g_cmd_table) / sizeof(cmdEntry_t));
    if (status == -1)
    {
        LOG_ERR("Unknown command :%s", cmd);
    }
}

// this is
void cli_app(void *pv)
{
    cli_cmd_t req;
    while (1)
    {
        if (xQueueReceive(cmd_queue_q, &req, portMAX_DELAY) == pdTRUE)
        {
            main_cmd_dispatch(req.cmd);

            if (req.caller != NULL) {
                xTaskNotifyGive(req.caller);
            }
            // caller == NULL means nobody's waiting — internal/fire-and-forget trigger
        }
    }
}
