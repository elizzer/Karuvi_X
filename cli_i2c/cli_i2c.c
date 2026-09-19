#include <stdio.h>
#include "cli_i2c.h"
#include "cli_app.h"
#include "cli_log.h"
#include "interface_registry.h"
#include "kx_i2c_hal.h"

#define CHECK_HANDLE_NULL(handle)      \
    do                                 \
    {                                  \
        if (handle == NULL)            \
        {                              \
            LOG_ERR("Handle is NULL"); \
            return -1;                 \
        }                              \
    } while (0)

#define LOG_TAG "CLI_I2C"

cmdEntry_t i2c_cmds[] = {
    {"alloc_instance", cli_i2c_alloc_instance, "alloc_instance [instance]             : Allocate an I2C instance (optional instance number, default: auto)"},
    {"set_sda", cli_i2c_set_sda, "set_sda <IO_x>                        : Set the SDA GPIO pin"},
    {"set_scl", cli_i2c_set_scl, "set_scl <IO_x>                        : Set the SCL GPIO pin"},
    {"set_addr", cli_i2c_set_addr, "set_addr <addr>                       : Set the master device address"},
    {"set_speed", cli_i2c_set_speed, "set_speed <standard|fast|fast_plus|high> : Set the I2C bus speed"},
    {"set_mode", cli_i2c_set_mode, "set_mode <master|slave>               : Set the I2C device mode"},
    {"read", cli_i2c_read, "read <slave_addr> <length>            : Read length bytes from slave_addr"},
    {"write", cli_i2c_write, "write <addr> <byte0> [byte1 ...]      : Write bytes to slave device"},
    {"scan", cli_i2c_scan, "scan                                  : Scan the I2C bus for connected devices"},
    {"probe", cli_i2c_probe, "probe <slave_addr>                    : Check if a device acknowledges at slave_addr"},
    {"", NULL, ""} // sentinel
};

typedef int8_t (*i2c_cmd_func_t)(void *, char *);

int8_t cli_i2c_register()
{
    LOG_INFO("Registring I2C interface for CLI");
    InterfaceRegistryEntry_t entry;
    entry.id = INTERFACE_I2C;
    strcpy(entry.name, "i2c");
    entry.init = cli_i2c_init;
    entry.de_init = cli_i2c_deinit;
    entry.cmd_handler = cli_i2c_cmd_dispatch;
    entry.help_handler = cli_i2c_help;
    int8_t reg_sts = interface_registry_register(&entry);
    if (reg_sts == 0)
    {
        LOG_INFO("CLI I2C registration success");
        return 0;
    }
    else
    {
        LOG_ERR("CLI I2C registration fail");
        return 1;
    }
}

int8_t cli_i2c_init(void **handle)
{
    // create a hanlde, and set default values
    cliI2CHandle_t i2c = (cliI2CHandle_t)malloc(sizeof(cliI2CConfig_t));
    if (i2c == NULL)
    {
        LOG_ERR("Failed to allocate memory for I2C handle");
        return KX_HAL_ERR_FAIL;
    }
    memset(i2c, 0, sizeof(cliI2CConfig_t));
    i2c->sda_pin = -1;                  // assuming -1 is an invalid pin
    i2c->scl_pin = -1;                  // assuming -1 is an invalid pin
    i2c->addr = 0x00;                   // default address
    i2c->speed = KX_I2C_SPEED_STANDARD; // default frequency 100kHz
    i2c->mode = KX_I2C_MODE_MASTER;     // default mode

    *handle = (void *)i2c;

    return KX_HAL_OK;
}

int8_t cli_i2c_deinit(void *handle)
{
    if (handle)
    {
        free(handle);
        return KX_HAL_OK;
    }
    return KX_HAL_ERR_FAIL; // invalid handle
}

void cli_i2c_help()
{
    printf("\n\r--- I2C Interface Commands ---\n\r");
    for (int i = 0; i2c_cmds[i].func != NULL; i++)
    {
        printf("  %s\n\r", i2c_cmds[i].help_str);
    }
    printf("--------------------------------\n\r");
}

