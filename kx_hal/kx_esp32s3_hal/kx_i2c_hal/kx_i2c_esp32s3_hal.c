#include "kx_i2c_hal.h"

#include "soc/i2c_reg.h"
#include "soc/i2c_struct.h"
#include "soc/gpio_struct.h"
#include "hal/i2c_ll.h"
#include "hal/gpio_ll.h"
#include "soc/gpio_sig_map.h"
#include "esp_private/periph_ctrl.h"
#include "esp_rom_gpio.h"
#include <stdio.h>
#include "esp_timer.h"
#include <stdint.h>

#define FI2C_SCLK 40000000U

// globale variable to hold the instance
static KxI2C_ChannelConfig_t g_i2c_instances[KX_I2C_INSTANCE_MAX] = {0};

typedef struct
{
    uint8_t sda_in;
    uint8_t scl_in;
    uint8_t sda_out;
    uint8_t scl_out;

} KxI2C_Signal;

typedef union
{
    struct
    {
        uint32_t byte_num : 8;
        uint32_t ack_en : 1;
        uint32_t ack_exp : 1;
        uint32_t ack_val : 1;
        uint32_t op_code : 3;
        uint32_t reserved14 : 17;
        uint32_t done : 1;
    };

    uint32_t val;

} kx_i2c_hal_command_reg_t;

static KxI2C_Signal i2c_signal_map[KX_I2C_INSTANCE_MAX] = {
    {
        .scl_in = I2CEXT0_SCL_IN_IDX,
        .scl_out = I2CEXT0_SCL_OUT_IDX,
        .sda_in = I2CEXT0_SDA_IN_IDX,
        .sda_out = I2CEXT0_SDA_OUT_IDX,
    },
    {
        .scl_in = I2CEXT1_SCL_IN_IDX,
        .scl_out = I2CEXT1_SCL_OUT_IDX,
        .sda_in = I2CEXT1_SDA_IN_IDX,
        .sda_out = I2CEXT1_SDA_OUT_IDX,
    },
};

Kx_ErrorCode kx_i2c_init()
{
    // init the clock for the i2c peripheral
    // init values for the global state array
    for (uint8_t t_idx = 0; t_idx < KX_I2C_INSTANCE_MAX; t_idx++)
    {
        g_i2c_instances[t_idx].is_initialized = false;
        g_i2c_instances[t_idx].is_used = false;
        g_i2c_instances[t_idx].instance = t_idx;
    }
    return KX_HAL_OK;
}
Kx_ErrorCode kx_i2c_deinit()
{
    return KX_HAL_OK;
}

Kx_ErrorCode i2c_alloc_instance(KxI2C_Handle_t *handle, int8_t instance)
{

    // validate the handle not null
    if (handle == NULL)
    {
        return KX_HAL_ERR_INVALID_ARG;
    }

    // loop and get an free instance
    uint8_t free_idx = KX_I2C_INSTANCE_MAX;
    if (instance >= 0 && instance < KX_I2C_INSTANCE_MAX)
    {
        printf("Allocating I2C instance %d\n", instance);
        if (g_i2c_instances[instance].is_used == false)
        {
            free_idx = instance;
        }
        else
        {
            return KX_HAL_ERR_BUSY;
        }
    }
    else
    {

        for (uint8_t s_idx = 0; s_idx < KX_I2C_INSTANCE_MAX; s_idx++)
        {
            if (g_i2c_instances[s_idx].is_used == false)
            {
                free_idx = s_idx;
                break;
            }
        }
        if (free_idx == KX_I2C_INSTANCE_MAX)
        {
            return KX_HAL_ERR_FAIL;
        }
    }

    i2c_port_t t_port = (i2c_port_t)g_i2c_instances[free_idx].instance;

    PERIPH_RCC_ATOMIC()
    {
        i2c_ll_enable_bus_clock(t_port, true);
        i2c_ll_reset_register(t_port);
    }

    i2c_dev_t *t_dev = I2C_LL_GET_HW(t_port);
    i2c_ll_set_source_clk(t_dev, I2C_CLK_SRC_XTAL);
    i2c_ll_enable_controller_clock(t_dev, true);

    g_i2c_instances[free_idx].is_used = true;
    g_i2c_instances[free_idx].is_initialized = true;

    *handle = &g_i2c_instances[free_idx];

    return KX_HAL_OK;
}

