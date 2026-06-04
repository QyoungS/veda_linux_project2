#include <stdio.h>
#include <wiringPi.h>
#include "light.h"
#include "led.h"

#define LIGHT_PIN 11

static int initialized = 0;

static void light_init(void)
{
    if (initialized) {
        return;
    }

    wiringPiSetupGpio();

    pinMode(LIGHT_PIN, INPUT);
    pullUpDnControl(LIGHT_PIN, PUD_UP);

    initialized = 1;
}

int light_read(void)
{
    int value;

    light_init();

    value = digitalRead(LIGHT_PIN);

    if (value == LOW) {
        /* 빛이 감지되면 LED OFF */
        led_off();
    } else {
        /* 빛이 감지되지 않으면 LED ON */
        led_on();
    }

    return value;
}
