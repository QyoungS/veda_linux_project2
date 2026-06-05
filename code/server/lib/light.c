#include <stdio.h>
#include <wiringPi.h>
#include "light.h"
#include "led.h"

#define LIGHT_PIN 11

static int initialized = 0;

static void light_init(void) /* Initialize the light sensor pin once. */
{
    if (initialized) {
        return;
    }

    wiringPiSetupGpio();

    pinMode(LIGHT_PIN, INPUT);
    pullUpDnControl(LIGHT_PIN, PUD_UP);

    initialized = 1;
}

int light_read(void) /* Read light value and update LED automatically. */
{
    int value;

    light_init();

    value = digitalRead(LIGHT_PIN);

    if (value == LOW) {
        /* Turn LED off when light is detected. */
        led_off();
    } else {
        /* Turn LED on when light is not detected. */
        led_on();
    }

    return value;
}