Kx_ErrorCode KxI2C_free_instance(KxI2C_Handle_t handle)
{
    if (handle == NULL)
    {
        return KX_HAL_ERR_INVALID_ARG;
    }
    return KX_HAL_OK;
}

Kx_ErrorCode KxI2C_set_device_mode(KxI2C_Handle_t handle, KxI2C_Mode_t mode)
{

    if (handle == NULL)
    {
        return KX_HAL_ERR_INVALID_ARG;
    }

    printf("Setting I2C device mode to %s\n", KX_I2C_MODE_MASTER == mode ? "master" : "slave");
    if (KX_I2C_MODE_MASTER == mode)
    {
        i2c_ll_master_init(I2C_LL_GET_HW(handle->instance));
    }
    else if (KX_I2C_MODE_SLAVE == mode)
    {
        i2c_ll_slave_init(I2C_LL_GET_HW(handle->instance));
    }

    i2c_ll_update(I2C_LL_GET_HW(handle->instance));

    return KX_HAL_OK;
}

Kx_ErrorCode KxI2C_set_speed(KxI2C_Handle_t handle, KxI2C_Speed_t speed)
{
    if (handle == NULL)
    {
        return KX_HAL_ERR_INVALID_ARG;
    }
    i2c_dev_t *t_dev = I2C_LL_GET_HW(handle->instance);

    i2c_hal_clk_config_t clk_cfg = {0};
    uint32_t sclk = FI2C_SCLK;
    uint32_t bus_freq = 0;
    uint32_t clkm_div = 1;
    // timeout is not enabled for now
    if (KX_I2C_SPEED_STANDARD == speed)
    {
        printf("Setting I2C speed to standard (100 kHz)\n");
        bus_freq = 100000;
        clkm_div = (sclk / (bus_freq * 1024)) + 1;
    }
    else if (KX_I2C_SPEED_FAST == speed)
    {
        printf("Setting I2C speed to fast (400 kHz)\n");
        bus_freq = 400000;
        clkm_div = (sclk / (bus_freq * 1024)) + 1;
    }
    else if (KX_I2C_SPEED_FAST_PLUS == speed)
    {
        printf("Setting I2C speed to fast plus (1 MHz)\n");
        bus_freq = 1000000;
        clkm_div = (sclk / (bus_freq * 1024)) + 1;
    }
    else if (KX_I2C_SPEED_HIGH == speed)
    {
        printf("Setting I2C speed to high (3.4 MHz)\n");
        bus_freq = 3400000;
        clkm_div = (sclk / (bus_freq * 1024)) + 1;
    }
    else
    {
        return KX_HAL_ERR_INVALID_ARG;
    }
    uint32_t half_cycle = (sclk / bus_freq) / 2;
    if (KX_I2C_MODE_MASTER == handle->device_mode)
    {

        clk_cfg.clkm_div = clkm_div;
        clk_cfg.scl_low = half_cycle;
        clk_cfg.scl_wait_high = half_cycle / 4;
        clk_cfg.scl_high = half_cycle * 0.75;
        clk_cfg.setup = half_cycle;
        clk_cfg.hold = half_cycle;
        clk_cfg.sda_hold = half_cycle / 4;
        clk_cfg.sda_sample = half_cycle / 2;
        clk_cfg.tout = 10;

        printf("I2C Clock Config:\n"
               "  clkm_div    = %u\n"
               "  scl_low     = %u\n"
               "  scl_wait_high = %u\n"
               "  scl_high    = %u\n"
               "  setup       = %u\n"
               "  hold        = %u\n"
               "  sda_hold    = %u\n"
               "  sda_sample  = %u\n"
               "  tout        = %u\n",
               (unsigned)clk_cfg.clkm_div,
               (unsigned)clk_cfg.scl_low,
               (unsigned)clk_cfg.scl_wait_high,
               (unsigned)clk_cfg.scl_high,
               (unsigned)clk_cfg.setup,
               (unsigned)clk_cfg.hold,
               (unsigned)clk_cfg.sda_hold,
               (unsigned)clk_cfg.sda_sample,
               (unsigned)clk_cfg.tout);
    }
    else if (KX_I2C_MODE_SLAVE == handle->device_mode)
    {
        clk_cfg.sda_hold = half_cycle / 4;
        clk_cfg.sda_sample = half_cycle / 2;
    }
    else
    {
        return KX_HAL_ERR_FAIL;
    }
    i2c_ll_master_set_bus_timing(t_dev, &clk_cfg);
    return KX_HAL_OK;
}

