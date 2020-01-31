#include "platform.h"
#include "fingerprint.h"

void trigger_high(void) {
    platform_gpio_write(0, PLATFORM_GPIO_HIGH);
    platform_gpio_write(0, PLATFORM_GPIO_LOW);
    platform_gpio_write(0, PLATFORM_GPIO_HIGH);
    platform_gpio_write(0, PLATFORM_GPIO_LOW);
    platform_gpio_write(0, PLATFORM_GPIO_HIGH);
}

void trigger_low(void) {
    platform_gpio_write(0, PLATFORM_GPIO_LOW);
    platform_gpio_write(0, PLATFORM_GPIO_HIGH);
    platform_gpio_write(0, PLATFORM_GPIO_LOW);
    platform_gpio_write(0, PLATFORM_GPIO_HIGH);
    platform_gpio_write(0, PLATFORM_GPIO_LOW);
}
