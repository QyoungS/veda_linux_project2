#include <stdio.h>
#include <wiringPi.h>
#include "device.h"
#include "device_log.h"

#define LIGHT_PIN 11

static int initialized = 0;

static void light_init(void)
{
    if (initialized) {
        return;
    }

    DEVICE_LOG("[device] LIGHT init: sensor_pin=%d\n", LIGHT_PIN);

    wiringPiSetupGpio();

    pinMode(LIGHT_PIN, INPUT);
    pullUpDnControl(LIGHT_PIN, PUD_UP);

    initialized = 1;
}

int light_read(void)
{
    int value;

    DEVICE_LOG("[device] LIGHT command: read\n");

    light_init();

    value = digitalRead(LIGHT_PIN);
    DEVICE_LOG("[device] LIGHT value: %d\n", value);

    if (value == LOW) {
        /* 빛이 감지되면 LED OFF */
        led_off();
        DEVICE_LOG("[device] light detected: LED OFF\n");
    } else {
        /* 빛이 감지되지 않으면 LED ON */
        led_on();
        DEVICE_LOG("[device] light not detected: LED ON\n");
    }

    return value;
}
