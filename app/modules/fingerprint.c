#include "platform.h"
#include "fingerprint.h"

#define ONE asm volatile("nop");
#define TEN ONE ONE ONE ONE ONE ONE ONE ONE ONE ONE
#define HUN TEN TEN TEN TEN TEN TEN TEN TEN TEN TEN
#define THO HUN HUN HUN HUN HUN HUN HUN HUN HUN HUN
#define TTO THO THO THO THO THO THO THO THO THO THO

//#define SNE asm volatile("xor %0, %1, %2;" : :"r"(a), "r"(a), "r"(b));
#define SNE asm volatile("xor a4, a4, a5");
#define SEN SNE SNE SNE SNE SNE SNE SNE SNE SNE SNE
#define SUN SEN SEN SEN SEN SEN SEN SEN SEN SEN SEN
#define SHO SUN SUN SUN SUN SUN SUN SUN SUN SUN SUN
#define STO SHO SHO SHO SHO SHO SHO SHO SHO SHO SHO

#define WAVE_ITERS 4
#define WAVE_NOPS 128
#define WAVE_XORS 128

void wave(void) {
    /*
    for(int i = 0; i < WAVE_ITERS; i++) {
        // Nop train
        for(int j = 0; j < WAVE_NOPS; j++) {
            HUN;
        }

        // Setup registers
        asm volatile("xor a4, a4, a4" ::: "a4");
        asm volatile("xor a5, a5, a5" ::: "a5");
        asm volatile("movi.n a5, -0x1" ::: "a5");

        // XOR train
        for(int j = 0; j < WAVE_XORS; j++) {
            SUN;
        }

        system_soft_wdt_feed(); // Reset watchdog
    }*/

    /*
    THO;
    THO;
    THO; // peak

    THO;
    THO;
    THO;

    THO;
    THO;
    THO;

    THO;
    THO;
    THO;

    THO;
    THO;
    THO;
    */

    //TTO;
    //TTO;
}

void trigger_high(void) {
    platform_gpio_write(0, PLATFORM_GPIO_HIGH);
    platform_gpio_write(0, PLATFORM_GPIO_LOW);
    platform_gpio_write(0, PLATFORM_GPIO_HIGH);
    platform_gpio_write(0, PLATFORM_GPIO_LOW);
    platform_gpio_write(0, PLATFORM_GPIO_HIGH);
    //wave();
    //platform_gpio_write(0, PLATFORM_GPIO_HIGH);
}

void trigger_low(void) {
    platform_gpio_write(0, PLATFORM_GPIO_LOW);
    platform_gpio_write(0, PLATFORM_GPIO_HIGH);
    platform_gpio_write(0, PLATFORM_GPIO_LOW);
    platform_gpio_write(0, PLATFORM_GPIO_HIGH);
    platform_gpio_write(0, PLATFORM_GPIO_LOW);
    //platform_gpio_write(0, PLATFORM_GPIO_LOW);
    //wave();
}