int8_t cli_i2c_cmd_dispatch(void *handle, const char *cmd)
{
    if (handle == NULL)
    {
        return -1;
    }
    char key[32];
    char args[128];
    cmd_parse(cmd, key, 32, args, 128);
    int8_t cb_idx = cmd_tbl_search(key, i2c_cmds, sizeof(i2c_cmds) / sizeof(cmdEntry_t));
    if (cb_idx == -1)
    {
        LOG_ERR("\n\r Unknown I2C command \"%s\" not found", key);
        return -1;
    }

    ((i2c_cmd_func_t)i2c_cmds[cb_idx].func)(handle, args);
    return 0;
}

static int8_t is_digit(char ch)
{
    return (ch >= '0' && ch <= '9');
}

// this function will allocate a new I2C instance and return the handle to the caller
// if no instance is specified, it will allocate the first available instance
// the instance is mentioned as 0,1
int8_t cli_i2c_alloc_instance(void *handle, char *args)
{
    CHECK_HANDLE_NULL(handle);
    // check if args has something, if yes, then use it as the instance number
    cliI2CHandle_t i2c_handle = (cliI2CHandle_t)handle;
    int8_t instance = -1;
    if (args != NULL && strlen(args) > 0)
    {
        // get the string length and check if it is a number, if yes, then convert to int
        int len = strlen(args);
        for (int i = 0; i < len; i++)
        {
            if (!is_digit(args[i]))
            {
                LOG_ERR("Invalid instance number: %s", args);
                break;
            }
        }
        instance = atoi(args);
    }
    // call the hal function to allocate the instance
    Kx_ErrorCode err = i2c_alloc_instance(&i2c_handle->hal_handle, instance);
    if (err != KX_HAL_OK)
    {
        LOG_ERR("Failed to allocate I2C instance");
        return -1;
    }
    return KX_HAL_OK;
}

int8_t cli_i2c_set_sda(void *handle, char *args)
{
    CHECK_HANDLE_NULL(handle);
    cliI2CHandle_t i2c_handle = (cliI2CHandle_t)handle;

    i2c_handle->sda_pin = (Kx_IO)atoi(args);
    // call the hal function to set the sda pin from hal
    KxI2C_set_sda(i2c_handle->hal_handle, i2c_handle->sda_pin);
    return KX_HAL_OK;
}

int8_t cli_i2c_set_scl(void *handle, char *args)
{
    CHECK_HANDLE_NULL(handle);
    cliI2CHandle_t i2c_handle = (cliI2CHandle_t)handle;

    i2c_handle->scl_pin = (Kx_IO)atoi(args);
    // call the hal function to set the scl pin from hal
    KxI2C_set_scl(i2c_handle->hal_handle, i2c_handle->scl_pin);
    return KX_HAL_OK;
}

int8_t cli_i2c_set_addr(void *handle, char *args)
{
    CHECK_HANDLE_NULL(handle);
    cliI2CHandle_t i2c_handle = (cliI2CHandle_t)handle;

    i2c_handle->addr = (uint8_t)atoi(args);
    // call the hal function to set the master address from hal
    return KX_HAL_OK;
}

int8_t cli_i2c_set_mode(void *handle, char *args)
{
    CHECK_HANDLE_NULL(handle);
    cliI2CHandle_t i2c_handle = (cliI2CHandle_t)handle;

    // string compare to set the mode
    if (strcmp(args, "master") == 0)
    {
        i2c_handle->mode = KX_I2C_MODE_MASTER;
    }
    else if (strcmp(args, "slave") == 0)
    {
        i2c_handle->mode = KX_I2C_MODE_SLAVE;
    }
    else
    {
        LOG_ERR("Invalid mode. Use 'master' or 'slave'");
        return KX_HAL_ERR_INVALID_ARG;
    }
    // call the hal function to set the mode from hal
    Kx_ErrorCode status;
    status = KxI2C_set_device_mode(i2c_handle->hal_handle, i2c_handle->mode);
    if (status == KX_HAL_OK)
    {
        return KX_HAL_OK;
    }
    else if (status == KX_HAL_ERR_INVALID_ARG)
    {
        LOG_ERR("Something is wrong with the args");
    }

    return KX_HAL_ERR_FAIL;
}

