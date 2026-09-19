
#ifndef KX_GPIO_HAL_H
#define KX_GPIO_HAL_H

#include <stdint.h>
#include "kx_hal_types.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef uint8_t Kx_GpioPin;

// Pin direction
typedef enum Kx_GpioDirection {
    KX_GPIO_DIR_INPUT = 0,
    KX_GPIO_DIR_OUTPUT,
    KX_GPIO_DIR_INPUT_OUTPUT,   // open-drain style bidirectional
} Kx_GpioDirection_t;

// Pin pull configuration
typedef enum Kx_GpioPull {
    KX_GPIO_PULL_NONE = 0,
    KX_GPIO_PULL_UP,
    KX_GPIO_PULL_DOWN,
    KX_GPIO_PULL_UP_DOWN,       // both enabled, if HW supports it
} Kx_GpioPull_t;

// Pin logic state
typedef enum Kx_GpioState {
    KX_GPIO_STATE_LOW = 0,
    KX_GPIO_STATE_HIGH,
} Kx_GpioState_t;

// Pin operation (for interrupt/edge config, or read-vs-write intent)
typedef enum Kx_GpioOperation {
    KX_GPIO_OP_READ = 0,
    KX_GPIO_OP_WRITE,
    KX_GPIO_OP_TOGGLE,
} Kx_GpioOperation_t;


typedef enum {
    KX_GPIO_INTR_DISABLE = 0,     /*!< Disable GPIO interrupt                             */
    KX_GPIO_INTR_POSEDGE = 1,     /*!< GPIO interrupt type : rising edge                  */
    KX_GPIO_INTR_NEGEDGE = 2,     /*!< GPIO interrupt type : falling edge                 */
    KX_GPIO_INTR_ANYEDGE = 3,     /*!< GPIO interrupt type : both rising and falling edge */
    KX_GPIO_INTR_LOW_LEVEL = 4,   /*!< GPIO interrupt type : input low level trigger      */
    KX_GPIO_INTR_HIGH_LEVEL = 5,  /*!< GPIO interrupt type : input high level trigger     */
    KX_GPIO_INTR_MAX,
} Kx_GpioEvent_t;

typedef void (*gpio_intr_cb_fp)(uint32_t pin,TickType_t time); 

typedef struct{
    gpio_intr_cb_fp app_intr_cb;
} kx_GpioConfig_t;

Kx_ErrorCode KxGpio_Init();
Kx_ErrorCode KxGpio_DeInit();
Kx_ErrorCode KxGpio_register_intr_cb(gpio_intr_cb_fp);
Kx_ErrorCode KxGpio_SetDirection(Kx_GpioPin,Kx_GpioDirection_t);
Kx_ErrorCode KxGpio_SetPull(Kx_GpioPin,Kx_GpioPull_t);
Kx_ErrorCode KxGpio_Set(Kx_GpioPin);
Kx_ErrorCode KxGpio_Clear(Kx_GpioPin);
Kx_ErrorCode KxGpio_Read(Kx_GpioPin, Kx_GpioState_t*);
Kx_ErrorCode KxGpio_clear_intr(Kx_GpioPin pin);
Kx_ErrorCode KxGpio_set_intr(Kx_GpioPin pin, Kx_GpioEvent_t trig_type);
Kx_ErrorCode KxGpio_get_intr(Kx_GpioPin pin, Kx_GpioEvent_t *trig_type);

//register intrupt
//enable intrrupt
//disable intrrupt
//un reigstser intrrupt



#endif //KX_GPIO_HAL_H
