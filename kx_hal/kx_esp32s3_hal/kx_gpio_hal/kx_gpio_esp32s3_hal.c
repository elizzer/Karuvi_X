#include "kx_gpio_hal.h"
#include "driver/gpio.h"
#include "hal/gpio_ll.h"
#include "esp_attr.h"
#include "esp_intr_alloc.h"
#include "soc/interrupts.h"

gpio_intr_cb_fp app_intr_cb = NULL;

// --- internal helpers: map Kx enums to ESP-IDF enums ---

static gpio_mode_t Kx_ToEspGpioMode(Kx_GpioDirection_t direction)
{
    switch (direction)
    {
    case KX_GPIO_DIR_INPUT:
        return GPIO_MODE_INPUT;
    case KX_GPIO_DIR_OUTPUT:
        return GPIO_MODE_OUTPUT;
    case KX_GPIO_DIR_INPUT_OUTPUT:
        return GPIO_MODE_INPUT_OUTPUT_OD;
    default:
        return GPIO_MODE_DISABLE;
    }
    return GPIO_MODE_DISABLE;
}

IRAM_ATTR void gpio_isr_handler(void *arg)
{
    (void)arg;
    if (app_intr_cb == NULL)
    {
        return;
    }
    gpio_dev_t *dev = GPIO_LL_GET_HW(0);
    uint32_t status_lo;
    gpio_ll_get_intr_status(dev, 0, &status_lo);
    uint32_t status_hi;
    gpio_ll_get_intr_status_high(dev, 0, &status_hi);

    gpio_ll_clear_intr_status(dev, status_lo);
    gpio_ll_clear_intr_status_high(dev, status_hi);

    TickType_t now = xTaskGetTickCountFromISR();

    for (int i = 0; i < 32; i++)
    {
        if (status_lo & (1u << i))
        {
            Kx_GpioEvent_t t;
            if (KxGpio_get_intr(i, &t) == KX_HAL_OK &&
                (t == KX_GPIO_INTR_HIGH_LEVEL || t == KX_GPIO_INTR_LOW_LEVEL))
            {
                gpio_ll_intr_disable(dev, i);  // disarm before it can re-fire
            }
            app_intr_cb(i, now);
        }
    }
    for (int i = 0; i < GPIO_PIN_COUNT - 32; i++)
    {
        if (status_hi & (1u << i))
        {
            int pin = i + 32;
            Kx_GpioEvent_t t;
            if (KxGpio_get_intr(pin, &t) == KX_HAL_OK &&
                (t == KX_GPIO_INTR_HIGH_LEVEL || t == KX_GPIO_INTR_LOW_LEVEL))
            {
                gpio_ll_intr_disable(dev, pin);
            }
            app_intr_cb(pin, now);
        }
    }
}

Kx_ErrorCode KxGpio_Init(void)
{
    intr_handle_t handle;
    esp_intr_alloc(ETS_GPIO_INTR_SOURCE,
                   ESP_INTR_FLAG_IRAM,
                   gpio_isr_handler,
                   NULL,
                   &handle);
    return KX_HAL_OK;
}

Kx_ErrorCode KxGpio_DeInit(void)
{
    return KX_HAL_OK;
}

Kx_ErrorCode KxGpio_SetDirection(Kx_GpioPin pin, Kx_GpioDirection_t direction)
{
    esp_err_t err = gpio_set_direction((gpio_num_t)pin, Kx_ToEspGpioMode(direction));
    if (err != ESP_OK)
    {
        return KX_HAL_ERR_FAIL;
    }
    return KX_HAL_OK;
}

Kx_ErrorCode KxGpio_SetPull(Kx_GpioPin pin, Kx_GpioPull_t pull)
{
    esp_err_t err = ESP_OK;

    switch (pull)
    {
    case KX_GPIO_PULL_NONE:
        err = gpio_set_pull_mode((gpio_num_t)pin, GPIO_FLOATING);
        break;
    case KX_GPIO_PULL_UP:
        err = gpio_set_pull_mode((gpio_num_t)pin, GPIO_PULLUP_ONLY);
        break;
    case KX_GPIO_PULL_DOWN:
        err = gpio_set_pull_mode((gpio_num_t)pin, GPIO_PULLDOWN_ONLY);
        break;
    case KX_GPIO_PULL_UP_DOWN:
        err = gpio_set_pull_mode((gpio_num_t)pin, GPIO_PULLUP_PULLDOWN);
        break;
    default:
        return KX_HAL_ERR_INVALID_ARG;
    }

    if (err != ESP_OK)
    {
        return KX_HAL_ERR_FAIL;
    }
    return KX_HAL_OK;
}

Kx_ErrorCode KxGpio_Set(Kx_GpioPin pin)
{
    esp_err_t err = gpio_set_level((gpio_num_t)pin, 1);
    if (err != ESP_OK)
    {
        return KX_HAL_ERR_FAIL;
    }
    return KX_HAL_OK;
}

Kx_ErrorCode KxGpio_Clear(Kx_GpioPin pin)
{
    esp_err_t err = gpio_set_level((gpio_num_t)pin, 0);
    if (err != ESP_OK)
    {
        return KX_HAL_ERR_FAIL;
    }
    return KX_HAL_OK;
}
Kx_ErrorCode KxGpio_Read(Kx_GpioPin pin, Kx_GpioState_t *state)
{
    if (state == NULL)
    {
        return KX_HAL_ERR_INVALID_ARG;
    }

    int level = gpio_get_level((gpio_num_t)pin);
    *state = level ? KX_GPIO_STATE_HIGH : KX_GPIO_STATE_LOW;

    return KX_HAL_OK;
}

Kx_ErrorCode KxGpio_set_intr(Kx_GpioPin pin, Kx_GpioEvent_t trig_type)
{
    // enable the intrupr for the pin

    // validate the trig_type
    if (trig_type >= KX_GPIO_INTR_MAX)
    {
        return KX_HAL_ERR_INVALID_ARG;
    }

    gpio_dev_t *dev = GPIO_LL_GET_HW(0);
    gpio_ll_set_intr_type(dev, pin, trig_type);
    gpio_ll_intr_enable_on_core(dev, 0, pin);

    return KX_HAL_OK;
}

Kx_ErrorCode KxGpio_clear_intr(Kx_GpioPin pin)
{
    gpio_dev_t *dev = GPIO_LL_GET_HW(0);
    gpio_ll_set_intr_type(dev, pin, KX_GPIO_INTR_DISABLE);
    gpio_ll_intr_disable(dev, pin);

    return KX_HAL_OK;
}

Kx_ErrorCode KxGpio_get_intr(Kx_GpioPin pin, Kx_GpioEvent_t *trig_type)
{
    gpio_dev_t *dev = GPIO_LL_GET_HW(0);
    *trig_type = dev->pin[pin].int_type;
    return KX_HAL_OK;
}

Kx_ErrorCode KxGpio_register_intr_cb(gpio_intr_cb_fp cb)
{
    if (cb == NULL)
    {
        return KX_HAL_ERR_INVALID_ARG;
    }
    app_intr_cb = cb;
    return KX_HAL_OK;
}