int8_t cli_i2c_set_speed(void *handle, char *args)
{
    CHECK_HANDLE_NULL(handle);
    cliI2CHandle_t i2c_handle = (cliI2CHandle_t)handle;

    // string compare to set the speed
    if (strcmp(args, "standard") == 0)
    {
        i2c_handle->speed = KX_I2C_SPEED_STANDARD;
    }
    else if (strcmp(args, "fast") == 0)
    {
        i2c_handle->speed = KX_I2C_SPEED_FAST;
    }
    else if (strcmp(args, "fast_plus") == 0)
    {
        i2c_handle->speed = KX_I2C_SPEED_FAST_PLUS;
    }
    else if (strcmp(args, "high") == 0)
    {
        i2c_handle->speed = KX_I2C_SPEED_HIGH;
    }
    else
    {
        LOG_ERR("Invalid speed. Use 'standard', 'fast', 'fast_plus', or 'high'");
        return KX_HAL_ERR_INVALID_ARG;
    }
    // call the hal function to set the speed from hal
    KxI2C_set_speed(i2c_handle->hal_handle, i2c_handle->speed);
    return KX_HAL_OK;
}

int8_t cli_i2c_read(void *handle, char *args)
{
    uint8_t slave_addr;
    uint8_t length;
    uint8_t *buff;

    CHECK_HANDLE_NULL(handle);
    cliI2CHandle_t i2c_handle = (cliI2CHandle_t)handle;

    char *token = strtok(args, " ");
    slave_addr = (uint8_t)strtol(token, NULL, 0);
    token = strtok(NULL, " ");
    length = (uint8_t)strtol(token, NULL, 0);
    buff = (uint8_t *)malloc(length);
    printf("\n\rStarting read operation on slave address 0x%X for %d bytes", slave_addr, length);

    KxI2C_master_read(i2c_handle->hal_handle, slave_addr, buff, length);

    return KX_HAL_OK;
}

int8_t cli_i2c_write(void *handle, char *args)
{
    uint8_t data[128];
    size_t length = 0;
    // parse the args to get the data to write
    // sample args: "0x01 0x02 0x03" use i write 0x01 0x02 0x03
    CHECK_HANDLE_NULL(handle);
    cliI2CHandle_t i2c_handle = (cliI2CHandle_t)handle;
    char *token = strtok(args, " ");
    while (token != NULL && length < sizeof(data))
    {
        data[length++] = (uint8_t)strtol(token, NULL, 0);
        token = strtok(NULL, " ");
    }
    KxI2C_master_write(i2c_handle->hal_handle, data[0], &data[1], length - 1);
    return KX_HAL_OK;
}

int8_t cli_i2c_probe(void *handle, char *args)
{
    uint8_t slave_addr;
    CHECK_HANDLE_NULL(handle);
    cliI2CHandle_t i2c_handle = (cliI2CHandle_t)handle;
    slave_addr = (uint8_t)strtol(args, NULL, 0);
    LOG_INFO("Probing 0x%X", slave_addr);
    Kx_ErrorCode err = KxI2C_probe(i2c_handle->hal_handle, slave_addr);
    if (err == KX_HAL_ERR_TIMEOUT)
    {
        LOG_ERR("\n\rI2C Probe timeour");
    }
    else if (err == KX_HAL_OK)
    {
        printf("Found device at address 0x%02X\n", slave_addr);
    }
    else
    {
        LOG_ERR("No device found");
    }
    return KX_HAL_OK;
}

int8_t cli_i2c_scan(void *handle, char *args)
{
    CHECK_HANDLE_NULL(handle);
    cliI2CHandle_t i2c_handle = (cliI2CHandle_t)handle;
    printf("Scanning for I2C devices...\n");
    for (uint16_t addr = 0x00; addr <= 0x7E; addr++)
    {
        Kx_ErrorCode err = KxI2C_probe(i2c_handle->hal_handle, addr);
        if (err == KX_HAL_ERR_TIMEOUT)
        {
            LOG_ERR("\n\rI2C Probe timeour");
        }
        else if (err == KX_HAL_OK)
        {
            printf("Found device at address 0x%02X\n", addr);
        }
        // add a small delay to avoid bus congestion
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    printf("Scan complete.\n");
    return KX_HAL_OK;
}