Kx_ErrorCode KxI2C_set_sda(KxI2C_Handle_t handle, Kx_IO sda)
{

    if (handle == NULL)
    {
        return KX_HAL_ERR_INVALID_ARG;
    }

    gpio_dev_t *gpio = GPIO_LL_GET_HW(0);
    // set the pin to gpio
    gpio_ll_func_sel(gpio, sda, PIN_FUNC_GPIO);
    gpio_ll_output_enable(gpio, sda);
    gpio_ll_od_enable(gpio, sda);

    gpio_ll_input_enable(gpio, sda);
    gpio_ll_pullup_en(gpio, sda);
    gpio_ll_pulldown_dis(gpio, sda); // make sure pulldown isn't also on

    // map the i2c SDA in and out signals
    esp_rom_gpio_connect_in_signal(sda, i2c_signal_map[handle->instance].sda_in, false);
    esp_rom_gpio_connect_out_signal(sda, i2c_signal_map[handle->instance].sda_out, false, false);
    return KX_HAL_OK;
}

Kx_ErrorCode KxI2C_set_scl(KxI2C_Handle_t handle, Kx_IO scl)
{
    if (handle == NULL)
    {
        return KX_HAL_ERR_INVALID_ARG;
    }

    gpio_dev_t *gpio = GPIO_LL_GET_HW(0);
    // set the pin to gpio
    gpio_ll_func_sel(gpio, scl, PIN_FUNC_GPIO);
    gpio_ll_output_enable(gpio, scl);
    gpio_ll_od_enable(gpio, scl);

    gpio_ll_input_enable(gpio, scl);
    gpio_ll_pullup_en(gpio, scl);
    gpio_ll_pulldown_dis(gpio, scl); // make sure pulldown isn't also on

    // map the i2c scl in and out signals
    esp_rom_gpio_connect_in_signal(scl, i2c_signal_map[handle->instance].scl_in, false);
    esp_rom_gpio_connect_out_signal(scl, i2c_signal_map[handle->instance].scl_out, false, false);
    return KX_HAL_OK;
}

Kx_ErrorCode KxI2C_set_slave_addr(KxI2C_Handle_t handle, KxI2C_AddrMode_t addr_mode, uint16_t addr)
{
    if (handle == NULL)
    {
        return KX_HAL_ERR_INVALID_ARG;
    }
    return KX_HAL_OK;
}

Kx_ErrorCode KxI2C_probe(KxI2C_Handle_t handle, uint16_t addr)
{
    if (handle == NULL)
    {
        return KX_HAL_ERR_INVALID_ARG;
    }
    // send a start condition, send the address with write bit, and check for ack
    i2c_dev_t *t_dev = I2C_LL_GET_HW(handle->instance);

    i2c_ll_txfifo_rst(t_dev);
    t_dev->fifo_conf.nonfifo_en = 1;
    t_dev->txfifo_mem[0] = ((uint8_t)addr << 1) & 0xFE; // write address with write bit
    kx_i2c_hal_command_reg_t cmd;

    cmd.val = 0;
    cmd.op_code = 6; //
    t_dev->comd[0].val = cmd.val;

    // write
    cmd.val = 0;
    cmd.op_code = 1;
    cmd.ack_en = 1;
    cmd.ack_exp = 0; // expect ack
    cmd.byte_num = 1;
    t_dev->comd[1].val = cmd.val;

    // stop
    cmd.val = 0;
    cmd.op_code = 2;
    t_dev->comd[2].val = cmd.val;

    i2c_ll_enable_intr_mask(t_dev, I2C_LL_MASTER_EVENT_INTR);
    i2c_ll_clear_intr_mask(t_dev, I2C_LL_MASTER_EVENT_INTR);

    i2c_ll_update(t_dev);
    // print the intr enable register
    i2c_ll_master_trans_start(t_dev);

    int32_t start_us = esp_timer_get_time();
    const int32_t timeout_us = 10000; // 10ms is generous for a single-byte transfer at 100kHz

    while (!(t_dev->int_raw.trans_complete_int_raw || t_dev->int_raw.nack_int_raw))
    {
        if (esp_timer_get_time() - start_us > timeout_us)
        {
            t_dev->int_clr.val = t_dev->int_raw.val;
            return KX_HAL_ERR_TIMEOUT; // add this error code if you don't have one
        }
    }

    if (t_dev->int_raw.nack_int_raw)
    {
        uint32_t raw = t_dev->int_raw.val;

        t_dev->int_clr.val = t_dev->int_raw.val;
        return KX_HAL_ERR_FAIL;
    }

    t_dev->int_clr.val = t_dev->int_raw.val;
    i2c_ll_disable_intr_mask(t_dev, I2C_LL_MASTER_EVENT_INTR);
    i2c_ll_clear_intr_mask(t_dev, I2C_LL_MASTER_EVENT_INTR);
    return KX_HAL_OK;
}

Kx_ErrorCode KxI2C_master_read(KxI2C_Handle_t handle, uint16_t s_addr, uint8_t *data, size_t length)
{
    if (handle == NULL)
    {
        return KX_HAL_ERR_INVALID_ARG;
    }
    i2c_dev_t *t_dev = I2C_LL_GET_HW(handle->instance);

    // reset the fifo before reading

    t_dev->fifo_conf.rx_fifo_rst = 1;
    t_dev->fifo_conf.rx_fifo_rst = 0;
    t_dev->fifo_conf.tx_fifo_rst = 1;
    t_dev->fifo_conf.tx_fifo_rst = 0;

    uint8_t addr_byte = s_addr << 1;
    t_dev->fifo_conf.nonfifo_en = 0;

    i2c_ll_write_txfifo(t_dev, &addr_byte, 1);

    // now fill the command registers with the write commands
    kx_i2c_hal_command_reg_t cmd;

    cmd.val = 0;
    cmd.op_code = 6; //
    t_dev->comd[0].val = cmd.val;

    // write
    cmd.val = 0;
    cmd.op_code = 1;
    cmd.byte_num = 1;
    cmd.ack_en = 1;
    cmd.ack_exp = 0;
    t_dev->comd[1].val = cmd.val;

    // read n byte
    cmd.val = 0;
    cmd.op_code = 3;
    cmd.byte_num = length;
    t_dev->comd[2].val = cmd.val;

    // stop
    cmd.val = 0;
    cmd.op_code = 2;
    t_dev->comd[3].val = cmd.val;

    i2c_ll_enable_intr_mask(t_dev, I2C_LL_MASTER_EVENT_INTR);
    i2c_ll_clear_intr_mask(t_dev, I2C_LL_MASTER_EVENT_INTR);

    i2c_ll_update(t_dev);

    i2c_ll_master_trans_start(t_dev);

    // wait for transfer to complete
    int32_t start_us = esp_timer_get_time();
    const int32_t timeout_us = 10000; // 10ms is generous for a single-byte transfer at 100kHz

    while (!(t_dev->int_raw.trans_complete_int_raw || t_dev->int_raw.nack_int_raw))
    {
        if (esp_timer_get_time() - start_us > timeout_us)
        {
            t_dev->int_clr.val = t_dev->int_raw.val;
            printf("\n\rRead operation filed, transfer timeout");
            return KX_HAL_ERR_TIMEOUT; // add this error code if you don't have one
        }
    }

    if (t_dev->int_raw.nack_int_raw)
    {
        t_dev->int_clr.val = t_dev->int_raw.val;
        printf("\n\rTransfer fail received NACK");

        // loop throught all commands and check their status
        for (uint8_t i = 0; i < 4; i++)
        {
            printf("\n\rCommand %d status: %d", i, t_dev->comd->command_done ? 1 : 0);
        }
        return KX_HAL_ERR_FAIL;
    }

    t_dev->int_clr.val = t_dev->int_raw.val;
    i2c_ll_disable_intr_mask(t_dev, I2C_LL_MASTER_EVENT_INTR);
    i2c_ll_clear_intr_mask(t_dev, I2C_LL_MASTER_EVENT_INTR);
    printf("\n\rI2C Read completed");

    i2c_ll_read_rxfifo(t_dev, data, length);

    for (uint8_t i = 0; i < length; i++)
    {
        printf("\n\r0x%x", data[i]);
    }

    return KX_HAL_OK;
}
Kx_ErrorCode KxI2C_master_write(KxI2C_Handle_t handle, uint16_t s_addr, uint8_t *data, size_t length)
{
    if (handle == NULL)
    {
        return KX_HAL_ERR_INVALID_ARG;
    }
    // put the data into the fifo and start the transfer
    i2c_dev_t *t_dev = I2C_LL_GET_HW(handle->instance);

    // cleare the fifo before writing
    t_dev->fifo_conf.tx_fifo_rst = 1;
    t_dev->fifo_conf.tx_fifo_rst = 0;
    // for every write the data is written from the start
    uint8_t addr_byte = s_addr << 1;
    t_dev->fifo_conf.nonfifo_en = 0;

    i2c_ll_write_txfifo(t_dev, &addr_byte, 1);
    i2c_ll_write_txfifo(t_dev, data, length);

    printf("\n\rTX RAM is ready");
    // now fill the command registers with the write commands
    kx_i2c_hal_command_reg_t cmd;

    cmd.val = 0;
    cmd.op_code = 6; //
    t_dev->comd[0].val = cmd.val;

    length = length + 1;
    // write
    cmd.val = 0;
    cmd.op_code = 1;
    cmd.byte_num = length > 32 ? 32 : length;
    t_dev->comd[1].val = cmd.val;

    // stop
    cmd.val = 0;
    cmd.op_code = 2;
    t_dev->comd[2].val = cmd.val;

    i2c_ll_enable_intr_mask(t_dev, I2C_LL_MASTER_EVENT_INTR);
    i2c_ll_clear_intr_mask(t_dev, I2C_LL_MASTER_EVENT_INTR);

    i2c_ll_update(t_dev);

    printf("\n\rStarting I2C write transfer to slave address 0x%02X with %d bytes", s_addr, length);
    i2c_ll_master_trans_start(t_dev);

    // wait for transfer to complete
    int32_t start_us = esp_timer_get_time();
    const int32_t timeout_us = 10000; // 10ms is generous for a single-byte transfer at 100kHz

    while (!(t_dev->int_raw.trans_complete_int_raw || t_dev->int_raw.nack_int_raw))
    {
        if (esp_timer_get_time() - start_us > timeout_us)
        {
            t_dev->int_clr.val = t_dev->int_raw.val;
            printf("\n\rRead operation filed, transfer timeout");
            return KX_HAL_ERR_TIMEOUT; // add this error code if you don't have one
        }
    }

    if (t_dev->int_raw.nack_int_raw)
    {
        t_dev->int_clr.val = t_dev->int_raw.val;
        printf("\n\rTransfer fail received NACK");
        return KX_HAL_ERR_FAIL;
    }

    t_dev->int_clr.val = t_dev->int_raw.val;
    i2c_ll_disable_intr_mask(t_dev, I2C_LL_MASTER_EVENT_INTR);
    i2c_ll_clear_intr_mask(t_dev, I2C_LL_MASTER_EVENT_INTR);

    return KX_HAL_OK;
}
